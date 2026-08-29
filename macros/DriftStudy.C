// DriftStudy.C — what actually drifted overnight? Time-series of every
// measurable stability quantity through runs 28+29 (merged, chronological),
// in 2,000-event slices (~30 min each):
//
//   1. srCFD shower-time width  (the symptom)
//   2. shower-time center       (drift vs jitter)
//   3. pooled HG MIP MPV        (SiPM gain proxy, off-coincidence hadrons)
//   4. HG baseline RMS          (SiPM dark noise proxy)
//   5. MCP amplitude            (MCP gain)
//   6. trigger rate             (beam/pileup conditions, from trigger_time_tag)
//   plus baselines (electronics DC levels) printed per slice.
//
// The point: whichever curves co-move with (1) name the culprit; whichever
// stay flat are exonerated.
//
// Usage: root -l -b -q 'macros/DriftStudy.C+'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TGraph.h"
#include "TCanvas.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "radStyle.h"

static const int NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

struct P { double base, amp; int pk; };
static P pf(const float *w, int pol, int be) {
  P r; r.base = 0; for (int s = 0; s < be; ++s) r.base += w[s]; r.base /= be;
  float pv = w[0]; r.pk = 0;
  for (int s = 0; s < NSAMP; ++s) { float v = w[s];
    if (pol > 0 ? v > pv : v < pv) { pv = v; r.pk = s; } }
  r.amp = (pol > 0 ? pv - r.base : r.base - pv) * MV2ADC; return r;
}
static double le(const float *w, const float *tx, int pol, double b, double thrA) {
  double thr = pol > 0 ? b + thrA/MV2ADC : b - thrA/MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s)
    if (pol > 0 ? w[s] >= thr : w[s] <= thr) {
      double v0 = w[s-1], v1 = w[s]; if (v1 == v0) return tx[s];
      return tx[s-1] + (thr - v0)/(v1 - v0) * (tx[s] - tx[s-1]); }
  return -1e9;
}
static void robust(std::vector<double> &v, double &rc, double &rw) {
  rc = 0; rw = 0; if (v.size() < 20) return;
  double s = 0, s2 = 0; for (double x : v) { s += x; s2 += x*x; }
  rc = s/v.size(); rw = std::sqrt(std::max(0.0, s2/v.size() - rc*rc));
  for (int it = 0; it < 5; ++it) { s = 0; s2 = 0; long n = 0;
    for (double x : v) if (std::fabs(x - rc) < 2.5*rw) { s += x; s2 += x*x; ++n; }
    if (n < 15) break; rc = s/n; rw = std::sqrt(std::max(0.0, s2/n - rc*rc)); }
  rw *= 1.0/0.9546;
}

