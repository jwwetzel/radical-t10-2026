// SaturationStudy.C — is the high-energy response flattening real?
//
// Three suspects for the -14% (11 GeV) / -3% (9 GeV) deviation from linear:
//   (a) transverse leakage: the 11 GeV beam is wide (T10 momentum limit) —
//       off-center impacts leak laterally and pull the fitted peak down;
//   (b) cross-run gain systematics: raw ADC-eq comparison across runs on the
//       day-scale gain ladder, with a MIP estimator known to inflate under
//       daytime noise (max-over-full-window picks up the noise envelope);
//   (c) real physics: shower-max migration (~ln E) past the fixed sampling
//       depth, and (unlikely at ~3 MIP/capillary) SiPM pixel saturation.
//
// Tests, one canvas:
//   1. run 30 SumLG spectrum: all on-module vs centered subset (balance cut)
//   2. peak vs centrality r for 3/5/7/9/11 GeV, normalized to centered bin
//      (run 15 = tight-beam control: must be flat)
//   3. robust MIP ladder: windowed+smoothed amplitude estimator vs the old
//      full-window one, per run — the honest gain-normalization factors
//   4. response vs E: scan-raw | centered | centered + gain-corrected,
//      against a linear reference through the corrected 5 GeV point
//
// Usage: root -l -b -q 'macros/SaturationStudy.C+'
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLine.h"
#include "TLatex.h"
#include "TSystem.h"
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15}; // 4 TL, 5 TR, 6 BL, 7 BR

static double ampFull(const float *w, int pol, int baseEnd)  // as EnergyScan: full-window extremum
{
  double b = 0; for (int s = 0; s < baseEnd; ++s) b += w[s]; b /= baseEnd;
  float ex = w[0];
  for (int s = 1; s < NSAMP; ++s) if (pol > 0 ? w[s] > ex : w[s] < ex) ex = w[s];
  return (pol > 0 ? ex - b : b - ex) * MV2ADC;
}
static int pkSample(const float *w, int pol, int baseEnd)
{
  double b = 0; for (int s = 0; s < baseEnd; ++s) b += w[s]; b /= baseEnd;
  float ex = w[0]; int ps = 0;
  for (int s = 1; s < NSAMP; ++s) if (pol > 0 ? w[s] > ex : w[s] < ex) { ex = w[s]; ps = s; }
  return ps;
}
// windowed + 3-sample smoothed amplitude: immune to the max-over-1024 noise
// envelope that inflates the naive MIP MPV under daytime noise
static double ampWin(const float *w, int pol, int baseEnd, int p0, int p1)
{
  double b = 0; for (int s = 0; s < baseEnd; ++s) b += w[s]; b /= baseEnd;
  if (p0 < baseEnd + 1) p0 = baseEnd + 1;
  if (p1 > NSAMP - 2) p1 = NSAMP - 2;
  double ex = -1e30;
  for (int s = p0; s <= p1; ++s) {
    double v = (w[s-1] + w[s] + w[s+1]) / 3.0;
    double a = pol > 0 ? v - b : b - v;
    if (a > ex) ex = a;
  }
  return ex * MV2ADC;
}
// iterative Gaussian core fit above a floor; returns false if too few events
static bool corePeak(TH1F *h, double floorS, double &m, double &me, double &sg, double &sge)
{
  if (h->Integral(h->FindBin(floorS), h->GetNbinsX()) < 40) return false;
  int pb = h->FindBin(floorS); double pv = 0; int pbb = pb;
  for (int b = pb; b <= h->GetNbinsX(); ++b)
    if (h->GetBinContent(b) > pv) { pv = h->GetBinContent(b); pbb = b; }
  m = h->GetBinCenter(pbb); double s = 900;
  TF1 g("gcs","gaus", floorS, 10500);
  g.SetParameters(pv, m, s);
  g.SetParLimits(1, floorS + 30, 10200);
  g.SetParLimits(2, 25, 2500);
  for (int it = 0; it < 4; ++it) {
    h->Fit(&g, "QRN", "", std::max(floorS, m - 1.6*s), m + 1.6*s);
    m = g.GetParameter(1); s = std::fabs(g.GetParameter(2));
  }
  me = g.GetParError(1); sg = s; sge = g.GetParError(2);
  return true;
}
// Landau fit around the mode (range explicit, pedestal excluded)
static bool mipFit(TH1F *h, double &mpv, double &mpve)
{
  if (h->GetEntries() < 300) return false;
  int b0 = h->FindBin(150); double pv = 0; int pbb = b0;
  for (int b = b0; b <= h->FindBin(1500); ++b)
    if (h->GetBinContent(b) > pv) { pv = h->GetBinContent(b); pbb = b; }
  double mode = h->GetBinCenter(pbb);
  TF1 L("Lmip","landau", 150, 3000);
  L.SetParameters(pv, mode, 0.15*mode);
  for (int it = 0; it < 2; ++it) {
    h->Fit(&L, "QRN", "", 0.55*mode, 3.0*mode);
    mode = L.GetParameter(1);
  }
  mpv = L.GetParameter(1); mpve = L.GetParError(1);
  return true;
}

