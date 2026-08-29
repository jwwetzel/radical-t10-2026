// Resolution.C — first-pass timing & energy resolution measurements, run 12.
//
// Method (per James, 2026-08-28):
//  - Electrons selected by Cherenkov coincidence (ch 0 & 1 amp > 150 ADC).
//  - Clipped high-gain pulses are NOT discarded: fit HG peak vs LG peak in
//    the linear (unclipped) region, infer the true HG amplitude from the LG
//    channel, and run a CFD on the intact HG rising edge with a threshold
//    at CFD_FRAC of the inferred amplitude.
//  - LG/HG pairing measured from amplitude correlations: 4-13, 5-12, 6-15, 7-14.
//  - Time axis: `times[group][sample]` (ns, per-event DRS4 cell calibration);
//    HG channels and MCP1 (ch 17) are group 1, MCP0 (ch 16) is group 0.
//
// Measurements:
//   1. MCP0-MCP1 time difference (reference timing floor, incl. inter-group)
//   2. per-capillary HG CFD time vs MCP1 (same group)  -> timing resolution
//   3. electron energy spectra: sum of 4 LG peaks, sum of 4 inferred HG peaks
//   4. MIP peaks per HG channel from the off-coincidence (hadron/muon) sample
//   5. split-half stability of every headline number (statistics check)
//
// Usage:  root -l -b -q 'macros/Resolution.C+(12)'
// Writes Output/run_<N>/Resolution_*.png, Resolution.root, Resolution_summary.txt

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLine.h"
#include "TSystem.h"
#include "TProfile.h"
#include "TMath.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include "radStyle.h"

static const int NSAMP = 1024;
static const int BASE_CTR = 200;   // baseline window for beam counters
static const int BASE_UP  = 80;    // baseline window for module + MCPs
static const float CHER_THR = 150.0f;
static const float HG_LIN_MAX = 1700.0f; // HG amp below this: linear region
static const float LG_MIN_FIT = 30.0f;
static const float LG_MAX_FIT = 400.0f;  // HG rolls off above this (approaching clip)
static const double CFD_FRAC = 0.30;
static const float ADC_RAIL_HI = 4090.0f, ADC_RAIL_LO = 5.0f;

static const int LG[4] = {4, 5, 6, 7};
static const int HG[4] = {13, 12, 15, 14};  // HG[i] pairs with LG[i]

struct ChInfo { double base, amp; int pkS; bool sat; };

static ChInfo chAmp(const float *w, int pol, int baseEnd)
{
  ChInfo r; r.base = 0;
  for (int s = 0; s < baseEnd; ++s) r.base += w[s];
  r.base /= baseEnd;
  float pkV = w[0]; r.pkS = 0; r.sat = false;
  for (int s = 0; s < NSAMP; ++s) {
    float v = w[s];
    if (pol > 0 ? (v > pkV) : (v < pkV)) { pkV = v; r.pkS = s; }
    if (v >= ADC_RAIL_HI || v <= ADC_RAIL_LO) r.sat = true;
  }
  r.amp = pol > 0 ? (pkV - r.base) : (r.base - pkV);
  return r;
}

// CFD on the rising edge: crossing of base +/- frac*amp before the peak.
// For clipped pulses pass the *inferred* amplitude; the rising edge is intact.
// Returns time in ns from the group time axis, or -1 if no crossing found.
static double cfdTime(const float *w, const float *tax, int pol, double base,
                      double amp, int pkS, double frac)
{
  const double thr = pol > 0 ? base + frac * amp : base - frac * amp;
  // find first sample at/beyond threshold walking back from the peak
  int s = pkS;
  while (s > 0 && (pol > 0 ? w[s - 1] >= thr : w[s - 1] <= thr)) --s;
  if (s == 0) return -1;
  const double v0 = w[s - 1], v1 = w[s];
  if (v1 == v0) return tax[s];
  const double f = (thr - v0) / (v1 - v0);
  return tax[s - 1] + f * (tax[s] - tax[s - 1]);
}