void DriftStudy()
{
  SetRadStyle();
  gSystem->mkdir("Output/run_2829", true);
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  unsigned int ttt;
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchStatus("trigger_time_tag", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  t->SetBranchAddress("trigger_time_tag", &ttt);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  const double a = 180, b = 2.59, wall = 3180;   // 9 GeV transfer (Diag9)

  std::vector<double> dt[64], mipA[64], dttt[64];
  double rmsHG[64] = {0}, mcpS[64] = {0}, baseHG[64] = {0}, baseLG[64] = {0};
  long nEvt[64] = {0}, nMcp[64] = {0};
  unsigned int prevT = 0; bool first = true;

  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    ++nEvt[sl];
    if (!first && ttt > prevT) dttt[sl].push_back(double(ttt - prevT));
    prevT = ttt; first = false;
    // baselines + HG noise (every event)
    for (int j = 0; j < 4; ++j) {
      double bb = 0, b2 = 0;
      for (int k = 0; k < BASE_MOD; ++k) { bb += ch[HGs[j]][k]; b2 += ch[HGs[j]][k]*ch[HGs[j]][k]; }
      bb /= BASE_MOD;
      rmsHG[sl] += std::sqrt(std::max(0.0, b2/BASE_MOD - bb*bb)) * MV2ADC / 4;
      baseHG[sl] += bb / 4;
      double bl = 0; for (int k = 0; k < BASE_MOD; ++k) bl += ch[LGs[j]][k];
      baseLG[sl] += (bl / BASE_MOD) / 4;
    }
    P m1 = pf(ch[17], -1, BASE_MOD);
    if (m1.amp > 300) { mcpS[sl] += m1.amp; ++nMcp[sl]; }
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    const bool beam = c0.amp > 40 && c1.amp > 40;
    if (!beam) {              // hadrons: pooled MIP amplitudes
      for (int j = 0; j < 4; ++j) { double hA = pf(ch[HGs[j]], +1, BASE_MOD).amp;
        if (hA > 80 && hA < 1200) mipA[sl].push_back(hA); }
    } else if (m1.amp > 300) { // srCFD shower time
      double t1 = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
      if (t1 > -1e8) {
        double ts = 0; int nOK = 0;
        for (int j = 0; j < 4; ++j) {
          P l = pf(ch[LGs[j]], +1, BASE_MOD), h = pf(ch[HGs[j]], +1, BASE_MOD);
          double thr = 0.15*(a + b*l.amp);
          if (thr < 20*MV2ADC || thr > 0.9*wall || h.amp < thr) continue;
          double tc = le(ch[HGs[j]], tx[1], +1, h.base, thr);
          if (tc > -1e8) { ts += tc; ++nOK; }
        }
        if (nOK >= 2) dt[sl].push_back(ts/nOK - t1);
      }
    }
    if (i % 5000 == 0) printf("  %lld/%lld\n", i, N);
  }

  FILE *sum = fopen("Output/run_2829/DriftStudy_summary.txt", "w");
  fprintf(sum, "DriftStudy runs 28+29, %d slices of 2000 events (~30 min each; slices 0-2 = run 28)\n", NS);
  fprintf(sum, "%3s %7s %7s %8s %8s %8s %8s %9s %9s\n",
          "sl", "sigT", "cenT", "MIPmpv", "HGrms", "MCPamp", "rate", "baseHG", "baseLG");
  TGraph *g[6]; for (int q = 0; q < 6; ++q) g[q] = new TGraph();
  for (int sl = 0; sl < NS; ++sl) {
    double rc, rw; robust(dt[sl], rc, rw);
    // pooled MIP MPV: Landau fit
    double mpv = 0;
    if (mipA[sl].size() > 300) {
      TH1F hm("hm", "", 80, 0, 1200); hm.SetDirectory(nullptr);
      for (double x : mipA[sl]) hm.Fill(x);
      int pb = hm.GetMaximumBin(); double pk = hm.GetBinCenter(pb);
      TF1 lan("lan", "landau", 0.55*pk, 2.5*pk);
      hm.Fit(&lan, "QRN");
      mpv = lan.GetParameter(1);
    }
    std::sort(dttt[sl].begin(), dttt[sl].end());
    double medDt = dttt[sl].size() ? dttt[sl][dttt[sl].size()/2] : 0;
    double rate = medDt > 0 ? 1.0/(medDt*8.5e-9) : 0;   // DT5742 ttt LSB = 8.5 ns
    fprintf(sum, "%3d %7.0f %7.0f %8.1f %8.2f %8.0f %8.2f %9.1f %9.1f\n",
            sl, rw*1000, rc*1000, mpv, rmsHG[sl]/nEvt[sl], nMcp[sl] ? mcpS[sl]/nMcp[sl] : 0,
            rate, baseHG[sl]/nEvt[sl], baseLG[sl]/nEvt[sl]);
    g[0]->SetPoint(sl, sl*0.5, rw*1000);
    g[1]->SetPoint(sl, sl*0.5, rc*1000);
    g[2]->SetPoint(sl, sl*0.5, mpv);
    g[3]->SetPoint(sl, sl*0.5, rmsHG[sl]/nEvt[sl]);
    g[4]->SetPoint(sl, sl*0.5, nMcp[sl] ? mcpS[sl]/nMcp[sl] : 0);
    g[5]->SetPoint(sl, sl*0.5, rate);
  }
  fclose(sum);

  const char *ttl[6] = {"shower-time width [ps]  #font[62]{(the symptom)}",
                        "shower-time center [ps]", "pooled HG MIP MPV [ADC-eq]  (SiPM gain)",
                        "HG baseline RMS [ADC-eq]  (dark noise)", "MCP amplitude [ADC-eq]",
                        "trigger rate [Hz]"};
  TCanvas c("c", "c", 1500, 1750); c.Divide(1, 6, 0.002, 0.004);
  for (int q = 0; q < 6; ++q) {
    c.cd(q + 1);
    gPad->SetLeftMargin(0.09); gPad->SetBottomMargin(q == 5 ? 0.18 : 0.06);
    g[q]->SetTitle(q == 5 ? ";hours since run-28 start;" : ";;");
    g[q]->SetMarkerStyle(20); g[q]->SetMarkerSize(1.1);
    g[q]->SetMarkerColor(q == 0 ? rad::cRed() : rad::cInk());
    g[q]->SetLineColor(q == 0 ? rad::cRed() : rad::cTeal()); g[q]->SetLineWidth(2);
    g[q]->Draw("APL");
    g[q]->GetXaxis()->SetLimits(0, NS*0.5);
    g[q]->GetXaxis()->SetLabelSize(q == 5 ? 17 : 12);
    TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(18);
    l.DrawLatex(0.11, 0.83, ttl[q]);
  }
  c.SaveAs("Output/run_2829/DriftStudy.png");
  printf("Wrote Output/run_2829/DriftStudy.png, DriftStudy_summary.txt\n");
  fflush(nullptr);
  gSystem->Exit(0);
}

