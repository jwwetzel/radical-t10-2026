// XCETTagProbe.C — validation suite for the tag-and-probe technique itself.
//
// Everything the pe calibration concludes rests on tag-and-probe. This macro
// measures each assumption instead of asserting it:
//
// 1. COUNTING BOOTSTRAP of the 1-pe gain ruler: on tagged electrons the probe
//    counter gives two observables — the zero fraction f0 and the mean
//    amplitude including zeros. Under Poisson statistics these are two
//    equations in two unknowns (lambda, A1pe):
//        f0   = e^-lam * (1-leak_up) + sum_n Pois(n|lam) * P(n-pe reads < zcut)
//        mean = lam * A1 + e^-lam * bPedG
//    with the n-pe response Gaussian of width sqrt(n) * r * A1 (r = sig1/A1 =
//    0.36 for BOTH PMTs from the anti-tag fits). Solved per run, this measures
//    A1pe from BEAM ELECTRONS ALONE — no spectral fit, no pedestal edge —
//    directly cross-checking XCET43's single-run pedestal-edge ruler.
//    The NAIVE version (global-window amplitude, calibration tag) FAILS, and
//    its failure modes are themselves measurements (kept in the summary):
//      (a) the min-of-1024-samples amplitude has an extreme-value noise
//          envelope that straddles the 25 ADC-eq zero cut (true zeros leak
//          OUT of the zero class — worst for XCET40's noisier channel);
//      (b) fake tags (dark pe in the tag counter over a non-radiating
//          particle) put hadrons in the probe sample that read zero (zeros
//          leak IN — worst for XCET43, which is tagged by XCET40 whose 1-pe
//          gain sits exactly at the 40 ADC-eq threshold).
//    The ROBUST version fixes both: prompt-window amplitude (+-35 samples
//    around the beam-synchronous pulse time — extreme-value bias collapses),
//    per-run zero cut at the measured 99th percentile of the prompt noise,
//    harder tag (max(60, cthr) ADC-eq), and explicit fake-tag subtraction
//    using the measured quiet-occupancy rates.
// 2. TAG-THRESHOLD SCAN: probe mean vs cthr, normalized to cthr=40. Flat =
//    pure + uncorrelated. The low-pressure climb measures fake-tag dilution
//    (+ correlation); the 0.59-bar flatness bounds intrinsic correlation.
// 3. PER-EVENT CORRELATION: Pearson r of (a40, a43), both fired — bounds the
//    independence assumption behind coincidence = product.
// 4. FAKE-TAG RATE: P(amp > thr | other counter quiet) — dark counts / CO2
//    scintillation above threshold; the tag-purity dilution driver.
//
// Usage: root -l -b -q 'macros/XCETTagProbe.C+'
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TF1.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TBox.h"
#include "TMath.h"
#include "TSystem.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const double ZCUT = 25.0;    // global-window zero/anti-tag cut (calibration definition)
static const double RSIG = 0.36;    // sig1pe/A1pe, identical for both PMTs (14.5/40.2, 7.9/21.8)
static const int PWIN = 35;         // prompt window half-width [samples] around the beam pulse time

static double ampOf(const float *w, int be, int lo, int hi, int *pos = nullptr)
{
  double b = 0; for (int k = 0; k < be; ++k) b += w[k]; b /= be;
  if (lo < 0) lo = 0; if (hi >= NSAMP) hi = NSAMP-1;
  float mn = w[lo]; int p = lo;
  for (int k = lo+1; k <= hi; ++k) if (w[k] < mn) { mn = w[k]; p = k; }
  if (pos) *pos = p;
  return (b - mn) * MV2ADC;
}

// P(observed amp < zcut) for (lambda, A1): zero class + smeared n-pe leakage
static double f0Model(double lam, double A1, double zcut, double r, double leakUp)
{
  double f = std::exp(-lam) * (1.0 - leakUp);
  for (int n = 1; n <= 80; ++n)
    f += TMath::PoissonI(n, lam) * 0.5 * TMath::Erfc((n*A1 - zcut)/(std::sqrt(2.0*n)*r*A1));
  return f;
}

static double lamOf(double f0, double A1, double zcut, double r, double leakUp)
{
  double lo = 0.005, hi = 16.0;
  for (int it = 0; it < 60; ++it) {
    double mid = 0.5*(lo+hi);
    if (f0Model(mid, A1, zcut, r, leakUp) > f0) lo = mid; else hi = mid;
  }
  return 0.5*(lo+hi);
}

