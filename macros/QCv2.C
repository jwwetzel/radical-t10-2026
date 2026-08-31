// QCv2.C — full run QC for the run-14+ data format.
//
// Format (per run-15 metadata): waveforms in mV = 1000*(counts/4095 - 0.5);
// array slot = group*9 + ch_in_group; slots 8 and 17 are TR0 (MCP) copies.
// Slots: 0/1 XCET Cherenkovs, 2 scint, 3 empty, 4-7 LG upstream,
//        8 MCP(grp0), 9-12 empty, 13-16 HG upstream, 17 MCP(grp1).
// HG pairing (DT5742-native, unchanged since run 12): LG4-s14, LG5-s13,
// LG6-s16, LG7-s15. Positions: 4 TL, 5 TR, 6 BL, 7 BR.
//
// Checks: completeness (entries, event-number continuity, trigger-tag gaps),
// per-slot baseline/noise/signal/clipping, pulse containment in the record,
// Cherenkov coincidence rate, MIP calibration per HG slot, and beam
// alignment (miss fraction + MIP-normalized row/column ratios).
//
// Usage: root -l -b -q 'macros/QCv2.C+(15)'   (reads data/download/run_<N>.root)

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TString.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <algorithm>
#include "radStyle.h"

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;      // 1000 mV / 4095 counts
static const float CHER_THR = 150.0f;    // ADC-eq
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int EDGE = 30;