// per-slice TR0 copy-vs-copy width: pure digitizer timing (same MCP pulse
// in both DRS groups — SiPMs, beam, and the MCP itself all cancel).
void DriftTR0()
{
  SetRadStyle();
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  std::vector<double> dmm[64];
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    P m0 = pf(ch[8], -1, BASE_MOD), m1 = pf(ch[17], -1, BASE_MOD);
    if (m0.amp < 300 || m1.amp < 300) continue;
    double t0 = le(ch[8],  tx[0], -1, m0.base, 0.20*m0.amp);
    double t1 = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
    if (t0 > -1e8 && t1 > -1e8) dmm[sl].push_back(t0 - t1);
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  printf("slice | TR0g0-TR0g1 center [ps] | width [ps]  (pure DRS inter-group timing)\n");
  for (int sl = 0; sl < NS; ++sl) {
    double rc, rw; robust(dmm[sl], rc, rw);
    printf("%5d | %+8.0f | %6.0f   (N=%zu)\n", sl, rc*1000, rw*1000, dmm[sl].size());
  }
  fflush(nullptr);
  gSystem->Exit(0);
}

// per-slice tag health: XCET amplitudes, coincidence rate, and the
// low-deposit fraction of tagged events (contamination proxy).
void DriftTag()
{
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP];
  t->SetBranchStatus("*", 0); t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  double x0[64] = {0}, x1[64] = {0}; long nB[64] = {0}, nLo[64] = {0}, nE[64] = {0};
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    ++nE[sl];
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    if (!(c0.amp > 40 && c1.amp > 40)) continue;
    ++nB[sl]; x0[sl] += c0.amp; x1[sl] += c1.amp;
    double S = 0; for (int j = 0; j < 4; ++j) S += pf(ch[LGs[j]], +1, BASE_MOD).amp;
    if (S < 660) ++nLo[sl];
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  printf("slice | coinc%% | XCET40mean | XCET43mean | lowSum frac (contamination proxy)\n");
  for (int sl = 0; sl < NS; ++sl)
    printf("%5d | %5.2f | %7.0f | %7.0f | %5.2f\n", sl, 100.0*nB[sl]/nE[sl],
           nB[sl] ? x0[sl]/nB[sl] : 0, nB[sl] ? x1[sl]/nB[sl] : 0,
           nB[sl] ? double(nLo[sl])/nB[sl] : 0);
  fflush(nullptr);
  gSystem->Exit(0);
}

