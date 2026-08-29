// RunQC.C — first-look quality checks for a T10 run.
//
// Answers two questions per channel:
//   1. Are the SiPM bias voltages sensible?  (baseline mean/RMS, signal
//      amplitude spectrum, fraction of events saturating the ADC range)
//   2. Does the timing trigger capture the full signal?  (distribution of
//      pulse-peak sample within the 1024-sample record; pulses piling up at
//      either edge mean the trigger delay / post-trigger fraction is wrong)
//
// Usage:  root -l -b -q 'macros/RunQC.C+(12)'
// Reads  data/run_<N>.root, writes Output/run_<N>/{RunQC.root, RunQC.pdf,
// RunQC_summary.txt}.
//
// Waveforms are channel[18][1024]. Pulse polarity is auto-detected per
// channel from the mean extremum relative to baseline. The baseline window
// is the first 200 samples: with post_trigger = 70% the trigger sits near
// sample ~307, so samples 0-199 are pre-signal.

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLatex.h"
#include "TSystem.h"
#include "TMath.h"
#include <cstdio>
#include <cmath>
#include <vector>

static const int NCH   = 18;
static const int NSAMP = 1024;
static const int BASE_END = 200;    // baseline window: samples [0, BASE_END)
static const int EDGE  = 30;        // "at the edge" margin for peak-time check
static const float SAT_LO = 5.0f;   // counts from rail counted as saturated
static const float ADC_MAX = 4095.0f;   // 12-bit digitizer (baselines sit at ~2048)