struct Ev { double S, r; };

// Longo profile: fraction of E deposited in a fixed window centered on the
// 5 GeV shower max; returns S(E)/(linear through 5 GeV) — the calculable
// shower-max-migration nonlinearity of fixed-depth sampling
static double longoFactor(double E_GeV, double thick = 1.5, double b = 0.5, double Ec_MeV = 10.0)
{
  auto frac = [&](double E) {
    double tmax = std::log(E*1000.0/Ec_MeV) - 0.5, a = b*tmax + 1.0;
    double t1 = (std::log(5000.0/Ec_MeV) - 0.5) - thick/2, t2 = t1 + thick;
    double sum = 0; const int n = 200;
    for (int i = 0; i < n; ++i) {
      double t = t1 + (t2-t1)*(i+0.5)/n;
      sum += std::exp((a-1)*std::log(b*t) - b*t + std::log(b) - std::lgamma(a));
    }
    return sum*(t2-t1)/n;
  };
  return frac(E_GeV)/frac(5.0);
}

void SaturationStudy()
{
  SetRadStyle();
  const int NR = 5;
  int    runs[NR]  = {24, 15, 27, 9001, 30};
  double E[NR]     = {3, 5, 7, 9, 11};
  double cthr[NR]  = {100, 100, 50, 40, 40};
  double floorRaw[NR]  = {300, 420, 540, 660, 780};    // miss/noise floor (EnergyScan formula)
  double floorPos[NR]  = {800, 1200, 1700, 2200, 2500};  // balance-validity floor (~40% of peak): junk has 4 noise-level amps, balance ~0, and would pile into the CENTRAL bin
  double floorScan[NR] = {300, 420, 540, 660, 3800};   // what the scan table uses (11 GeV containment)
  int    cols[NR]  = {rad::cAmber(), rad::cRed(), rad::cBlue(), rad::cInk(), rad::cViolet()};

  const int NB = 4;                                     // centrality bins in r = |balance|
  double rEdge[NB+1] = {0, 0.18, 0.35, 0.60, 1.01};

  FILE *sum = fopen("Output/saturation/SaturationStudy_summary.txt", "w");
  auto out = [&](const char *f, ...) {
    va_list a; va_start(a, f); vfprintf(stdout, f, a); va_end(a);
    va_start(a, f); vfprintf(sum, f, a); va_end(a); };

  TH1F *hAll[NR], *hCent[NR], *hRB[NR][NB], *hMw[NR][4], *hMf[NR][4];
  double pkScan[NR], pkScanE[NR], pkCent[NR], pkCentE[NR];
  double pkR[NR][NB], pkRE[NR][NB]; long nRB[NR][NB];
  double mpvW[NR][4], mpvWe[NR][4], mpvF[NR][4], mpvFe[NR][4];
  double share[NR][4]; long nShare[NR];

  for (int ir = 0; ir < NR; ++ir) {
    for (int b = 0; b < NB; ++b) { nRB[ir][b] = 0; pkR[ir][b] = -1; }
    hAll[ir]  = new TH1F(Form("hAll%d", ir), ";#SigmaLG [ADC-eq];events", 150, 0, 10500);
    hCent[ir] = new TH1F(Form("hCent%d", ir), "", 150, 0, 10500);
    for (int b = 0; b < NB; ++b) hRB[ir][b] = new TH1F(Form("hRB%d_%d", ir, b), "", 105, 0, 10500);
    for (int j = 0; j < 4; ++j) {
      hMw[ir][j] = new TH1F(Form("hMw%d_%d", ir, j), "", 120, 0, 2400);
      hMf[ir][j] = new TH1F(Form("hMf%d_%d", ir, j), "", 120, 0, 2400);
    }
    hAll[ir]->SetDirectory(nullptr); hCent[ir]->SetDirectory(nullptr);
    for (int b = 0; b < NB; ++b) hRB[ir][b]->SetDirectory(nullptr);
    for (int j = 0; j < 4; ++j) { hMw[ir][j]->SetDirectory(nullptr); hMf[ir][j]->SetDirectory(nullptr); }
    for (int j = 0; j < 4; ++j) share[ir][j] = 0;
    nShare[ir] = 0;

    TFile *f = TFile::Open(Form("data/download/run_%d.root", runs[ir]));
    if (!f || f->IsZombie()) { printf("no file run %d\n", runs[ir]); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    Float_t ch[NSLOT][NSAMP]; t->SetBranchAddress("channel", ch);
    Long64_t nEnt = t->GetEntries();

    // pass 0: median HG peak sample (prompt window seed), first 6000 events
    std::vector<int> pks;
    for (Long64_t i = 0; i < std::min<Long64_t>(6000, nEnt); ++i) {
      t->GetEntry(i);
      for (int j = 0; j < 4; ++j)
        if (ampFull(ch[HGs[j]], +1, BASE_MOD) > 150) pks.push_back(pkSample(ch[HGs[j]], +1, BASE_MOD));
    }
    std::sort(pks.begin(), pks.end());
    int pkMed = pks.empty() ? 60 : pks[pks.size()/2];
    int w0 = pkMed - 10, w1 = pkMed + 10;
    out("run %d: %lld events, HG prompt window samples [%d,%d]\n", runs[ir], nEnt, w0, w1);

    for (Long64_t i = 0; i < nEnt; ++i) {
      if (i % 10000 == 0) { printf("  [%g GeV] %lld/%lld\n", E[ir], i, nEnt); fflush(stdout); }
      t->GetEntry(i);
      double c0 = ampFull(ch[0], -1, 200), c1 = ampFull(ch[1], -1, 200);
      bool tag = (c0 > cthr[ir] && c1 > cthr[ir]);
      if (!tag) {                                         // off-coincidence: MIP spectra, both estimators
        for (int j = 0; j < 4; ++j) {
          double aw = ampWin(ch[HGs[j]], +1, BASE_MOD, w0, w1);
          double af = ampFull(ch[HGs[j]], +1, BASE_MOD);
          if (aw > 60)  hMw[ir][j]->Fill(aw);
          if (af > 60)  hMf[ir][j]->Fill(af);
        }
        continue;
      }
      double a[4], S = 0;
      for (int j = 0; j < 4; ++j) { a[j] = ampFull(ch[LGs[j]], +1, BASE_MOD); S += a[j]; }
      if (S < floorRaw[ir]) continue;
      double bx = ((a[1] + a[3]) - (a[0] + a[2])) / S;    // right - left
      double by = ((a[0] + a[1]) - (a[2] + a[3])) / S;    // top - bottom
      double r = std::sqrt(bx*bx + by*by);
      hAll[ir]->Fill(S);
      if (S < floorPos[ir]) continue;                     // balance undefined below validity floor
      for (int b = 0; b < NB; ++b)
        if (r >= rEdge[b] && r < rEdge[b+1]) { hRB[ir][b]->Fill(S); ++nRB[ir][b]; break; }
      if (r < rEdge[1]) {
        hCent[ir]->Fill(S);
        for (int j = 0; j < 4; ++j) share[ir][j] += a[j] / S;
        ++nShare[ir];
      }
    }
    f->Close();

    // fits
    double sg, sge;
    if (!corePeak(hAll[ir], floorScan[ir], pkScan[ir], pkScanE[ir], sg, sge)) pkScan[ir] = -1;
    if (!corePeak(hCent[ir], floorPos[ir], pkCent[ir], pkCentE[ir], sg, sge)) pkCent[ir] = -1;
    for (int b = 0; b < NB; ++b)
      if (!corePeak(hRB[ir][b], floorPos[ir], pkR[ir][b], pkRE[ir][b], sg, sge)) pkR[ir][b] = -1;
    for (int j = 0; j < 4; ++j) {
      if (!mipFit(hMw[ir][j], mpvW[ir][j], mpvWe[ir][j])) mpvW[ir][j] = -1;
      if (!mipFit(hMf[ir][j], mpvF[ir][j], mpvFe[ir][j])) mpvF[ir][j] = -1;
    }

    out("  scan-floor peak %.0f +/- %.0f | centered (r<%.2f) peak %.0f +/- %.0f (N=%ld)\n",
        pkScan[ir], pkScanE[ir], rEdge[1], pkCent[ir], pkCentE[ir], nShare[ir]);
    for (int b = 0; b < NB; ++b)
      out("    r [%.2f,%.2f): peak %.0f +/- %.0f (N=%ld)\n",
          rEdge[b], rEdge[b+1], pkR[ir][b], pkRE[ir][b], nRB[ir][b]);
    out("  MIP MPV windowed: %.0f/%.0f/%.0f/%.0f | full-window: %.0f/%.0f/%.0f/%.0f\n",
        mpvW[ir][0], mpvW[ir][1], mpvW[ir][2], mpvW[ir][3],
        mpvF[ir][0], mpvF[ir][1], mpvF[ir][2], mpvF[ir][3]);
    if (nShare[ir] > 0)
      out("  centered cap shares (TL/TR/BL/BR): %.3f %.3f %.3f %.3f\n",
          share[ir][0]/nShare[ir], share[ir][1]/nShare[ir],
          share[ir][2]/nShare[ir], share[ir][3]/nShare[ir]);
  }

  // gain factors vs run 15 (windowed MIPs)
  double gW[NR], gWe[NR];
  double sum15 = 0, sum15e2 = 0;
  for (int j = 0; j < 4; ++j) { sum15 += mpvW[1][j]; sum15e2 += mpvWe[1][j]*mpvWe[1][j]; }
  for (int ir = 0; ir < NR; ++ir) {
    double s = 0, e2 = 0;
    for (int j = 0; j < 4; ++j) { s += mpvW[ir][j]; e2 += mpvWe[ir][j]*mpvWe[ir][j]; }
    gW[ir] = s / sum15;
    gWe[ir] = gW[ir] * std::sqrt(e2/(s*s) + sum15e2/(sum15*sum15));
    out("run %d: gain factor vs run 15 (windowed MIP) = %.3f +/- %.3f\n", runs[ir], gW[ir], gWe[ir]);
  }

  // ---------------- canvas ----------------
  TCanvas c("c","c",2000,1200); c.Divide(2,2,0.004,0.006);

  // (1) run 30 dissection
  c.cd(1); gPad->SetLogy();
  TH1F *hA = hAll[4], *hC = hCent[4];
  hA->SetLineColor(rad::cGrey()); hA->SetLineWidth(2); hA->SetFillColor(0);
  hA->SetTitle(";#SigmaLG [ADC-eq];events");
  hA->GetXaxis()->SetRangeUser(0, 10500);
  hA->Draw("hist");
  hC->SetLineColor(rad::cTeal()); hC->SetLineWidth(3); hC->Draw("hist same");
  TLine lf(3800, 0.5, 3800, hA->GetMaximum()); lf.SetLineColor(rad::cAmber());
  lf.SetLineStyle(2); lf.SetLineWidth(2); lf.DrawClone();
  TLegend *l1 = new TLegend(0.44,0.68,0.93,0.90); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(20);
  l1->AddEntry(hA, "11 GeV: all on-module tags", "l");
  l1->AddEntry(hC, Form("centered  r < %.2f  (peak %.0f#pm%.0f)", rEdge[1], pkCent[4], pkCentE[4]), "l");
  l1->AddEntry(&lf, "scan containment floor", "l");
  l1->Draw();
  TLatex tx; tx.SetTextFont(43); tx.SetTextSize(22); tx.SetNDC();
  tx.DrawLatex(0.14, 0.94, "run 30 (11 GeV): does centering restore the peak?");

  // (2) peak vs centrality, normalized to centered bin
  c.cd(2);
  TH1F *fr2 = gPad->DrawFrame(0, 0.70, 0.8, 1.20, ";centrality  r = |capillary balance|;peak(r) / peak(centered)");
  TLegend *l2 = new TLegend(0.14,0.16,0.52,0.42); l2->SetBorderSize(0); l2->SetTextFont(43); l2->SetTextSize(19);
  for (int ir = 0; ir < NR; ++ir) {
    TGraphErrors *g = new TGraphErrors();
    for (int b = 0; b < NB; ++b) {
      if (pkR[ir][b] < 0 || pkR[ir][0] < 0) continue;
      if (nRB[ir][b] < 60 || pkRE[ir][b] > 0.12*pkR[ir][b]) continue;   // unreliable sparse fits
      double rc = 0.5*(rEdge[b]+rEdge[b+1]);
      int n = g->GetN();
      g->SetPoint(n, rc, pkR[ir][b]/pkR[ir][0]);
      g->SetPointError(n, 0, pkRE[ir][b]/pkR[ir][0]);
    }
    g->SetLineColor(cols[ir]); g->SetMarkerColor(cols[ir]);
    g->SetMarkerStyle(20); g->SetMarkerSize(1.2); g->SetLineWidth(2);
    g->Draw("PL same");
    l2->AddEntry(g, Form("%g GeV", E[ir]), "pl");
  }
  TLine one(0, 1, 0.8, 1); one.SetLineColor(rad::cGrey()); one.SetLineStyle(2); one.DrawClone();
  l2->Draw();
  tx.DrawLatex(0.14, 0.94, "transverse leakage: peak vs impact centrality (5 GeV = control)");

  // (3) MIP ladder: windowed vs full estimator
  c.cd(3);
  TH1F *fr3 = gPad->DrawFrame(0.5, 0, 5.5, 2100, ";scan point;#Sigma_{caps} MIP MPV [ADC-eq]");
  fr3->GetXaxis()->SetNdivisions(505);
  TGraphErrors *gw = new TGraphErrors(), *gf = new TGraphErrors();
  for (int ir = 0; ir < NR; ++ir) {
    double sw = 0, sf = 0, ew2 = 0, ef2 = 0;
    for (int j = 0; j < 4; ++j) { sw += mpvW[ir][j]; sf += mpvF[ir][j];
      ew2 += mpvWe[ir][j]*mpvWe[ir][j]; ef2 += mpvFe[ir][j]*mpvFe[ir][j]; }
    gw->SetPoint(ir, ir+1, sw); gw->SetPointError(ir, 0, std::sqrt(ew2));
    gf->SetPoint(ir, ir+1, sf); gf->SetPointError(ir, 0, std::sqrt(ef2));
  }
  gf->SetMarkerStyle(24); gf->SetMarkerSize(1.5); gf->SetMarkerColor(rad::cGrey()); gf->SetLineColor(rad::cGrey());
  gw->SetMarkerStyle(20); gw->SetMarkerSize(1.5); gw->SetMarkerColor(rad::cTeal()); gw->SetLineColor(rad::cTeal());
  gf->Draw("P same"); gw->Draw("P same");
  TLegend *l3 = new TLegend(0.14,0.74,0.66,0.90); l3->SetBorderSize(0); l3->SetTextFont(43); l3->SetTextSize(19);
  l3->AddEntry(gw, "windowed + smoothed (noise-robust)", "p");
  l3->AddEntry(gf, "full-window max (old, noise-inflated)", "p");
  l3->Draw();
  for (int ir = 0; ir < NR; ++ir)
    tx.DrawLatex(0.16 + 0.155*ir, 0.15, Form("#font[43]{#scale[0.8]{r%d @ %gGeV}}", runs[ir], E[ir]));
  tx.DrawLatex(0.14, 0.94, "the gain ladder, measured honestly");

  // (4) money plot
  c.cd(4);
  TH1F *fr4 = gPad->DrawFrame(0, 0, 12.5, 9000, ";beam energy [GeV];#SigmaLG peak [ADC-eq, run-15 gain]");
  TGraphErrors *g1 = new TGraphErrors(), *g2 = new TGraphErrors();
  for (int ir = 0; ir < NR; ++ir) {
    g1->SetPoint(ir, E[ir] - 0.12, pkScan[ir]); g1->SetPointError(ir, 0, pkScanE[ir]);
    g2->SetPoint(ir, E[ir] + 0.12, pkCent[ir]); g2->SetPointError(ir, 0, pkCentE[ir]);
  }
  double slope = pkCent[1] / E[1];
  TF1 *lin = new TF1("lin","[0]*x", 0, 12.5); lin->SetParameter(0, slope);
  lin->SetLineColor(rad::cGrey()); lin->SetLineStyle(2); lin->SetLineWidth(2); lin->Draw("same");
  TGraph *gL = new TGraph();
  for (int i = 0; i <= 46; ++i) { double e = 1.0 + i*0.25; gL->SetPoint(i, e, slope*e*longoFactor(e)); }
  gL->SetLineColor(rad::cAmber()); gL->SetLineStyle(7); gL->SetLineWidth(3); gL->Draw("L same");
  g1->SetMarkerStyle(24); g1->SetMarkerSize(1.3); g1->SetMarkerColor(rad::cGrey()); g1->SetLineColor(rad::cGrey());
  g2->SetMarkerStyle(20); g2->SetMarkerSize(1.5); g2->SetMarkerColor(rad::cTeal()); g2->SetLineColor(rad::cTeal());
  g1->Draw("P same"); g2->Draw("P same");
  TLegend *l4 = new TLegend(0.14,0.62,0.66,0.90); l4->SetBorderSize(0); l4->SetTextFont(43); l4->SetTextSize(19);
  l4->AddEntry(g1, "scan (raw, as published)", "p");
  l4->AddEntry(g2, "centered subset  (r < 0.18)", "p");
  l4->AddEntry(lin, "linear through centered 5 GeV", "l");
  l4->AddEntry(gL, "linear #times shower-max migration (Longo)", "l");
  l4->Draw();
  tx.DrawLatex(0.14, 0.94, "response linearity, corrected step by step");

  gSystem->mkdir("Output/saturation", true);
  c.SaveAs("Output/saturation/SaturationStudy.png");

  out("\n== residuals vs linear (CENTERED curve, slope %.1f ADC-eq/GeV) ==\n", slope);
  for (int ir = 0; ir < NR; ++ir)
    out("  %2g GeV: centered %.0f +/- %.0f, linear %.0f -> %+.1f%%\n",
        E[ir], pkCent[ir], pkCentE[ir], slope*E[ir], 100.0*(pkCent[ir]/(slope*E[ir]) - 1));
  out("\n== residuals vs linear x Longo (shower-max migration removed) ==\n");
  for (int ir = 0; ir < NR; ++ir)
    out("  %2g GeV: %+.1f%%  (Longo factor %.3f)\n", E[ir],
        100.0*(pkCent[ir]/(slope*E[ir]*longoFactor(E[ir])) - 1), longoFactor(E[ir]));
  out("\nMIP-normalized variant NOT plotted: the MIP gain factors (%.2f/%.2f/%.2f/%.2f/%.2f)\n"
      "are invalidated as a gain monitor — beam species mix (p vs pi 'MIPs') and\n"
      "ambient-light estimator bias, not SiPM gain (drift study: response flat).\n",
      gW[0], gW[1], gW[2], gW[3], gW[4]);
  fclose(sum);
  printf("Wrote Output/saturation/SaturationStudy.png + summary\n");
  gSystem->Exit(0);
}
