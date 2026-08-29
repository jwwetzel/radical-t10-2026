// ChannelIntegrity.C — uniform channel-integrity treatment for EVERY run.
// Same plots, same analysis, both data formats:
//   runs <= 13 : raw 12-bit ADC counts, chs 0-17, MCP TR0 copies on 16/17
//   runs >= 14 : mV, slot = group*9 + ch, TR0 copies on 8/17
//
// Per run it produces (all in the house style):
//   Integrity_waveforms.png : all 18 channels, 8 overlaid events,
//                             baseline-subtracted, common ADC-eq scale
//   Integrity_health.png    : per-channel baseline RMS, signal fraction,
//                             rail fraction, median pulse-peak sample
//   Integrity_summary.txt   : health table + identity checks
//                             (TR0 copy-copy correlation, LG<->HG pairing)
//
// Usage: root -l -b -q 'macros/ChannelIntegrity.C+(12)'   (also 14, 15, ...)

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TGraph.h"
#include "TProfile.h"
#include "TCanvas.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <algorithm>
#include "radStyle.h"

static const int NCH = 18, NSAMP = 1024;

struct RunCfg {
  TString file;
  double toADC;          // multiply (value - baseline) by this to get ADC-eq
  const char *nm[NCH];
  int pol[NCH];
  int bwin[NCH];
  int mcpA, mcpB;        // the two TR0 copies
  int LG[4], HG[4];      // pairing convention (TL,TR,BL,BR order)
};

static RunCfg cfgFor(int run)
{
  RunCfg c;
  if (run <= 13) {
    c.file = TString::Format("data/run_%d.root", run);
    c.toADC = 1.0;
    static const char *n[NCH] = {"XCET40","XCET43","ScintA","EMPTY","LG 4","LG 5","LG 6","LG 7",
                                 "EMPTY","EMPTY","EMPTY","EMPTY","HG(5)","HG(4)","HG(7)","HG(6)","TR0 g0","TR0 g1"};
    for (int i = 0; i < NCH; ++i) c.nm[i] = n[i];
    int p[NCH] = {-1,-1,-1,+1, +1,+1,+1,+1, +1,+1,+1,+1, +1,+1,+1,+1, -1,-1};
    for (int i = 0; i < NCH; ++i) { c.pol[i] = p[i]; c.bwin[i] = (i <= 2) ? 200 : 80; }
    c.mcpA = 16; c.mcpB = 17;
    int lg[4] = {4,5,6,7}, hg[4] = {13,12,15,14};
    for (int i = 0; i < 4; ++i) { c.LG[i] = lg[i]; c.HG[i] = hg[i]; }
  } else {
    c.file = TString::Format("data/download/run_%d.root", run);
    c.toADC = 4.095;
    static const char *n[NCH] = {"XCET40","XCET43","ScintA","EMPTY","LG 4","LG 5","LG 6","LG 7",
                                 "TR0 g0","EMPTY","EMPTY","EMPTY","EMPTY","HG(5)","HG(4)","HG(7)","HG(6)","TR0 g1"};
    for (int i = 0; i < NCH; ++i) c.nm[i] = n[i];
    int p[NCH] = {-1,-1,-1,+1, +1,+1,+1,+1, -1, +1,+1,+1,+1, +1,+1,+1,+1, -1};
    for (int i = 0; i < NCH; ++i) { c.pol[i] = p[i]; c.bwin[i] = (i <= 2) ? 200 : 40; }
    c.mcpA = 8; c.mcpB = 17;
    int lg[4] = {4,5,6,7}, hg[4] = {14,13,16,15};
    for (int i = 0; i < 4; ++i) { c.LG[i] = lg[i]; c.HG[i] = hg[i]; }
  }
  return c;
}