void RunQC(int run = 12)
{
  gStyle->SetOptStat(0);
  TString dataFile = TString::Format("data/run_%d.root", run);
  TString outDir   = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);

  TFile *fin = TFile::Open(dataFile);
  if (!fin || fin->IsZombie()) { printf("Cannot open %s\n", dataFile.Data()); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  if (!t) { printf("No 'pulse' tree in %s\n", dataFile.Data()); return; }

  static float ch[NCH][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t nEnt = t->GetEntries();

  // First pass over a subsample to detect polarity per channel.
  double sumPos[NCH] = {0}, sumNeg[NCH] = {0};
  const Long64_t nPol = TMath::Min((Long64_t)500, nEnt);
  for (Long64_t i = 0; i < nPol; ++i) {
    t->GetEntry(i);
    for (int c = 0; c < NCH; ++c) {
      double base = 0;
      for (int s = 0; s < BASE_END; ++s) base += ch[c][s];
      base /= BASE_END;
      float mx = -1e30f, mn = 1e30f;
      for (int s = 0; s < NSAMP; ++s) { if (ch[c][s] > mx) mx = ch[c][s]; if (ch[c][s] < mn) mn = ch[c][s]; }
      sumPos[c] += mx - base;
      sumNeg[c] += base - mn;
    }
  }
  int pol[NCH];
  for (int c = 0; c < NCH; ++c) pol[c] = (sumPos[c] >= sumNeg[c]) ? +1 : -1;

  TFile *fout = new TFile(outDir + "/RunQC.root", "RECREATE");
  TH1F *hBase[NCH], *hRMS[NCH], *hAmp[NCH], *hPeakT[NCH];
  TH2F *hProf[NCH];
  for (int c = 0; c < NCH; ++c) {
    hBase[c]  = new TH1F(Form("hBase_ch%d", c),  Form("ch %d baseline;baseline [ADC];events", c), 256, 0, ADC_MAX + 1);
    hRMS[c]   = new TH1F(Form("hRMS_ch%d", c),   Form("ch %d baseline RMS;RMS [ADC];events", c), 200, 0, 200);
    hAmp[c]   = new TH1F(Form("hAmp_ch%d", c),   Form("ch %d amplitude;|peak - baseline| [ADC];events", c), 300, 0, 3000);
    hPeakT[c] = new TH1F(Form("hPeakT_ch%d", c), Form("ch %d peak sample;sample of pulse peak;events", c), 256, 0, NSAMP);
    hProf[c]  = new TH2F(Form("hProf_ch%d", c),  Form("ch %d waveform profile;sample;ADC", c), 256, 0, NSAMP, 256, 0, ADC_MAX + 1);
  }

  long nSat[NCH] = {0}, nEdgeLo[NCH] = {0}, nEdgeHi[NCH] = {0}, nSig[NCH] = {0};
  double baseSum[NCH] = {0}, baseSum2[NCH] = {0};

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    if (i % 5000 == 0) printf("  event %lld / %lld\n", i, nEnt);
    for (int c = 0; c < NCH; ++c) {
      double base = 0, base2 = 0;
      for (int s = 0; s < BASE_END; ++s) { base += ch[c][s]; base2 += ch[c][s]*ch[c][s]; }
      base /= BASE_END;
      double rms = std::sqrt(std::max(0.0, base2/BASE_END - base*base));
      hBase[c]->Fill(base);
      hRMS[c]->Fill(rms);
      baseSum[c] += base; baseSum2[c] += base*base;

      int   pkS = 0;
      float pkV = ch[c][0];
      bool  sat = false;
      for (int s = 0; s < NSAMP; ++s) {
        float v = ch[c][s];
        if (pol[c] > 0 ? (v > pkV) : (v < pkV)) { pkV = v; pkS = s; }
        if (v >= ADC_MAX - SAT_LO || v <= SAT_LO) sat = true;
      }
      float amp = pol[c] > 0 ? (pkV - base) : (base - pkV);
      hAmp[c]->Fill(amp);
      if (sat) ++nSat[c];

      // Peak-time bookkeeping only for real pulses, > 10x baseline RMS
      if (amp > 10.0 * std::max(rms, 1.0)) {
        ++nSig[c];
        hPeakT[c]->Fill(pkS);
        if (pkS < EDGE)          ++nEdgeLo[c];
        if (pkS >= NSAMP - EDGE) ++nEdgeHi[c];
      }
      if (i % 25 == 0)   // profile every 25th event to keep it light
        for (int s = 0; s < NSAMP; ++s) hProf[c]->Fill(s, ch[c][s]);
    }
  }

  // ---- summary ----
  FILE *sum = fopen(outDir + "/RunQC_summary.txt", "w");
  fprintf(sum, "RunQC summary, run %d, %lld events, file %s\n", run, nEnt, dataFile.Data());
  fprintf(sum, "%3s %4s %10s %8s %8s %8s %8s %8s %8s\n",
          "ch", "pol", "base", "baseRMS", "sig frac", "sat frac", "peak med", "edgeLo", "edgeHi");
  printf("\n%3s %4s %10s %8s %8s %8s %8s %8s %8s\n",
         "ch", "pol", "base", "baseRMS", "sig frac", "sat frac", "peak med", "edgeLo", "edgeHi");
  for (int c = 0; c < NCH; ++c) {
    double bm = baseSum[c]/nEnt;
    double br = std::sqrt(std::max(0.0, baseSum2[c]/nEnt - bm*bm));
    double med = 0; { double q = 0.5; hPeakT[c]->GetQuantiles(1, &med, &q); }
    double sigf = double(nSig[c])/nEnt, satf = double(nSat[c])/nEnt;
    double elo = nSig[c] ? double(nEdgeLo[c])/nSig[c] : 0;
    double ehi = nSig[c] ? double(nEdgeHi[c])/nSig[c] : 0;
    fprintf(sum, "%3d %+4d %10.1f %8.1f %8.3f %8.4f %8.0f %8.4f %8.4f\n",
            c, pol[c], bm, br, sigf, satf, med, elo, ehi);
    printf("%3d %+4d %10.1f %8.1f %8.3f %8.4f %8.0f %8.4f %8.4f\n",
           c, pol[c], bm, br, sigf, satf, med, elo, ehi);
  }
  fclose(sum);

  // ---- plots ----
  TCanvas cv("cv", "cv", 1600, 1000);
  TString pdf = outDir + "/RunQC.pdf";
  cv.Print(pdf + "[");
  const char *what[4] = {"baseline", "amplitude", "peak sample", "profile"};
  for (int w = 0; w < 4; ++w) {
    cv.Clear(); cv.Divide(6, 3);
    for (int c = 0; c < NCH; ++c) {
      cv.cd(c + 1);
      gPad->SetLogy(w == 1 || w == 2);
      if (w == 0) hBase[c]->Draw("hist");
      if (w == 1) hAmp[c]->Draw("hist");
      if (w == 2) hPeakT[c]->Draw("hist");
      if (w == 3) { gPad->SetLogy(0); gPad->SetLogz(1); hProf[c]->Draw("colz"); }
    }
    cv.cd(0);
    TLatex l; l.SetNDC(); l.SetTextSize(0.02);
    l.DrawLatex(0.005, 0.005, Form("run %d — %s", run, what[w]));
    cv.Print(pdf);
  }
  cv.Print(pdf + "]");

  fout->Write();
  fout->Close();
  printf("\nWrote %s/RunQC.root, RunQC.pdf, RunQC_summary.txt\n", outDir.Data());
}