// solve (lambda, A1) from (f0, meanG) by fixed-point iteration
static bool solveBoot(double f0, double meanG, double bPedG, double zcut, double r,
                      double leakUp, double A1seed, double &lam, double &A1)
{
  A1 = A1seed;
  for (int it = 0; it < 50; ++it) {
    lam = lamOf(f0, A1, zcut, r, leakUp);
    double A1n = (meanG - std::exp(-lam)*bPedG)/lam;
    if (A1n < 5 || A1n > 150) return false;
    if (std::fabs(A1n - A1) < 1e-4) { A1 = A1n; return true; }
    A1 = A1n;
  }
  return false;   // did not converge
}

static double quantile(std::vector<double> v, double q)
{
  if (v.empty()) return -1;
  std::sort(v.begin(), v.end());
  size_t i = (size_t)(q*(v.size()-1));
  return v[i];
}

void XCETTagProbe()
{
  SetRadStyle();
  gSystem->mkdir("Output/summary", true);
  FILE *sum = fopen("Output/summary/XCETTagProbe_summary.txt", "w");
  auto out = [&](const char *f, ...) { va_list a; va_start(a,f); vfprintf(stdout,f,a); va_end(a);
    va_start(a,f); vfprintf(sum,f,a); va_end(a); fflush(sum); };

  struct RunDef { int run; double p40, p43, cthr; };
  RunDef R[] = {
    {15,0.400,0.400,100},{24,1.300,1.300,100},{2526,1.300,1.300,100},{27,0.210,0.210,50},
    {9001,0.150,0.156,40},{30,0.062,0.060,40},{31,0.062,0.060,40},{32,0.149,0.146,40},
    {33,0.206,0.194,40},{35,1.313,1.303,100},{37,0.400,0.405,100},
    {38,0.590,0.553,100},{39,0.593,0.593,100},{40,0.593,0.593,100},{41,0.452,0.439,100},
    {42,0.225,0.220,40},{43,0.152,0.156,40},{44,0.090,0.088,40} };   // run 34 omitted (pion-contaminated tag)
  const int NR = sizeof(R)/sizeof(R[0]);
  const Long64_t MAXEV = 20000;
  const double A1ref[2] = {40.2, 21.8};   // spectral-fit rulers (anti-tag 1-pe)

  const int NT = 7; const double TSCAN[NT] = {30, 40, 60, 80, 100, 140, 200};

  double bootA1[NR][2], bootA1e[NR][2], bootA1s[NR][2], bootLam[NR][2], fakeF[NR][2];
  double f0obs[NR][2], meanObs[NR][2]; long nTag[NR][2], nBQ[NR];
  double qOcc[NR][2][3];   // off-time occupancy of counter c above {40,60,100} ADC-eq, scaled to 1024 samples
  double naiveF0[NR][2], meanRMS[NR][2];
  double naiveA1[NR][2];
  double scanMean[NR][2][NT]; long scanN[NR][2][NT];
  double pearson[NR]; long pearsonN[NR];
  double fake40cut[NR][2]; long quietN[NR][2];
  double zP[NR][2], envG[NR][2][2], envP[NR][2][2], bPedG[NR][2];
  for (int i = 0; i < NR; ++i) { pearson[i] = -2; pearsonN[i] = 0;
    for (int c = 0; c < 2; ++c) { bootA1[i][c] = -1; naiveA1[i][c] = -1; fake40cut[i][c] = -1;
      f0obs[i][c] = -1; meanObs[i][c] = -1; nTag[i][c] = 0; fakeF[i][c] = 0; nBQ[i] = 0;
      qOcc[i][c][0] = -1; qOcc[i][c][1] = -1; qOcc[i][c][2] = -1; naiveF0[i][c] = -1; meanRMS[i][c] = 0;
      quietN[i][c] = 0; zP[i][c] = -1; bPedG[i][c] = 0;
      envG[i][c][0] = -1; envG[i][c][1] = -1; envP[i][c][0] = -1; envP[i][c][1] = -1;
      for (int k = 0; k < NT; ++k) { scanMean[i][c][k] = 0; scanN[i][c][k] = 0; } } }

  for (int ir = 0; ir < NR; ++ir) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", R[ir].run));
    if (!f || f->IsZombie()) { out("# run %d missing\n", R[ir].run); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP];
    t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
    t->SetBranchAddress("channel", ch);
    Long64_t nEnt = std::min(t->GetEntries(), MAXEV);

    // pass 1: global amplitudes + pulse positions
    std::vector<double> aG[2]; std::vector<int> pG[2];
    for (int c = 0; c < 2; ++c) { aG[c].reserve(nEnt); pG[c].reserve(nEnt); }
    for (Long64_t i = 0; i < nEnt; ++i) {
      t->GetEntry(i);
      for (int c = 0; c < 2; ++c) {
        int p; aG[c].push_back(ampOf(ch[c], 200, 0, NSAMP-1, &p)); pG[c].push_back(p);
      }
    }
    int t0[2];
    for (int c = 0; c < 2; ++c) {
      std::vector<int> pv;
      for (size_t i = 0; i < aG[c].size(); ++i) if (aG[c][i] > 80) pv.push_back(pG[c][i]);
      if (pv.size() < 50) { t0[c] = -1; continue; }
      std::sort(pv.begin(), pv.end()); t0[c] = pv[pv.size()/2];
    }
    // pass 2: prompt-window amplitudes + off-time amplitudes (dark-rate control:
    // dark pulses are uniform in time, beam signals sit at t0 — an off-time
    // window measures the pure dark occupancy, free of beam/halo contamination)
    std::vector<double> aP[2], aO[2];
    for (int c = 0; c < 2; ++c) { aP[c].assign(nEnt, -1); aO[c].assign(nEnt, -1); }
    int oLo[2], oHi[2];
    for (int c = 0; c < 2; ++c) {
      if (t0[c] < 0) continue;
      if (t0[c] >= 340) { oLo[c] = t0[c]-300; oHi[c] = t0[c]-100; }   // pre-pulse preferred
      else              { oLo[c] = t0[c]+100; oHi[c] = t0[c]+300; }
      if (oHi[c] > NSAMP-1) oHi[c] = NSAMP-1;
    }
    if (t0[0] > 0 || t0[1] > 0)
      for (Long64_t i = 0; i < nEnt; ++i) {
        t->GetEntry(i);
        for (int c = 0; c < 2; ++c) if (t0[c] > 0) {
          aP[c][i] = ampOf(ch[c], 200, t0[c]-PWIN, t0[c]+PWIN);
          aO[c][i] = ampOf(ch[c], 200, oLo[c], oHi[c]);
        }
      }
    f->Close();

    // noise metrology from anti-tag (quiet other counter) events; the aG < 60
    // guard removes geometric-miss electrons that contaminate the "quiet" sample
    // at high pressure (seen as a real-signal tail in the run-39 XCET40 envelope)
    for (int c = 0; c < 2; ++c) {
      int o = 1 - c;
      std::vector<double> nG, nPr;
      for (Long64_t i = 0; i < nEnt; ++i)
        if (aG[o][i] < ZCUT) { nG.push_back(aG[c][i]);
          if (aP[c][i] >= 0 && aG[c][i] < 60) nPr.push_back(aP[c][i]); }
      envG[ir][c][0] = quantile(nG, 0.50); envG[ir][c][1] = quantile(nG, 0.99);
      envP[ir][c][0] = quantile(nPr, 0.50); envP[ir][c][1] = quantile(nPr, 0.99);
      zP[ir][c] = envP[ir][c][1];                       // per-run prompt zero cut = q99 of prompt noise
      double s = 0; long n = 0;                          // full-envelope global mean of true empties
      for (Long64_t i = 0; i < nEnt; ++i)
        if (aG[o][i] < ZCUT && aG[c][i] < 60 && aP[c][i] >= 0 && aP[c][i] < zP[ir][c]) { s += aG[c][i]; ++n; }
      bPedG[ir][c] = n > 20 ? s/n : 15.0;
    }

    { long nq = 0; for (Long64_t i = 0; i < nEnt; ++i) if (aG[0][i] < ZCUT && aG[1][i] < ZCUT) ++nq;
      nBQ[ir] = nq; }
    for (int c = 0; c < 2; ++c) {
      int o = 1 - c;
      double TAGB = std::max(60.0, R[ir].cthr);
      // occupancy of counter o above thresholds given a quiet counter c: at high
      // pressure this is the clean dark/afterglow rate; at low pressure it is
      // dominated by real (halo) electrons whose probe Poisson-zeroed
      long nQ = 0, nF40 = 0, nF60 = 0, nF100 = 0;
      for (Long64_t i = 0; i < nEnt; ++i) if (aG[c][i] < ZCUT) {
        ++nQ; if (aG[o][i] > R[ir].cthr) ++nF40;
        if (aG[o][i] > 60) ++nF60; if (aG[o][i] > 100) ++nF100;
      }
      fake40cut[ir][c] = nQ > 100 ? (double)nF40/nQ : -1; quietN[ir][c] = nQ;
      // off-time dark occupancy of counter c, scaled to the full 1024-sample window
      if (t0[c] > 0) {
        long nO = 0, o40 = 0, o60 = 0, o100 = 0;
        for (Long64_t i = 0; i < nEnt; ++i) if (aO[c][i] >= 0) {
          ++nO; if (aO[c][i] > 40) ++o40; if (aO[c][i] > 60) ++o60; if (aO[c][i] > 100) ++o100; }
        if (nO > 2000) {
          double sc = 1024.0/(oHi[c]-oLo[c]+1);
          qOcc[ir][c][0] = std::min(1.0, sc*o40/nO); qOcc[ir][c][1] = std::min(1.0, sc*o60/nO);
          qOcc[ir][c][2] = std::min(1.0, sc*o100/nO);
        }
      }

      // tag-threshold scan + naive bootstrap (calibration definitions, global window)
      long nTn = 0, nZn = 0; double sMn = 0;
      for (Long64_t i = 0; i < nEnt; ++i) {
        if (aG[o][i] > R[ir].cthr) { ++nTn; sMn += aG[c][i]; if (aG[c][i] < ZCUT) ++nZn; }
        for (int k = 0; k < NT; ++k)
          if (aG[o][i] > TSCAN[k]) { scanMean[ir][c][k] += aG[c][i]; ++scanN[ir][c][k]; }
      }
      if (nTn > 200) naiveF0[ir][c] = (double)nZn/nTn;
      if (nTn > 200 && nZn >= 8) {
        double lamN, A1n;
        if (solveBoot((double)nZn/nTn, sMn/nTn, bPedG[ir][c], ZCUT, RSIG, 0.0, A1ref[c], lamN, A1n))
          naiveA1[ir][c] = A1n;
      }

      // robust bootstrap observables: prompt zero class, hard PROMPT tag — a
      // dark fake must land inside the tag's 71-sample prompt window, not
      // anywhere in 1024 samples, cutting the fake rate by ~14x. (Solved after
      // the run loop, once the geometric-miss floor is calibrated from high P.)
      if (t0[c] < 0 || t0[o] < 0) continue;
      long nT = 0, nZ = 0; double sM = 0, sM2 = 0;
      for (Long64_t i = 0; i < nEnt; ++i)
        if (aP[o][i] > TAGB && aP[c][i] >= 0) { ++nT; sM += aG[c][i]; sM2 += aG[c][i]*aG[c][i]; if (aP[c][i] < zP[ir][c]) ++nZ; }
      if (nT < 120) continue;
      f0obs[ir][c] = (double)nZ/nT; meanObs[ir][c] = sM/nT; nTag[ir][c] = nT;
      meanRMS[ir][c] = std::sqrt(std::max(0.0, sM2/nT - (sM/nT)*(sM/nT)));
    }

    for (int c = 0; c < 2; ++c) for (int k = 0; k < NT; ++k)
      if (scanN[ir][c][k] > 100) scanMean[ir][c][k] /= scanN[ir][c][k]; else scanMean[ir][c][k] = -1;

    {   // Pearson, both fired
      double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0; long nC = 0;
      for (Long64_t i = 0; i < nEnt; ++i) if (aG[0][i] > ZCUT && aG[1][i] > ZCUT) {
        sx += aG[0][i]; sy += aG[1][i]; sxx += aG[0][i]*aG[0][i]; syy += aG[1][i]*aG[1][i];
        sxy += aG[0][i]*aG[1][i]; ++nC; }
      if (nC > 500) {
        double mx = sx/nC, my = sy/nC;
        double vx = sxx/nC - mx*mx, vy = syy/nC - my*my, cv = sxy/nC - mx*my;
        if (vx > 0 && vy > 0) { pearson[ir] = cv/std::sqrt(vx*vy); pearsonN[ir] = nC; }
      }
    }
    printf("run %d done (t0 %d/%d)\n", R[ir].run, t0[0], t0[1]);
  }

  // ---- geometric-miss floor, calibrated at high pressure ----
  // At 0.55-0.59 bar, lambda >= 10 so Poisson zeros are ~1e-5: any prompt-empty
  // probe events there are electrons that passed the tag counter and missed the
  // probe counter's acceptance (or readout losses). Median over runs 38/39/40.
  double fMiss[2] = {0, 0}, darkMed[2][3] = {{0,0,0},{0,0,0}};
  for (int c = 0; c < 2; ++c) {
    std::vector<double> v, d40, d60, d100;
    for (int i = 0; i < NR; ++i) {
      if ((R[i].run == 38 || R[i].run == 39 || R[i].run == 40) && f0obs[i][c] >= 0)
        v.push_back(f0obs[i][c]);
      if (qOcc[i][c][0] >= 0) { d40.push_back(qOcc[i][c][0]); d60.push_back(qOcc[i][c][1]); d100.push_back(qOcc[i][c][2]); }
    }
    fMiss[c] = v.empty() ? 0.005 : quantile(v, 0.5);
    darkMed[c][0] = d40.empty() ? 0.002 : quantile(d40, 0.5);   // off-time occupancy > 40 ADC-eq, per 1024 samples
    darkMed[c][1] = d60.empty() ? 0.001 : quantile(d60, 0.5);   // > 60
    darkMed[c][2] = d100.empty() ? 0.0005 : quantile(d100, 0.5); // > 100
  }
  // the off-time occupancy = dark + in-window PILEUP particles. Pileup lights
  // BOTH counters (a real electron) so it does not produce probe-empty fakes;
  // it is common to both channels and measured by XCET43's rate above 100
  // ADC-eq (~4.6 dark pe — negligible true dark). Subtract it to get dark-only.
  double pileup = std::min(darkMed[0][2], darkMed[1][2]);
  double darkOnly[2][3];
  for (int c = 0; c < 2; ++c) for (int tt = 0; tt < 3; ++tt)
    darkOnly[c][tt] = std::max(0.0, darkMed[c][tt] - pileup);
  // per-run fake fraction of the tag sample: dark rate of the TAG counter
  // (high-P calibrated, HV-stable) x the non-radiating population (both-quiet)
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c) {
    if (f0obs[i][c] < 0) continue;
    int o = 1 - c, tidx = (std::max(60.0, R[i].cthr) > 60) ? 2 : 1;
    fakeF[i][c] = std::min(0.5, darkOnly[o][tidx] * ((2.0*PWIN+1)/1024.0) * (double)nBQ[i] / nTag[i][c]);
  }

  // ---- robust bootstrap solve per run ----
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c) {
    if (f0obs[i][c] < 0) continue;
    double f0o = f0obs[i][c], meanG = meanObs[i][c], phiF = fakeF[i][c];
    long nT = nTag[i][c];
    auto solveCorr = [&](double f0x, double mx, double phix, double fM, double rx, double lux,
                         double &L, double &A) {
      double f0c = std::max(1e-4, ((f0x - phix)/(1 - phix) - fM)/(1 - fM));
      double mc  = ((mx - phix*bPedG[i][c])/(1 - phix) - fM*bPedG[i][c])/(1 - fM);
      return solveBoot(f0c, mc, bPedG[i][c], zP[i][c], rx, lux, A1ref[c], L, A);
    };
    double lam, A1;
    if (!solveCorr(f0o, meanG, phiF, fMiss[c], RSIG, 0.01, lam, A1)) continue;
    if (lam > 4.2) continue;   // validity: Poisson zeros must dominate the subtracted floors
    double sf0 = std::sqrt(f0o*(1-f0o)/nT), Lu, Au, Ld, Ad, Lm, Am;
    bool okU = solveCorr(std::min(0.99, f0o+sf0), meanG, phiF, fMiss[c], RSIG, 0.01, Lu, Au);
    bool okD = solveCorr(std::max(1e-4, f0o-sf0), meanG, phiF, fMiss[c], RSIG, 0.01, Ld, Ad);
    if (!okU || !okD) continue;
    double sMean = meanRMS[i][c]/std::sqrt((double)nT);
    bool okM = solveCorr(f0o, meanG + sMean, phiF, fMiss[c], RSIG, 0.01, Lm, Am);
    double eStat = std::sqrt(std::pow(0.5*std::fabs(Au - Ad), 2) + (okM ? std::pow(Am - A1, 2) : 0.0));
    double L1,A1a,L2,A1b,L3,A1c2,L4,A1d2,L5,A1e2,L6,A1f2,L7,A1g2;
    solveCorr(f0o, meanG, phiF, fMiss[c], 0.30, 0.01, L1, A1a);
    solveCorr(f0o, meanG, phiF, fMiss[c], 0.45, 0.01, L2, A1b);
    solveCorr(f0o, meanG, 0.0,  fMiss[c], RSIG, 0.01, L3, A1c2);
    solveCorr(f0o, meanG, std::min(0.7, 2*phiF), fMiss[c], RSIG, 0.01, L4, A1d2);
    solveCorr(f0o, meanG, phiF, fMiss[c], RSIG, 0.03, L5, A1e2);
    solveCorr(f0o, meanG, phiF, 0.5*fMiss[c], RSIG, 0.01, L6, A1f2);
    solveCorr(f0o, meanG, phiF, 1.5*fMiss[c], RSIG, 0.01, L7, A1g2);
    double sR = std::max(std::fabs(A1a-A1), std::fabs(A1b-A1));
    double sF = std::max(std::fabs(A1c2-A1), std::fabs(A1d2-A1));
    double sL = std::fabs(A1e2-A1);
    double sM2 = std::max(std::fabs(A1f2-A1), std::fabs(A1g2-A1));
    double eSys = std::sqrt(sR*sR + sF*sF + sL*sL + sM2*sM2);
    bootA1[i][c] = A1; bootA1e[i][c] = eStat; bootA1s[i][c] = eSys; bootLam[i][c] = lam;
  }

  // ---- report ----
  out("# TAG-AND-PROBE VALIDATION (XCETTagProbe.C)\n");
  out("# prompt window +-%d samples; zero cut = per-run q99 of prompt noise; tag = max(60, cthr); fake-tag subtracted\n\n", PWIN);

  out("## 0. amplitude-estimator noise metrology (anti-tag events)\n");
  out("# run  ctr   global q50/q99   prompt q50/q99   zero-cut  envelope mean (global, true empties)\n");
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c)
    if (quietN[i][c] > 0 && envG[i][c][1] > 0)
      out("%5d  %d   %5.1f / %5.1f     %5.1f / %5.1f     %5.1f     %5.1f\n", R[i].run, c?43:40,
          envG[i][c][0], envG[i][c][1], envP[i][c][0], envP[i][c][1], zP[i][c], bPedG[i][c]);

  out("\n## 1. counting bootstrap of the 1-pe ruler (beam electrons only; no spectral fit)\n");
  out("# geometric-miss floor (prompt-empty probe | tag, at 0.55-0.59 bar): XCET40 %.4f, XCET43 %.4f\n", fMiss[0], fMiss[1]);
  out("# off-time occupancy (all runs, scaled to 1024 samples): XCET40 >60/>100: %.4f/%.4f  XCET43: %.4f/%.4f\n",
      darkMed[0][1], darkMed[0][2], darkMed[1][1], darkMed[1][2]);
  out("# off-time occupancy > 40 ADC-eq: XCET40 %.4f  XCET43 %.4f\n", darkMed[0][0], darkMed[1][0]);
  out("# in-window pileup component (common, min of the two > 100 rates): %.4f -> dark-only >40/>60/>100: XCET40 %.4f/%.4f/%.4f  XCET43 %.4f/%.4f/%.4f\n",
      pileup, darkOnly[0][0], darkOnly[0][1], darkOnly[0][2], darkOnly[1][0], darkOnly[1][1], darkOnly[1][2]);
  out("# valid regime: lambda <= 4.2 (Poisson zeros must dominate the subtracted floors)\n");
  out("# run  ctr  P[bar]  fakeFrac  lambda   A1boot +/- stat +/- syst    naive-A1  (spectral: 40.2 / 21.8)\n");
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c)
    if (bootA1[i][c] > 0)
      out("%5d  %d  %.3f   %5.3f    %5.2f   %5.1f +/- %4.1f +/- %4.1f    %s\n", R[i].run, c?43:40,
          c?R[i].p43:R[i].p40, fakeF[i][c], bootLam[i][c], bootA1[i][c], bootA1e[i][c], bootA1s[i][c],
          naiveA1[i][c] > 0 ? Form("%5.1f", naiveA1[i][c]) : "  --  ");
  for (int c = 0; c < 2; ++c) {
    // stat-weighted mean; systematic variations are common-mode across runs
    // (one r, one dark calibration, one floor) so they are quoted separately
    double sw = 0, swx = 0; int nb = 0; std::vector<double> vs;
    for (int i = 0; i < NR; ++i) if (bootA1[i][c] > 0 && bootA1e[i][c] > 0) {
      double w = 1.0/(bootA1e[i][c]*bootA1e[i][c]); sw += w; swx += w*bootA1[i][c]; ++nb;
      vs.push_back(bootA1s[i][c]); }
    if (sw > 0) {
      double m = swx/sw, es = 1.0/std::sqrt(sw), chi2 = 0;
      for (int i = 0; i < NR; ++i) if (bootA1[i][c] > 0 && bootA1e[i][c] > 0)
        chi2 += std::pow((bootA1[i][c]-m)/bootA1e[i][c], 2);
      double sf = nb > 1 ? std::sqrt(std::max(1.0, chi2/(nb-1))) : 1.0;
      double esys = quantile(vs, 0.5);
      out("A1boot XCET%d = %.1f +/- %.1f (stat x%.2f, chi2/ndf %.1f/%d) +/- %.1f (common syst) ADC-eq (%d runs)  [spectral ruler: %.1f]\n",
          c?43:40, m, es*sf, sf, chi2, nb-1, esys, nb, A1ref[c]);
    }
  }
  // dilution closure: predicted global-tag (cthr) fake fraction per low-P run
  out("\n# dilution closure: predicted fake fraction of the CALIBRATION tag sample (dark-only occ x bothQuiet / Ntag40)\n");
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c) {
    if (R[i].cthr > 40 || scanN[i][c][1] < 100) continue;
    int o = 1 - c;
    double phi40 = darkOnly[o][0] * (double)nBQ[i] / scanN[i][c][1];
    out("%5d  XCET%d probe  P=%.3f  predicted fake frac = %.3f  (dark-only occ40[%d]=%.4f, Nbq=%ld, Ntag=%ld)\n",
        R[i].run, c?43:40, c?R[i].p43:R[i].p40, phi40, o?43:40, darkOnly[o][0], nBQ[i], scanN[i][c][1]);
  }
  out("\n# naive full-window empty-probe fraction (tag=cthr, global window) at 0.55-0.59 bar (traceability for the acceptance-floor claim)\n");
  for (int i = 0; i < NR; ++i)
    if (R[i].run == 38 || R[i].run == 39 || R[i].run == 40)
      for (int c = 0; c < 2; ++c) if (naiveF0[i][c] >= 0)
        out("%5d  XCET%d  naive f0 = %.4f   prompt-tag f0 = %.4f\n", R[i].run, c?43:40, naiveF0[i][c], f0obs[i][c] >= 0 ? f0obs[i][c] : -1.0);

  out("\n## 2. tag-threshold scan: probe mean vs cthr, normalized to cthr=40 (fake-dilution + correlation diagnostic)\n");
  out("# run  ctr  P[bar]   thr:  ");
  for (int k = 0; k < NT; ++k) out("%6.0f ", TSCAN[k]); out("\n");
  int scanRuns[] = {31, 30, 44, 9001, 39, 38};
  for (int s = 0; s < 6; ++s) for (int ir = 0; ir < NR; ++ir) if (R[ir].run == scanRuns[s])
    for (int c = 0; c < 2; ++c) {
      int kref = 1;
      if (scanMean[ir][c][kref] <= 0) continue;
      out("%5d  %d  %.3f   rel:  ", R[ir].run, c?43:40, c?R[ir].p43:R[ir].p40);
      for (int k = 0; k < NT; ++k)
        out(scanMean[ir][c][k] > 0 ? Form("%6.3f ", scanMean[ir][c][k]/scanMean[ir][c][kref]) : "   --  ");
      out("  (N40=%ld)\n", scanN[ir][c][kref]);
    }

  out("\n## 3. per-event Pearson correlation a40 vs a43 (both > %g)\n", ZCUT);
  for (int i = 0; i < NR; ++i) if (pearson[i] > -2)
    out("%5d  P=%.3f/%.3f  r = %+.3f  (N=%ld)\n", R[i].run, R[i].p40, R[i].p43, pearson[i], pearsonN[i]);

  out("\n## 4. occupancy of the tag counter above cthr given a quiet probe\n");
  out("# NOTE: only the 0.55-0.59 bar rows are pure dark rates. At low pressure this\n");
  out("# occupancy is dominated by REAL electrons (direct + soft halo) whose probe\n");
  out("# Poisson-zeroed. Halo electrons well above the counter's own threshold energy\n");
  out("# (~70 MeV at 0.06 bar; yield within 5%% of saturation above ~0.3 GeV) radiate\n");
  out("# like beam electrons and are valid calibration particles, not fakes.\n");
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c)
    if (fake40cut[i][c] >= 0)
      out("%5d  tag-for-XCET%d-probe  cthr=%3.0f  P(fake) = %.4f  (Nquiet=%ld)\n",
          R[i].run, c?43:40, R[i].cthr, fake40cut[i][c], quietN[i][c]);

  // ---- canvas ----
  TCanvas c2("c2","c2",2000,900); c2.Divide(2,1,0.004,0.004);
  int colc[2] = {rad::cTeal(), rad::cBlue()};
  c2.cd(1);
  TH1F *fr1 = gPad->DrawFrame(0.03, 0, 0.25, 60, ";XCET pressure [bar];bootstrap A1pe [ADC-eq]");
  for (int c = 0; c < 2; ++c) {
    double w = A1ref[c]*(c ? 0.15 : 0.05);
    TBox *b = new TBox(0.03, A1ref[c]-w, 0.25, A1ref[c]+w);
    b->SetFillColorAlpha(colc[c], 0.15); b->SetLineWidth(0); b->Draw();
    TLine *L = new TLine(0.03, A1ref[c], 0.25, A1ref[c]);
    L->SetLineColor(colc[c]); L->SetLineStyle(7); L->SetLineWidth(2); L->Draw();
  }
  TGraphErrors *gB[2];
  for (int c = 0; c < 2; ++c) {
    gB[c] = new TGraphErrors();
    for (int i = 0; i < NR; ++i) if (bootA1[i][c] > 0) {
      int n = gB[c]->GetN();
      gB[c]->SetPoint(n, (c?R[i].p43:R[i].p40) + (c?0.004:-0.004), bootA1[i][c]);
      gB[c]->SetPointError(n, 0, std::sqrt(bootA1e[i][c]*bootA1e[i][c] + bootA1s[i][c]*bootA1s[i][c]));
    }
    gB[c]->SetMarkerStyle(c?21:20); gB[c]->SetMarkerSize(1.3); gB[c]->SetMarkerColor(colc[c]); gB[c]->SetLineColor(colc[c]);
    gB[c]->Draw("P same");
  }
  TLegend *l1 = new TLegend(0.17,0.68,0.72,0.90); l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextFont(43); l1->SetTextSize(21);
  l1->AddEntry(gB[0], "XCET40 bootstrap (points) vs spectral ruler (band)", "pl");
  l1->AddEntry(gB[1], "XCET43 bootstrap (points) vs spectral ruler (band)", "pl");
  l1->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(26);
  tx.DrawLatex(0.14,0.94,"1-pe gain from counting alone (zero fraction #otimes mean) vs the spectral ruler");
  c2.cd(2);
  TH1F *fr2 = gPad->DrawFrame(20, 0.86, 210, 1.70, ";tag threshold [ADC-eq];probe mean, relative to tag threshold = 40 ADC-eq");
  TLine *one = new TLine(20, 1.0, 210, 1.0);   // unity = no tag-hardness dependence
  one->SetLineColor(kGray+1); one->SetLineStyle(3); one->Draw();
  int demo[2] = {44, 39};   // low-P (fake-diluted) and high-P (pure) demo runs
  TGraphErrors *gS[4]; int gi = 0;
  TLegend *l2 = new TLegend(0.17,0.64,0.62,0.90); l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextFont(43); l2->SetTextSize(20);
  for (int c = 0; c < 2; ++c) for (int hp = 0; hp < 2; ++hp) {
    int rr = demo[hp];
    int ir = -1; for (int i = 0; i < NR; ++i) if (R[i].run == rr) ir = i;
    if (ir < 0 || scanMean[ir][c][1] <= 0) continue;
    gS[gi] = new TGraphErrors();
    for (int k = 0; k < NT; ++k) if (scanMean[ir][c][k] > 0) {
      int n = gS[gi]->GetN();
      gS[gi]->SetPoint(n, TSCAN[k], scanMean[ir][c][k]/scanMean[ir][c][1]);
      gS[gi]->SetPointError(n, 0, scanMean[ir][c][k]/scanMean[ir][c][1]/std::sqrt((double)std::max(1L,scanN[ir][c][k])));
    }
    gS[gi]->SetMarkerStyle(hp ? (c?21:20) : (c?25:24)); gS[gi]->SetMarkerSize(1.2);
    gS[gi]->SetMarkerColor(colc[c]); gS[gi]->SetLineColor(colc[c]);
    gS[gi]->SetLineStyle(hp ? 1 : 7); gS[gi]->SetLineWidth(2);
    gS[gi]->Draw("PL same");
    l2->AddEntry(gS[gi], Form("XCET%d probe, run %d (%.2f bar)", c?43:40, rr, c?R[ir].p43:R[ir].p40), "pl");
    ++gi;
  }
  l2->Draw();
  tx.DrawLatex(0.14,0.94,"Does a harder tag inflate the probe? (flat = pure and uncorrelated)");
  c2.SaveAs("Output/summary/XCETTagProbe.png");
  fclose(sum);
  printf("Wrote Output/summary/XCETTagProbe.png + summary\n");
  gSystem->Exit(0);
}
