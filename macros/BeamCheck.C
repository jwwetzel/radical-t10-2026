// BeamCheck.C — beam-selected look at the RADiCAL upstream channels.
//
// Selects beam events with a Cherenkov coincidence (ch 0 AND ch 1 amplitude
// above threshold) and compares the upstream SiPM channels in selected vs
// rejected events:
//   - average waveform per channel (beam vs off-beam)
//   - amplitude spectra in beam events (low gain 4-7, high gain 12-15)
//   - saturation fraction of the high-gain channels in beam events
//
// Usage:  root -l -b -q 'macros/BeamCheck.C+(12)'
// Writes Output/run_<N>/BeamCheck.{root,png} and BeamCheck_summary.txt.

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TProfile.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TSystem.h"
#include <cstdio>
#include <cmath>
#include "radStyle.h"

static const int NCH   = 18;
static const int NSAMP = 1024;
static const int BASE_END = 200;      // counters: pulses arrive ~sample 460+
static const int BASE_END_UP = 80;    // upstream module: pulse arrives ~sample 90
static const float ADC_MAX = 4095.0f;
static const float CHER_THR = 150.0f;   // Cherenkov amplitude cut, ADC counts

// per-event baseline and (signed) peak amplitude for one channel
static void basePeak(const float *w, int polarity, double &base, double &amp, int &pkS, bool &sat,
                     int baseEnd = BASE_END)
{
  base = 0;
  for (int s = 0; s < baseEnd; ++s) base += w[s];
  base /= baseEnd;
  float pkV = w[0];
  pkS = 0; sat = false;
  for (int s = 0; s < NSAMP; ++s) {
    float v = w[s];
    if (polarity > 0 ? (v > pkV) : (v < pkV)) { pkV = v; pkS = s; }
    if (v >= ADC_MAX - 5.0f || v <= 5.0f) sat = true;
  }
  amp = polarity > 0 ? (pkV - base) : (base - pkV);
}