// per-slice timing WITH an on-module quality cut (SumLG > 660): if the
// intermittent widths vanish, the "drift" was estimator instability on a
// contaminated mixture, not the detector.
void DriftClean()
{
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  const double a = 180, b = 2.59, wall = 3180;
  std::vector<double> dt[64], all;
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    if (!(c0.amp > 40 && c1.amp > 40)) continue;
    double S = 0; for (int j = 0; j < 4; ++j) S += pf(ch[LGs[j]], +1, BASE_MOD).amp;
    if (S < 660) continue;                               // on-module showers only
    P m1 = pf(ch[17], -1, BASE_MOD); if (m1.amp < 300) continue;
    double t1 = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
    if (t1 < -1e8) continue;
    double ts = 0; int nOK = 0;
    for (int j = 0; j < 4; ++j) {
      P l = pf(ch[LGs[j]], +1, BASE_MOD), h = pf(ch[HGs[j]], +1, BASE_MOD);
      double thr = 0.15*(a + b*l.amp);
      if (thr < 20*MV2ADC || thr > 0.9*wall || h.amp < thr) continue;
      double tc = le(ch[HGs[j]], tx[1], +1, h.base, thr);
      if (tc > -1e8) { ts += tc; ++nOK; }
    }
    if (nOK >= 2) { dt[sl].push_back(ts/nOK - t1); all.push_back(ts/nOK - t1); }
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  printf("slice | N | center [ps] | width [ps]   (SumLG > 660 quality cut)\n");
  for (int sl = 0; sl < NS; ++sl) {
    double rc, rw; robust(dt[sl], rc, rw);
    printf("%5d | %3zu | %+8.0f | %6.0f\n", sl, dt[sl].size(), rc*1000, rw*1000);
  }
  double rc, rw; robust(all, rc, rw);
  printf("ALL 46k, quality cut: N=%zu, width = %.0f ps\n", all.size(), rw*1000);
  fflush(nullptr);
  gSystem->Exit(0);
}

// per-capillary slice widths: one bad channel vs common-mode drift.
void DriftPerCap()
{
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  const double a = 180, b = 2.59, wall = 3180;
  std::vector<double> dt[64][4], dcc[64];
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    if (!(c0.amp > 40 && c1.amp > 40)) continue;
    P m1 = pf(ch[17], -1, BASE_MOD); if (m1.amp < 300) continue;
    double t1 = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
    if (t1 < -1e8) continue;
    double tc[4] = {-1e9,-1e9,-1e9,-1e9};
    for (int j = 0; j < 4; ++j) {
      P l = pf(ch[LGs[j]], +1, BASE_MOD), h = pf(ch[HGs[j]], +1, BASE_MOD);
      double thr = 0.15*(a + b*l.amp);
      if (thr < 20*MV2ADC || thr > 0.9*wall || h.amp < thr) continue;
      tc[j] = le(ch[HGs[j]], tx[1], +1, h.base, thr);
      if (tc[j] > -1e8) dt[sl][j].push_back(tc[j] - t1);
    }
    if (tc[0] > -1e8 && tc[1] > -1e8) dcc[sl].push_back(tc[0] - tc[1]);  // cap4 - cap5, MCP-free
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  printf("slice | width cap4 | cap5 | cap6 | cap7 | cap4-cap5 (MCP-free)  [ps]\n");
  for (int sl = 0; sl < NS; ++sl) {
    double rc, rw; printf("%5d |", sl);
    for (int j = 0; j < 4; ++j) { robust(dt[sl][j], rc, rw); printf(" %5.0f |", rw*1000); }
    robust(dcc[sl], rc, rw); printf(" %5.0f\n", rw*1000);
  }
  fflush(nullptr);
  gSystem->Exit(0);
}