void QCv2(int run = 15, double cthr = CHER_THR)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(TString::Format("data/download/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("cannot open run file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NSLOT][NSAMP];
  int evnum; unsigned int ttt;
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("event", 1);
  t->SetBranchStatus("trigger_time_tag", 1);
  t->SetBranchAddress("channel", ch);
  t->SetBranchAddress("event", &evnum);
  t->SetBranchAddress("trigger_time_tag", &ttt);
  const Long64_t nEnt = t->GetEntries();

  FILE *sum = fopen(outDir + "/QCv2_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };

  // slot metadata
  const char *nm[NSLOT] = {"XCET40","XCET43","ScintA","EMPTY","LG-TL4","LG-TR5","LG-BL6","LG-BR7",
                           "MCPg0","EMPTY","EMPTY","EMPTY","EMPTY","HG(5)","HG(4)","HG(7)","HG(6)","MCPg1"};
  int pol[NSLOT] = {-1,-1,-1,+1, +1,+1,+1,+1, -1, +1,+1,+1,+1, +1,+1,+1,+1, -1};
  int bwin[NSLOT]; for (int s = 0; s < NSLOT; ++s) bwin[s] = (s <= 2) ? BASE_CTR : BASE_MOD;
  // alignment mapping: position j (TL,TR,BL,BR) -> LG slot, HG slot
  const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

  // run-global extrema for rail detection
  double gmax[NSLOT], gmin[NSLOT];
  for (int s = 0; s < NSLOT; ++s) { gmax[s] = -1e9; gmin[s] = 1e9; }
  for (Long64_t i = 0; i < nEnt; i += 7) {           // subsample is enough for rails
    t->GetEntry(i);
    for (int s = 0; s < NSLOT; ++s) for (int k = 0; k < NSAMP; ++k) {
      if (ch[s][k] > gmax[s]) gmax[s] = ch[s][k];
      if (ch[s][k] < gmin[s]) gmin[s] = ch[s][k];
    }
  }

  // histograms
  TH1F *hMIP[4], *hPkT[NSLOT];
  for (int j = 0; j < 4; ++j)
    hMIP[j] = new TH1F(Form("hMIP_%d", HGs[j]), ";HG amplitude [ADC-eq];events", 100, 0, 2000);
  for (int s = 0; s < NSLOT; ++s)
    hPkT[s] = new TH1F(Form("hPkT_%d", s), Form("%s peak sample;sample", nm[s]), 128, 0, NSAMP);
  TH1F *hSumLG = new TH1F("hSumLG", "electrons #Sigma LG;#Sigma LG [ADC-eq]", 90, 0, 4500);

  // accumulators
  long nBeam = 0, nMiss = 0, evGap = 0, tttNonMono = 0;
  long nRailBeam[4] = {0}, nSig[NSLOT] = {0};
  double baseS[NSLOT] = {0}, baseS2[NSLOT] = {0}, rmsS[NSLOT] = {0};
  std::vector<float> hgBeam[4];
  int prevEv = -1; unsigned int prevT = 0; bool firstT = true;
  long edgeLo[NSLOT] = {0}, edgeHi[NSLOT] = {0};

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    if (prevEv >= 0 && evnum != prevEv + 1) ++evGap;
    prevEv = evnum;
    if (!firstT && ttt < prevT) ++tttNonMono;
    prevT = ttt; firstT = false;

    double amp[NSLOT], base[NSLOT]; int pkS[NSLOT];
    for (int s = 0; s < NSLOT; ++s) {
      double b = 0, b2 = 0;
      for (int k = 0; k < bwin[s]; ++k) { b += ch[s][k]; b2 += ch[s][k]*ch[s][k]; }
      b /= bwin[s]; base[s] = b;
      rmsS[s] += std::sqrt(std::max(0.0, b2/bwin[s] - b*b));
      baseS[s] += b; baseS2[s] += b*b;
      float ex = ch[s][0]; int ps = 0;
      for (int k = 0; k < NSAMP; ++k) { float v = ch[s][k];
        if (pol[s] > 0 ? v > ex : v < ex) { ex = v; ps = k; } }
      amp[s] = (pol[s] > 0 ? ex - b : b - ex) * MV2ADC;
      pkS[s] = ps;
    }
    const bool beam = amp[0] > cthr && amp[1] > cthr;
    for (int s = 0; s < NSLOT; ++s)
      if (amp[s] > 150) { ++nSig[s]; hPkT[s]->Fill(pkS[s]);
        if (pkS[s] < EDGE) ++edgeLo[s];
        if (pkS[s] >= NSAMP - EDGE) ++edgeHi[s]; }

    if (beam) {
      ++nBeam;
      double S = 0;
      for (int j = 0; j < 4; ++j) {
        S += amp[LGs[j]];
        hgBeam[j].push_back(amp[HGs[j]]);
        // rail: any sample within 1 mV of the slot's run-global max
        bool rail = false;
        for (int k = 0; k < NSAMP; ++k) if (ch[HGs[j]][k] > gmax[HGs[j]] - 1.0) { rail = true; break; }
        if (rail) ++nRailBeam[j];
      }
      hSumLG->Fill(S);
      if (S < 300) ++nMiss;
    } else {
      for (int j = 0; j < 4; ++j) hMIP[j]->Fill(amp[HGs[j]]);
    }
    if (i % 5000 == 0) printf("  %lld / %lld\n", i, nEnt);
  }

  // ---- report ----
  out("QCv2 run %d: %lld entries\n\n== COMPLETENESS ==\n", run, nEnt);
  out("event-number gaps: %ld;  trigger_time_tag non-monotonic steps: %ld (wraps are normal)\n", evGap, tttNonMono);
  out("\n== CHANNEL HEALTH ==\n%5s %8s %10s %8s %9s %9s %9s\n",
      "slot", "name", "base[mV]", "RMS[mV]", "sig frac", "edgeLo", "edgeHi");
  for (int s = 0; s < NSLOT; ++s) {
    double sf = double(nSig[s]) / nEnt;
    out("%5d %8s %10.1f %8.2f %9.3f %9.4f %9.4f\n", s, nm[s], baseS[s]/nEnt, rmsS[s]/nEnt, sf,
        nSig[s] ? double(edgeLo[s])/nSig[s] : 0, nSig[s] ? double(edgeHi[s])/nSig[s] : 0);
  }

  out("\n== BEAM / TRIGGER ==\n");
  out("Cherenkov coincidence: %ld / %lld = %.2f%%\n", nBeam, nEnt, 100.0*nBeam/nEnt);

  // MIP fits
  out("\n== MIP CALIBRATION (off-coincidence) ==\n");
  double MPV[4];
  TCanvas cM("cM","cM",1600,1200); cM.Divide(2,2);
  for (int j = 0; j < 4; ++j) {
    cM.cd(j+1); gPad->SetLogy();
    int pb = 0; double pv = 0;
    for (int b = hMIP[j]->FindBin(120); b <= hMIP[j]->GetNbinsX(); ++b)
      if (hMIP[j]->GetBinContent(b) > pv) { pv = hMIP[j]->GetBinContent(b); pb = b; }
    double pk = hMIP[j]->GetBinCenter(pb);
    TF1 *lan = new TF1(Form("lan%d", j), "landau", 0.55*pk, 2.5*pk);
    hMIP[j]->Fit(lan, "QR"); hMIP[j]->Draw("hist"); lan->Draw("same");
    MPV[j] = lan->GetParameter(1);
    TLatex tl; tl.SetNDC(); tl.SetTextFont(43); tl.SetTextSize(24);
    tl.DrawLatex(0.55, 0.85, Form("#font[62]{capillary %d}  (slot %d)", LGs[j], HGs[j]));
    tl.DrawLatex(0.55, 0.78, Form("MPV %.0f ADC-eq", MPV[j]));
    out("cap %d (slot %d): MIP MPV = %.1f +/- %.1f ADC-eq\n", LGs[j], HGs[j], MPV[j], lan->GetParError(1));
  }
  cM.SaveAs(outDir + "/QCv2_MIP.png");

  // alignment
  out("\n== ALIGNMENT ==\n");
  double p = double(nMiss)/nBeam;
  out("miss fraction (SumLG<300): %.1f%% +/- %.1f%%   (run 14: 7.9%%, run 12: 42.9%%)\n",
      100*p, 100*std::sqrt(p*(1-p)/nBeam));
  const char *pos[4] = {"TL(4)","TR(5)","BL(6)","BR(7)"};
  double m[4], e[4];
  for (int j = 0; j < 4; ++j) {
    double s1 = 0, s2 = 0;
    for (float a : hgBeam[j]) { double v = a/MPV[j]; s1 += v; s2 += v*v; }
    m[j] = s1/nBeam; e[j] = std::sqrt((s2/nBeam - m[j]*m[j])/nBeam);
    out("  %s: %5.2f +/- %.2f MIP-eq   HG rail frac (beam): %.3f\n", pos[j], m[j], e[j],
        double(nRailBeam[j])/nBeam);
  }
  double top=m[0]+m[1], bot=m[2]+m[3], lef=m[0]+m[2], rig=m[1]+m[3];
  double rr=bot/top, cc=lef/rig;
  double rre=rr*std::sqrt((e[0]*e[0]+e[1]*e[1])/(top*top)+(e[2]*e[2]+e[3]*e[3])/(bot*bot));
  double cce=cc*std::sqrt((e[0]*e[0]+e[2]*e[2])/(lef*lef)+(e[1]*e[1]+e[3]*e[3])/(rig*rig));
  out("rows bottom/top = %.3f +/- %.3f (centered = 1)\ncols left/right = %.3f +/- %.3f\n", rr, rre, cc, cce);

  // plots: sum spectrum + peak-time containment for key slots
  TCanvas cA("cA","cA",1600,600); cA.Divide(2,1);
  cA.cd(1); hSumLG->SetTitle(";#SigmaLG [ADC-eq];events"); hSumLG->Draw("hist");
  { TLatex tl; tl.SetNDC(); tl.SetTextFont(43); tl.SetTextSize(24);
    tl.DrawLatex(0.55, 0.85, "#font[62]{#SigmaLG, tagged events}"); }
  cA.cd(2); gPad->SetLogy();
  int key[5] = {0, 2, 4, 14, 8}; int col[5] = {kBlack, kGray+2, kBlue, kRed, kGreen+2};
  const char *knm[5] = {"XCET40", "ScintA", "LG cap 4", "HG cap 4", "MCP g0"};
  TLegend *lgA = new TLegend(0.62, 0.60, 0.93, 0.88); lgA->SetBorderSize(0); lgA->SetTextFont(43); lgA->SetTextSize(19);
  for (int k = 0; k < 5; ++k) { hPkT[key[k]]->SetLineColor(col[k]); hPkT[key[k]]->SetLineWidth(2);
    hPkT[key[k]]->SetTitle(";pulse peak time [sample];events");
    hPkT[key[k]]->Draw(k ? "hist same" : "hist"); lgA->AddEntry(hPkT[key[k]], knm[k], "l"); }
  lgA->Draw();
  cA.SaveAs(outDir + "/QCv2_align.png");

  fclose(sum);
  printf("\nWrote %s/QCv2_summary.txt, QCv2_MIP.png, QCv2_align.png\n", outDir.Data());
}