// iterative Gaussian core fit, seeded from the histogram peak (robust
// against wide tails): mean = max bin, sigma = FWHM/2.35, 3 passes at +/-2 sigma
static TF1 *coreFit(TH1F *h, const char *name)
{
  const int pb = h->GetMaximumBin();
  double m = h->GetBinCenter(pb), pk = h->GetBinContent(pb);
  int lo = pb, hi = pb;
  while (lo > 1 && h->GetBinContent(lo) > pk / 2) --lo;
  while (hi < h->GetNbinsX() && h->GetBinContent(hi) > pk / 2) ++hi;
  double s = std::max((h->GetBinCenter(hi) - h->GetBinCenter(lo)) / 2.355,
                      h->GetBinWidth(1));
  TF1 *g = new TF1(name, "gaus", m - 2 * s, m + 2 * s);
  g->SetParameters(pk, m, s);
  for (int it = 0; it < 3; ++it) {
    h->Fit(g, "QNR", "", m - 2 * s, m + 2 * s);
    m = g->GetParameter(1); s = std::fabs(g->GetParameter(2));
  }
  h->Fit(g, "QR", "", m - 2 * s, m + 2 * s);
  return g;
}

void Resolution(int run = 12, Long64_t nMax = -1)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(TString::Format("data/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("no data file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[18][NSAMP], tax[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tax);
  const Long64_t nEnt = (nMax > 0 && nMax < t->GetEntries()) ? nMax : t->GetEntries();

  FILE *sum = fopen(outDir + "/Resolution_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };
  out("Resolution.C run %d, %lld events; CFD fraction %.2f; pairing LG->HG: ", run, nEnt, CFD_FRAC);
  for (int i = 0; i < 4; ++i) out("%d->%d ", LG[i], HG[i]);
  out("\n\n");

  TFile *fout = new TFile(outDir + "/Resolution.root", "RECREATE");

  // ---------------- pass 1: amplitudes ----------------
  std::vector<char>  isBeam(nEnt);
  std::vector<float> lgA(4 * nEnt), hgA(4 * nEnt);
  std::vector<char>  hgSat(4 * nEnt);
  TH2F *hHL[4];
  for (int i = 0; i < 4; ++i)
    hHL[i] = new TH2F(Form("hHL_%d", i),
      Form("HG ch %d vs LG ch %d (beam);LG peak [ADC];HG peak [ADC]", HG[i], LG[i]),
      120, 0, 1200, 120, 0, 2400);
  TH1F *hMIP[4];
  for (int i = 0; i < 4; ++i)
    hMIP[i] = new TH1F(Form("hMIP_%d", i),
      Form("HG ch %d off-coincidence;HG peak [ADC];events", HG[i]), 200, 0, 2000);

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    ChInfo c0 = chAmp(ch[0], -1, BASE_CTR), c1 = chAmp(ch[1], -1, BASE_CTR);
    const bool beam = (c0.amp > CHER_THR && c1.amp > CHER_THR);
    isBeam[i] = beam;
    for (int j = 0; j < 4; ++j) {
      ChInfo l = chAmp(ch[LG[j]], +1, BASE_UP);
      ChInfo h = chAmp(ch[HG[j]], +1, BASE_UP);
      lgA[4 * i + j] = l.amp; hgA[4 * i + j] = h.amp; hgSat[4 * i + j] = h.sat;
      if (beam && !h.sat && h.amp < HG_LIN_MAX) hHL[j]->Fill(l.amp, h.amp);
      if (!beam) hMIP[j]->Fill(h.amp);
    }
    if (i % 5000 == 0) printf("  pass1 %lld / %lld\n", i, nEnt);
  }

  // LG -> HG transfer lines from the unclipped linear region
  double p0[4], p1[4];
  out("LG->HG transfer fits (linear region, HG < %.0f ADC):\n", HG_LIN_MAX);
  TCanvas cHL("cHL", "cHL", 1600, 1200); cHL.Divide(2, 2);
  for (int j = 0; j < 4; ++j) {
    cHL.cd(j + 1); gPad->SetLogz();
    TF1 *fl = new TF1(Form("fl%d", j), "pol1", LG_MIN_FIT, LG_MAX_FIT);
    hHL[j]->Draw("colz");
    // fit the profile of the 2D to reduce outlier pull
    TProfile *pr = hHL[j]->ProfileX(Form("prHL_%d", j));
    pr->Fit(fl, "QR", "", LG_MIN_FIT, LG_MAX_FIT);
    fl->SetRange(LG_MIN_FIT, 1200);  // draw the extrapolation used for recovery
    pr->SetLineColor(kBlack); pr->Draw("same");
    fl->SetLineColor(kRed); fl->Draw("same");
    p0[j] = fl->GetParameter(0); p1[j] = fl->GetParameter(1);
    out("  LG%d -> HG%d :  HG = %7.2f + %6.3f * LG   (HG/LG gain ratio %.2f)\n",
        LG[j], HG[j], p0[j], p1[j], p1[j]);
  }
  cHL.SaveAs(outDir + "/Resolution_HGvsLG.png");
  out("\n");

  // ---------------- pass 2: timing + spectra ----------------
  TH1F *hMM   = new TH1F("hMM", "MCP0 - MCP1;#Deltat [ns];events", 1200, -3, 3);
  TH1F *hTC[4], *hTC_h1[4], *hTC_h2[4];
  for (int j = 0; j < 4; ++j) {
    hTC[j] = new TH1F(Form("hTC_%d", j),
      Form("HG ch %d CFD - MCP1;#Deltat [ns];events", HG[j]), 500, -10, 10);
    hTC_h1[j] = (TH1F*)hTC[j]->Clone(Form("hTC_%d_h1", j));
    hTC_h2[j] = (TH1F*)hTC[j]->Clone(Form("hTC_%d_h2", j));
  }
  TH1F *hMM_h1 = (TH1F*)hMM->Clone("hMM_h1"), *hMM_h2 = (TH1F*)hMM->Clone("hMM_h2");
  TH1F *hSumLG = new TH1F("hSumLG", "electrons: #Sigma LG peaks;#Sigma LG [ADC];events", 90, 0, 4500);
  TH1F *hCC = new TH1F("hCC", "HG ch 12 - HG ch 13 (both > 500 ADC);#Deltat [ns];events", 500, -10, 10);
  const double abEdge[5] = {300, 600, 1200, 2400, 8000};
  TH1F *hAB[4];
  for (int b = 0; b < 4; ++b)
    hAB[b] = new TH1F(Form("hAB_%d", b),
      Form("all HG - MCP1, A_{inf} %d-%d ADC;#Deltat [ns];events", (int)abEdge[b], (int)abEdge[b+1]),
      500, -10, 10);
  TH1F *hSumHG = new TH1F("hSumHG", "electrons: #Sigma inferred HG peaks;#Sigma HG_{inf} [ADC];events", 150, 0, 12000);
  TH1F *hSumLG_h1 = (TH1F*)hSumLG->Clone("hSumLG_h1"), *hSumLG_h2 = (TH1F*)hSumLG->Clone("hSumLG_h2");
  long nClipUsed = 0, nBeam = 0;

  for (Long64_t i = 0; i < nEnt; ++i) {
    if (!isBeam[i]) continue;
    t->GetEntry(i);
    ++nBeam;
    const bool half1 = (nBeam % 2 == 0);

    // MCPs: ch16 group 0, ch17 group 1
    ChInfo m0 = chAmp(ch[16], -1, BASE_UP), m1 = chAmp(ch[17], -1, BASE_UP);
    double t0 = cfdTime(ch[16], tax[0], -1, m0.base, m0.amp, m0.pkS, CFD_FRAC);
    double t1 = cfdTime(ch[17], tax[1], -1, m1.base, m1.amp, m1.pkS, CFD_FRAC);
    if (m0.amp > 300 && m1.amp > 300 && t0 > 0 && t1 > 0) {
      hMM->Fill(t0 - t1);
      (half1 ? hMM_h1 : hMM_h2)->Fill(t0 - t1);
    }

    double sumLG = 0, sumHG = 0;
    double tcap[4] = {-1, -1, -1, -1}, ainf[4] = {0, 0, 0, 0};
    for (int j = 0; j < 4; ++j) {
      float l = lgA[4 * i + j], h = hgA[4 * i + j];
      bool clipped = hgSat[4 * i + j];
      double hInf = clipped ? p0[j] + p1[j] * l : h;
      sumLG += l; sumHG += hInf; ainf[j] = hInf;
      if (clipped) ++nClipUsed;
      // capillary timing vs MCP1 (same DRS group): CFD with inferred amplitude
      if (hInf > 300) {
        ChInfo hg = chAmp(ch[HG[j]], +1, BASE_UP);
        int pk = hg.pkS;
        if (clipped) { pk = 0; while (pk < NSAMP && ch[HG[j]][pk] < ADC_RAIL_HI) ++pk; }
        double tc_ = cfdTime(ch[HG[j]], tax[1], +1, hg.base, hInf, pk, CFD_FRAC);
        tcap[j] = tc_;
        if (tc_ > 0 && m1.amp > 300 && t1 > 0) {
          hTC[j]->Fill(tc_ - t1); (half1 ? hTC_h1[j] : hTC_h2[j])->Fill(tc_ - t1);
          for (int b = 0; b < 4; ++b)
            if (hInf >= abEdge[b] && hInf < abEdge[b + 1]) hAB[b]->Fill(tc_ - t1);
        }
      }
    }
    // capillary-capillary time difference (HG 12 = j1, HG 13 = j0): MCP-free
    if (tcap[0] > 0 && tcap[1] > 0 && ainf[0] > 500 && ainf[1] > 500)
      hCC->Fill(tcap[1] - tcap[0]);
    hSumLG->Fill(sumLG); hSumHG->Fill(sumHG);
    (half1 ? hSumLG_h1 : hSumLG_h2)->Fill(sumLG);
    if (i % 5000 == 0) printf("  pass2 %lld / %lld\n", i, nEnt);
  }

  out("Beam (Cherenkov) events: %ld;  clipped HG pulses recovered via LG inference: %ld\n\n", nBeam, nClipUsed);

  // ---------------- fits & report ----------------
  out("=== TIMING ===\n");
  TCanvas cT("cT", "cT", 1800, 1000); cT.Divide(3, 2);
  cT.cd(1); TF1 *gMM = coreFit(hMM, "gMM"); hMM->Draw("hist"); gMM->Draw("same");
  out("MCP0-MCP1 (cross-group): sigma = %.1f +/- %.1f ps  (N=%.0f)  => single-MCP+intergroup %.1f ps\n",
      1e3 * gMM->GetParameter(2), 1e3 * gMM->GetParError(2), hMM->GetEntries(),
      1e3 * gMM->GetParameter(2) / std::sqrt(2.));
  for (int j = 0; j < 4; ++j) {
    cT.cd(j + 2);
    TF1 *g = coreFit(hTC[j], Form("gTC%d", j));
    hTC[j]->Draw("hist"); g->Draw("same");
    TF1 *g1 = coreFit(hTC_h1[j], Form("gTC%dh1", j));
    TF1 *g2 = coreFit(hTC_h2[j], Form("gTC%dh2", j));
    out("HG ch %2d - MCP1: sigma = %6.1f +/- %4.1f ps  (N=%5.0f)   halves: %5.1f+/-%4.1f | %5.1f+/-%4.1f ps\n",
        HG[j], 1e3 * g->GetParameter(2), 1e3 * g->GetParError(2), hTC[j]->GetEntries(),
        1e3 * g1->GetParameter(2), 1e3 * g1->GetParError(2),
        1e3 * g2->GetParameter(2), 1e3 * g2->GetParError(2));
  }
  {
    cT.cd(6);
    TF1 *gm1 = coreFit(hMM_h1, "gMMh1"); TF1 *gm2 = coreFit(hMM_h2, "gMMh2");
    hMM_h1->SetLineColor(kRed); hMM_h2->SetLineColor(kBlue);
    hMM_h1->Draw("hist"); hMM_h2->Draw("hist same");
    out("MCP0-MCP1 halves: %.1f+/-%.1f | %.1f+/-%.1f ps\n",
        1e3 * gm1->GetParameter(2), 1e3 * gm1->GetParError(2),
        1e3 * gm2->GetParameter(2), 1e3 * gm2->GetParError(2));
  }
  cT.SaveAs(outDir + "/Resolution_timing.png");

  TCanvas cT2("cT2", "cT2", 1800, 1000); cT2.Divide(3, 2);
  cT2.cd(1);
  TF1 *gCC = coreFit(hCC, "gCC"); hCC->Draw("hist"); gCC->Draw("same");
  out("HG12-HG13 (MCP-free, both > 500 ADC): sigma = %.1f +/- %.1f ps (N=%.0f) => per-capillary %.1f ps\n",
      1e3 * gCC->GetParameter(2), 1e3 * gCC->GetParError(2), hCC->GetEntries(),
      1e3 * gCC->GetParameter(2) / std::sqrt(2.));
  out("all-HG vs MCP1 in amplitude bins:\n");
  for (int b = 0; b < 4; ++b) {
    cT2.cd(b + 2);
    TF1 *g = coreFit(hAB[b], Form("gAB%d", b));
    hAB[b]->Draw("hist"); g->Draw("same");
    out("  A_inf %4d-%4d ADC: sigma = %6.1f +/- %5.1f ps  (N=%5.0f)\n",
        (int)abEdge[b], (int)abEdge[b + 1],
        1e3 * g->GetParameter(2), 1e3 * g->GetParError(2), hAB[b]->GetEntries());
  }
  cT2.SaveAs(outDir + "/Resolution_timing2.png");

  out("\n=== ENERGY RESPONSE (electrons, 5 GeV) ===\n");
  // No Gaussian peak exists: the beam spot is larger than the module, so the
  // response is a position-driven continuum ending at a sharp direct-hit
  // edge. Quote the edge position and width from an erfc fit; the relative
  // edge width is an UPPER BOUND on the energy resolution at full response
  // (it still contains the position falloff and beam momentum spread).
  long nMiss = 0;
  for (int b = 1; b <= hSumLG->FindBin(299.9); ++b) nMiss += (long)hSumLG->GetBinContent(b);
  out("module-miss fraction (Sigma LG < 300 ADC): %.1f%% of electrons\n",
      100.0 * nMiss / hSumLG->GetEntries());

  auto edgeFit = [&](TH1F *h, const char *nm) {
    TF1 *fe = new TF1(nm, "[0]*TMath::Erfc((x-[1])/(1.41421356*[2]))", 1800, 4200);
    fe->SetParameters(h->GetBinContent(h->FindBin(2200)), 3000, 200);
    h->Fit(fe, "QR");
    return fe;
  };
  TCanvas cE("cE", "cE", 1600, 600); cE.Divide(2, 1);
  cE.cd(1); hSumLG->Draw("hist");
  TF1 *feL = edgeFit(hSumLG, "feL"); feL->Draw("same");
  cE.cd(2); hSumHG->Draw("hist");
  double eE  = feL->GetParameter(1), eEe = feL->GetParError(1);
  double eW  = std::fabs(feL->GetParameter(2)), eWe = feL->GetParError(2);
  double rE  = eW / eE;
  double rEe = rE * std::sqrt(std::pow(eWe / eW, 2) + std::pow(eEe / eE, 2));
  out("Sum LG direct-hit edge: position %6.1f +/- %4.1f ADC, width %5.1f +/- %4.1f ADC\n", eE, eEe, eW, eWe);
  out("  => edge width / position = %.2f%% +/- %.2f%%  (upper bound on resolution at full response)\n",
      100 * rE, 100 * rEe);
  hSumLG_h1->Rebin(2); hSumLG_h2->Rebin(2);
  TF1 *feh1 = edgeFit(hSumLG_h1, "feh1");
  TF1 *feh2 = edgeFit(hSumLG_h2, "feh2");
  out("  halves: width/pos = %.2f%% +/- %.2f%%  |  %.2f%% +/- %.2f%%\n",
      100 * std::fabs(feh1->GetParameter(2)) / feh1->GetParameter(1),
      100 * std::fabs(feh1->GetParameter(2)) / feh1->GetParameter(1) *
        std::sqrt(std::pow(feh1->GetParError(2) / feh1->GetParameter(2), 2) +
                  std::pow(feh1->GetParError(1) / feh1->GetParameter(1), 2)),
      100 * std::fabs(feh2->GetParameter(2)) / feh2->GetParameter(1),
      100 * std::fabs(feh2->GetParameter(2)) / feh2->GetParameter(1) *
        std::sqrt(std::pow(feh2->GetParError(2) / feh2->GetParameter(2), 2) +
                  std::pow(feh2->GetParError(1) / feh2->GetParameter(1), 2)));
  cE.SaveAs(outDir + "/Resolution_energy.png");

  out("\n=== MIP (off-coincidence sample, HG channels) ===\n");
  TCanvas cM("cM", "cM", 1600, 1200); cM.Divide(2, 2);
  for (int j = 0; j < 4; ++j) {
    cM.cd(j + 1); gPad->SetLogy();
    hMIP[j]->Draw("hist");
    int pkBin = 0; double pkVal = 0;   // peak above noise region
    for (int b = hMIP[j]->FindBin(120); b <= hMIP[j]->GetNbinsX(); ++b)
      if (hMIP[j]->GetBinContent(b) > pkVal) { pkVal = hMIP[j]->GetBinContent(b); pkBin = b; }
    double pk = hMIP[j]->GetBinCenter(pkBin);
    TF1 *lan = new TF1(Form("lan%d", j), "landau", 0.55 * pk, 2.5 * pk);
    hMIP[j]->Fit(lan, "QR");
    lan->Draw("same");
    out("HG ch %2d: MIP MPV = %6.1f +/- %4.1f ADC\n", HG[j],
        lan->GetParameter(1), lan->GetParError(1));
  }
  cM.SaveAs(outDir + "/Resolution_MIP.png");

  fout->Write(); fout->Close();
  out("\nWrote %s/Resolution_{HGvsLG,timing,energy,MIP}.png, Resolution.root\n", outDir.Data());
  fclose(sum);
}