void BeamCheck(int run = 12)
{
  SetRadStyle();
  TString dataFile = TString::Format("data/run_%d.root", run);
  TString outDir   = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);

  TFile *fin = TFile::Open(dataFile);
  if (!fin || fin->IsZombie()) { printf("Cannot open %s\n", dataFile.Data()); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NCH][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t nEnt = t->GetEntries();

  // channels to study, laid out 5x2: top row low gain (+) and MCP0 (-),
  // bottom row high gain (+) and MCP1 (-)
  const int nUp = 10;
  const int upCh[nUp]  = {4, 5, 6, 7, 16, 12, 13, 14, 15, 17};
  const int upPol[nUp] = {+1, +1, +1, +1, -1, +1, +1, +1, +1, -1};

  TFile *fout = new TFile(outDir + "/BeamCheck.root", "RECREATE");
  TProfile *pBeam[nUp], *pOff[nUp];
  TH1F *hAmpBeam[nUp], *hAmpOff[nUp];
  for (int i = 0; i < nUp; ++i) {
    int c = upCh[i];
    pBeam[i] = new TProfile(Form("pBeam_ch%d", c),
      Form("ch %d avg waveform;sample;ADC", c), 256, 0, NSAMP);
    pOff[i]  = new TProfile(Form("pOff_ch%d", c), "", 256, 0, NSAMP);
    hAmpBeam[i] = new TH1F(Form("hAmpBeam_ch%d", c),
      Form("ch %d amplitude, beam events;|peak-baseline| [ADC];events", c), 250, 0, 2500);
    hAmpOff[i]  = new TH1F(Form("hAmpOff_ch%d", c), "", 250, 0, 2500);
  }

  Long64_t nBeam = 0;
  long nSatBeam[nUp] = {0};
  double ampSum[nUp] = {0};

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    double b0, a0, b1, a1; int s0, s1; bool q0, q1;
    basePeak(ch[0], -1, b0, a0, s0, q0);
    basePeak(ch[1], -1, b1, a1, s1, q1);
    const bool beam = (a0 > CHER_THR && a1 > CHER_THR);
    if (beam) ++nBeam;

    for (int j = 0; j < nUp; ++j) {
      int c = upCh[j];
      double base, amp; int pkS; bool sat;
      basePeak(ch[c], upPol[j], base, amp, pkS, sat, BASE_END_UP);
      if (beam) {
        hAmpBeam[j]->Fill(amp);
        ampSum[j] += amp;
        if (sat) ++nSatBeam[j];
        for (int s = 0; s < NSAMP; ++s) pBeam[j]->Fill(s, ch[c][s] - base);
      } else {
        hAmpOff[j]->Fill(amp);
        if (i % 4 == 0)   // off-beam profile from a subsample
          for (int s = 0; s < NSAMP; ++s) pOff[j]->Fill(s, ch[c][s] - base);
      }
    }
    if (i % 5000 == 0) printf("  event %lld / %lld\n", i, nEnt);
  }

  FILE *sum = fopen(outDir + "/BeamCheck_summary.txt", "w");
  auto both = [&](const char *line) { fputs(line, sum); fputs(line, stdout); };
  char buf[512];
  snprintf(buf, sizeof buf,
    "BeamCheck run %d: %lld / %lld events pass Cherenkov coincidence (ch0 & ch1 amp > %.0f)\n\n",
    run, nBeam, nEnt, CHER_THR); both(buf);
  snprintf(buf, sizeof buf, "%4s %10s %14s %12s %14s\n",
    "ch", "role", "mean amp beam", "sat frac", "end-of-record avg\n"); both(buf);
  for (int j = 0; j < nUp; ++j) {
    int c = upCh[j];
    // average baseline-subtracted level over the last 32 samples in beam events
    double tail = 0; int nb = 0;
    for (int b = pBeam[j]->GetNbinsX() - 7; b <= pBeam[j]->GetNbinsX(); ++b) { tail += pBeam[j]->GetBinContent(b); ++nb; }
    tail /= nb;
    snprintf(buf, sizeof buf, "%4d %10s %14.1f %12.4f %14.1f\n",
      c, c == 16 || c == 17 ? "MCP" : (c < 8 ? "low-gain" : "high-gain"),
      nBeam ? ampSum[j]/nBeam : 0,
      nBeam ? double(nSatBeam[j])/nBeam : 0, tail); both(buf);
  }
  fclose(sum);

  TCanvas cv("cv", "cv", 2200, 900); cv.Divide(5, 2);
  for (int j = 0; j < nUp; ++j) {
    cv.cd(j + 1);
    pBeam[j]->SetLineColor(kRed);  pBeam[j]->SetLineWidth(2);
    pOff[j]->SetLineColor(kBlue);
    pBeam[j]->Draw("hist"); pOff[j]->Draw("hist same");
    if (j == 0) {
      TLegend *lg = new TLegend(0.55, 0.72, 0.88, 0.88);
      lg->AddEntry(pBeam[j], "beam (Cher. coinc.)", "l");
      lg->AddEntry(pOff[j], "off-beam", "l");
      lg->Draw();
    }
  }
  cv.SaveAs(outDir + "/BeamCheck_profiles.png");

  TCanvas cv2("cv2", "cv2", 2200, 900); cv2.Divide(5, 2);
  for (int j = 0; j < nUp; ++j) {
    cv2.cd(j + 1); gPad->SetLogy();
    hAmpBeam[j]->SetLineColor(kRed);
    hAmpOff[j]->SetLineColor(kBlue);
    hAmpBeam[j]->Draw("hist"); hAmpOff[j]->Draw("hist same");
  }
  cv2.SaveAs(outDir + "/BeamCheck_amplitudes.png");

  fout->Write();
  fout->Close();
  printf("\nWrote %s/BeamCheck.root, BeamCheck_profiles.png, BeamCheck_amplitudes.png, BeamCheck_summary.txt\n",
         outDir.Data());
}