// Light-leak vs gain-change discriminator:
//   per slice: (1) tagged-electron SumLG response scale (robust center),
//   (2) rate of discrete pulses in the record tail (samples 550-1000,
//       away from the shower), (3) their median amplitude (pe-gain proxy).
// Light leak: response flat, pe amplitude flat, pe rate explodes.
// Real gain change: response and pe amplitude rise together.
void DriftLeak()
{
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP];
  t->SetBranchStatus("*", 0); t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t N = t->GetEntries();
  const int NS = (int)(N / 2000);
  std::vector<double> eS[64], peA[64];
  long nPe[64] = {0}, nScan[64] = {0};
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    int sl = std::min((int)(i / 2000), NS - 1);
    // (1) electron response scale
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    if (c0.amp > 40 && c1.amp > 40) {
      double S = 0; for (int j = 0; j < 4; ++j) S += pf(ch[LGs[j]], +1, BASE_MOD).amp;
      if (S > 660 && S < 12000) eS[sl].push_back(S);
    }
    // (2,3) tail-pulse counting on HG channels, every 4th event
    if (i % 4) continue;
    ++nScan[sl];
    for (int j = 0; j < 4; ++j) {
      const float *w = ch[HGs[j]];
      double med = 0; for (int k = 550; k < 1000; ++k) med += w[k]; med /= 450;
      for (int k = 555; k < 995; ++k) {
        double v = (w[k] - med) * MV2ADC;
        if (v > 120 && w[k] > w[k-1] && w[k] > w[k-2] && w[k] >= w[k+1] && w[k] >= w[k+2]) {
          peA[sl].push_back(v); ++nPe[sl]; k += 10;   // skip past this pulse
        }
      }
    }
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  printf("slice | e-response SumLG | tail pulses/event | median tail-pulse amp [ADC-eq]\n");
  for (int sl = 0; sl < NS; ++sl) {
    double rc, rw; robust(eS[sl], rc, rw);
    double med = 0;
    if (peA[sl].size() > 10) { std::sort(peA[sl].begin(), peA[sl].end()); med = peA[sl][peA[sl].size()/2]; }
    printf("%5d | %8.0f (N=%2zu) | %6.3f | %6.0f\n", sl, rc, eS[sl].size(),
           nScan[sl] ? double(nPe[sl])/(4*nScan[sl]) : 0, med);
  }
  fflush(nullptr);
  gSystem->Exit(0);
}

// smoothed leading-edge crossing: 5-sample moving average tames pe noise
static double leSmooth(const float *w, const float *tx, int pol, double b, double thrA)
{
  static double sm[NSAMP];
  for (int s = 2; s < NSAMP-2; ++s)
    sm[s] = (w[s-2] + w[s-1] + w[s] + w[s+1] + w[s+2]) / 5.0;
  sm[0]=sm[1]=sm[2]; sm[NSAMP-1]=sm[NSAMP-2]=sm[NSAMP-3];
  double thr = pol > 0 ? b + thrA/MV2ADC : b - thrA/MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s)
    if (pol > 0 ? sm[s] >= thr : sm[s] <= thr) {
      double v0 = sm[s-1], v1 = sm[s]; if (v1 == v0) return tx[s];
      return tx[s-1] + (thr - v0)/(v1 - v0) * (tx[s] - tx[s-1]); }
  return -1e9;
}