void ChannelIntegrity(int run = 15)
{
  SetRadStyle();
  RunCfg cfg = cfgFor(run);
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(cfg.file);
  if (!fin || fin->IsZombie()) { printf("cannot open %s\n", cfg.file.Data()); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NCH][NSAMP];
  int evnum;
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("event", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("event", &evnum);
  const Long64_t nEnt = t->GetEntries();

  FILE *sum = fopen(outDir + "/Integrity_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };
  out("ChannelIntegrity run %d: %lld events (%s format)\n\n", run, nEnt,
      cfg.toADC == 1.0 ? "raw ADC" : "mV");

  auto amp = [&](int s, double *baseOut = nullptr){
    double b = 0; for (int k = 0; k < cfg.bwin[s]; ++k) b += ch[s][k]; b /= cfg.bwin[s];
    if (baseOut) *baseOut = b;
    float ex = ch[s][0];
    for (int k = 0; k < NSAMP; ++k){ float v = ch[s][k];
      if (cfg.pol[s] > 0 ? v > ex : v < ex) ex = v; }
    return (cfg.pol[s] > 0 ? ex - b : b - ex) * cfg.toADC; };

  // run-global extrema for rail detection (subsampled)
  double gmax[NCH], gmin[NCH];
  for (int s = 0; s < NCH; ++s) { gmax[s] = -1e9; gmin[s] = 1e9; }
  for (Long64_t i = 0; i < nEnt; i += 7) { t->GetEntry(i);
    for (int s = 0; s < NCH; ++s) for (int k = 0; k < NSAMP; ++k) {
      if (ch[s][k] > gmax[s]) gmax[s] = ch[s][k];
      if (ch[s][k] < gmin[s]) gmin[s] = ch[s][k]; } }
  const double railTol = (cfg.toADC == 1.0) ? 1.5 : 0.4;   // ~1.5 counts

  // persistence maps (every event, every sample) + beam-average waveforms
  TH2F *hPer[NCH]; TProfile *pAvg[NCH];
  for (int s2 = 0; s2 < NCH; ++s2) {
    hPer[s2] = new TH2F(Form("hPer%d", s2), ";sample;ADC-eq", 256, 0, NSAMP, 240, -2800, 3600);
    pAvg[s2] = new TProfile(Form("pAvg%d", s2), "", 256, 0, NSAMP);
  }
  // accumulators
  double baseS[NCH] = {0}, rmsS[NCH] = {0};
  long nSig[NCH] = {0}, nRail[NCH] = {0}, edgeLo[NCH] = {0}, edgeHi[NCH] = {0}, evGap = 0;
  std::vector<short> pkT[NCH];
  // identity: TR0 copy correlation + LG-HG pairing (beam events)
  double mA_s=0,mB_s=0,mA2=0,mB2=0,mAB=0; long nM=0;
  double lx[4]={0},hx[4]={0},lxx[4]={0},hxx[4]={0},lh[4][4]={{0}}; long nP=0;
  int prevEv = -1;

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    if (prevEv >= 0 && evnum != prevEv + 1) ++evGap;
    prevEv = evnum;
    double a[NCH], b[NCH];
    for (int s = 0; s < NCH; ++s) {
      double bb, b2 = 0;
      a[s] = amp(s, &bb); b[s] = bb;
      for (int k = 0; k < cfg.bwin[s]; ++k) b2 += (ch[s][k]-bb)*(ch[s][k]-bb);
      baseS[s] += bb; rmsS[s] += std::sqrt(b2/cfg.bwin[s]) * cfg.toADC;
      if (a[s] > 150) { ++nSig[s];
        float ex = ch[s][0]; int ps = 0;
        for (int k = 0; k < NSAMP; ++k){ float v = ch[s][k];
          if (cfg.pol[s] > 0 ? v > ex : v < ex) { ex = v; ps = k; } }
        pkT[s].push_back(ps);
        if (ps < 30) ++edgeLo[s];
        if (ps >= NSAMP - 30) ++edgeHi[s]; }
      bool rail = false;
      for (int k = 0; k < NSAMP; ++k)
        if (ch[s][k] > gmax[s]-railTol || ch[s][k] < gmin[s]+railTol) { rail = true; break; }
      if (rail) ++nRail[s];
    }
    for (int s2 = 0; s2 < NCH; ++s2)
      for (int k = 0; k < NSAMP; k += 2)                     // every 2nd sample: persistence
        hPer[s2]->Fill(k, (ch[s2][k] - b[s2]) * cfg.toADC);
    const bool beam = a[0] > 150 && a[1] > 150;
    if (beam) {
      for (int s2 = 0; s2 < NCH; ++s2)
        for (int k = 0; k < NSAMP; k += 2)
          pAvg[s2]->Fill(k, (ch[s2][k] - b[s2]) * cfg.toADC);
      double mA = amp(cfg.mcpA), mB = amp(cfg.mcpB);
      mA_s += mA; mB_s += mB; mA2 += mA*mA; mB2 += mB*mB; mAB += mA*mB; ++nM;
      ++nP;
      for (int j = 0; j < 4; ++j) { double L = a[cfg.LG[j]];
        lx[j] += L; lxx[j] += L*L;
        for (int k = 0; k < 4; ++k) { double H = a[cfg.HG[k]];
          lh[j][k] += L*H; if (j == 0) { hx[k] += H; hxx[k] += H*H; } } }
    }
    if (i % 5000 == 0) printf("  %lld/%lld\n", i, nEnt);
  }

  // ---- summary table ----
  out("event-number gaps: %ld\n\n", evGap);
  out("%4s %8s %10s %8s %9s %9s %8s %8s %8s\n",
      "ch", "role", "base", "RMSeq", "sig frac", "rail frac", "pk med", "edgeLo", "edgeHi");
  double pkMed[NCH];
  for (int s = 0; s < NCH; ++s) {
    pkMed[s] = 0;
    if (pkT[s].size() > 10) { std::sort(pkT[s].begin(), pkT[s].end());
      pkMed[s] = pkT[s][pkT[s].size()/2]; }
    out("%4d %8s %10.1f %8.2f %9.3f %9.4f %8.0f %8.4f %8.4f\n",
        s, cfg.nm[s], baseS[s]/nEnt, rmsS[s]/nEnt, double(nSig[s])/nEnt,
        double(nRail[s])/nEnt, pkMed[s],
        nSig[s] ? double(edgeLo[s])/nSig[s] : 0, nSig[s] ? double(edgeHi[s])/nSig[s] : 0);
  }
  // identity checks
  double covM = mAB/nM - (mA_s/nM)*(mB_s/nM);
  double vA = mA2/nM - std::pow(mA_s/nM,2), vB = mB2/nM - std::pow(mB_s/nM,2);
  out("\nTR0 copy-copy amplitude correlation (beam): %.3f  (expect ~1: same MCP pulse)\n",
      covM/std::sqrt(vA*vB));
  out("LG->HG pairing check (best correlation per LG):\n");
  for (int j = 0; j < 4; ++j) {
    double best = -2; int bi = -1;
    for (int k = 0; k < 4; ++k) {
      double cov = lh[j][k]/nP - (lx[j]/nP)*(hx[k]/nP);
      double vl = lxx[j]/nP - std::pow(lx[j]/nP,2), vh = hxx[k]/nP - std::pow(hx[k]/nP,2);
      double r = cov/std::sqrt(vl*vh);
      if (r > best) { best = r; bi = k; }
    }
    out("  LG %d -> ch %d (r=%.2f)%s\n", cfg.LG[j], cfg.HG[bi], best,
        cfg.HG[bi] == cfg.HG[j] ? "  [matches convention]" : "  [MISMATCH]");
  }

  // ---- waveform persistence grid: ALL events overlaid (log-z), 18 pads ----
  TCanvas cw("cw", "cw", 2100, 1050); cw.Divide(6, 3, 0.003, 0.006);
  gStyle->SetPalette(kSunset); TColor::InvertPalette();
  for (int s = 0; s < NCH; ++s) {
    cw.cd(s + 1);
    gPad->SetLeftMargin(0.17); gPad->SetBottomMargin(0.15); gPad->SetLogz();
    hPer[s]->GetXaxis()->SetTitleSize(15); hPer[s]->GetYaxis()->SetTitleSize(15);
    hPer[s]->GetXaxis()->SetLabelSize(13); hPer[s]->GetYaxis()->SetLabelSize(13);
    hPer[s]->SetMinimum(0.8);
    hPer[s]->Draw("col");
    pAvg[s]->SetLineColor(rad::cTeal()); pAvg[s]->SetLineWidth(3);
    pAvg[s]->Draw("hist same");
    TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(17);
    l.DrawLatex(0.20, 0.85, Form("%d  #font[62]{%s}", s, cfg.nm[s]));
  }
  cw.SaveAs(outDir + "/Integrity_waveforms.png");

  // ---- health canvas: 4 bar panels ----
  TCanvas ck("ck", "ck", 1900, 520); ck.Divide(4, 1, 0.004, 0.004);
  const char *ttl[4] = {"baseline RMS  [ADC-eq]", "signal fraction  (amp > 150)",
                        "rail fraction", "median pulse-peak sample"};
  for (int p = 0; p < 4; ++p) {
    ck.cd(p + 1);
    gPad->SetBottomMargin(0.20); gPad->SetLeftMargin(0.13);
    TH1F *h = new TH1F(Form("hk%d", p), ";;", NCH, 0, NCH);
    for (int s = 0; s < NCH; ++s) {
      double v = p == 0 ? rmsS[s]/nEnt : p == 1 ? double(nSig[s])/nEnt
               : p == 2 ? double(nRail[s])/nEnt : pkMed[s];
      h->SetBinContent(s + 1, v);
      h->GetXaxis()->SetBinLabel(s + 1, Form("%d", s));
    }
    h->GetXaxis()->SetLabelSize(14); h->GetYaxis()->SetLabelSize(15);
    h->SetFillColor(rad::cFill()); h->SetLineColor(rad::cTeal()); h->SetLineWidth(2);
    if (p == 0 || p == 2) { gPad->SetLogy(); h->SetMinimum(p == 2 ? 1e-4 : 0.05); }
    if (p == 3) { h->SetMinimum(0); h->SetMaximum(1024); }
    h->Draw("hist");
    if (p == 3) {
      TLine *e1 = new TLine(0, 30, NCH, 30), *e2 = new TLine(0, 994, NCH, 994);
      for (TLine *e : {e1, e2}) { e->SetLineColor(rad::cRed()); e->SetLineStyle(2); e->SetLineWidth(2); e->Draw(); }
    }
    TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(19);
    l.DrawLatex(0.15, 0.86, ttl[p]);
  }
  ck.SaveAs(outDir + "/Integrity_health.png");

  fclose(sum);
  printf("Wrote %s/Integrity_{waveforms,health}.png, Integrity_summary.txt\n", outDir.Data());
}