// recovery test on the sunrise slices (run 29, entries >= 34000):
// standard srCFD vs smoothed-edge srCFD; also re-check a clean night window.
void DriftRecover()
{
  TFile *f = TFile::Open("data/DATA/RUN_LUAG_9GEV/run_29.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  const double a = 180, b = 2.59, wall = 3180;
  const char *lbl[2] = {"night ctrl (evts 4k-14k)", "sunrise (evts 34k-40k)"};
  Long64_t lo[2] = {4000, 34000}, hi[2] = {14000, 40000};
  for (int reg = 0; reg < 2; ++reg) {
    std::vector<double> dS, dM;   // standard, smoothed (median-combined caps)
    for (Long64_t i = lo[reg]; i < hi[reg]; ++i) {
      t->GetEntry(i);
      P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
      if (!(c0.amp > 40 && c1.amp > 40)) continue;
      double S = 0; for (int j = 0; j < 4; ++j) S += pf(ch[LGs[j]], +1, BASE_MOD).amp;
      if (S < 660) continue;
      P m1 = pf(ch[17], -1, BASE_MOD); if (m1.amp < 300) continue;
      double t1s = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
      double t1m = leSmooth(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
      if (t1s < -1e8 || t1m < -1e8) continue;
      std::vector<double> tcS, tcM;
      for (int j = 0; j < 4; ++j) {
        P l = pf(ch[LGs[j]], +1, BASE_MOD), h = pf(ch[HGs[j]], +1, BASE_MOD);
        double thr = 0.15*(a + b*l.amp);
        if (thr < 20*MV2ADC || thr > 0.9*wall || h.amp < thr) continue;
        double s1 = le(ch[HGs[j]], tx[1], +1, h.base, thr);
        double s2 = leSmooth(ch[HGs[j]], tx[1], +1, h.base, thr);
        if (s1 > -1e8) tcS.push_back(s1);
        if (s2 > -1e8) tcM.push_back(s2);
      }
      auto med = [](std::vector<double> &v){ std::sort(v.begin(), v.end()); return v[v.size()/2]; };
      if (tcS.size() >= 2) dS.push_back(med(tcS) - t1s);
      if (tcM.size() >= 2) dM.push_back(med(tcM) - t1m);
    }
    double rc, rw1, rw2;
    robust(dS, rc, rw1); robust(dM, rc, rw2);
    printf("%s: standard srCFD (median caps) = %.0f ps (N=%zu) | smoothed edge = %.0f ps (N=%zu)\n",
           lbl[reg], rw1*1000, dS.size(), rw2*1000, dM.size());
  }
  fflush(nullptr);
  gSystem->Exit(0);
}

// full 28+29 with per-event MEDIAN capillary combination — no DQ cuts.
void DriftMedianAll()
{
  TFile *f = TFile::Open("data/download/run_2829.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][NSAMP], tx[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tx);
  const Long64_t N = t->GetEntries();
  const double a = 180, b = 2.59, wall = 3180;
  std::vector<double> dt;
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    P c0 = pf(ch[0], -1, BASE_CTR), c1 = pf(ch[1], -1, BASE_CTR);
    if (!(c0.amp > 40 && c1.amp > 40)) continue;
    double S = 0; for (int j = 0; j < 4; ++j) S += pf(ch[LGs[j]], +1, BASE_MOD).amp;
    if (S < 660) continue;
    P m1 = pf(ch[17], -1, BASE_MOD); if (m1.amp < 300) continue;
    double t1 = le(ch[17], tx[1], -1, m1.base, 0.20*m1.amp);
    if (t1 < -1e8) continue;
    std::vector<double> tc;
    for (int j = 0; j < 4; ++j) {
      P l = pf(ch[LGs[j]], +1, BASE_MOD), h = pf(ch[HGs[j]], +1, BASE_MOD);
      double thr = 0.15*(a + b*l.amp);
      if (thr < 20*MV2ADC || thr > 0.9*wall || h.amp < thr) continue;
      double s1 = le(ch[HGs[j]], tx[1], +1, h.base, thr);
      if (s1 > -1e8) tc.push_back(s1);
    }
    if (tc.size() >= 2) { std::sort(tc.begin(), tc.end()); dt.push_back(tc[tc.size()/2] - t1); }
    if (i % 10000 == 0) printf("  %lld/%lld\n", i, N);
  }
  double rc, rw; robust(dt, rc, rw);
  printf("FULL 46k, median combination, no DQ cuts: N=%zu, shower-time sigma = %.0f ps\n",
         dt.size(), rw*1000);
  fflush(nullptr);
  gSystem->Exit(0);
}
