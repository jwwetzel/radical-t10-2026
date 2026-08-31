// XCETCalib.C — absolute photoelectron calibration of the two XCET counters.
//
// Idea (from the -7 GeV spectrum structure): the sharp low peak is the
// single-photoelectron response — a fixed gain ruler. Tag-and-probe with the
// OTHER counter gives an unbiased electron spectrum in the probe counter;
// dividing by the 1-pe position gives the absolute photoelectron yield.
// Two estimators of <Npe>:
//   mean : <probe amplitude incl. zeros>/A1pe  — Poisson-exact, works even
//          at 0.06 bar where the electron hump dissolves into counting;
//   peak : Gaussian core of the resolved electron hump / A1pe (>=0.15 bar).
// Fit <Npe> vs pressure through the origin per counter -> pe/bar. Efficiency
// panel: Poisson zero-suppression alone is only an UPPER BOUND — the folded
// prediction convolves the pe statistics with the fitted 1-pe width and the
// 40 ADC-eq software tag threshold the physics analyses used at low pressure,
// and is validated against the MEASURED per-run tag-and-probe efficiencies.
// Fit errors are chi2-scaled; the 1-pe ruler scale systematic (5% XCET40 from
// its 15-run RMS, 15% XCET43 from its single pedestal-edge fit) is quoted as
// a second term — it is 100% correlated across points and cannot live in the
// per-point errors.
//
// Usage: root -l -b -q 'macros/XCETCalib.C+'
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TMath.h"
#include "TSystem.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const double THR = 40.0;   // software tag threshold of the physics analyses at low pressure (per XCETCheck scans)

static double ampOf(const float *w, int be)
{
  double b = 0; for (int k = 0; k < be; ++k) b += w[k]; b /= be;
  float mn = w[0];
  for (int k = 1; k < NSAMP; ++k) if (w[k] < mn) mn = w[k];
  return (b - mn) * MV2ADC;                        // XCETs are negative-going
}

// tag efficiency with the software threshold folded in: Poisson pe count,
// n-pe amplitude smeared by sqrt(n) x the fitted 1-pe width
static double effFold(double lam, double A1, double s1, double thr)
{
  double e = 0;
  for (int n = 1; n <= 120; ++n)
    e += TMath::PoissonI(n, lam) * 0.5 * TMath::Erfc((thr - n*A1)/(std::sqrt(2.0*n)*s1));
  return e;
}

void XCETCalib()
{
  SetRadStyle();
  gSystem->mkdir("Output/summary", true);
  FILE *sum = fopen("Output/summary/XCETCalib_summary.txt", "w");
  auto out = [&](const char *f, ...) { va_list a; va_start(a,f); vfprintf(stdout,f,a); va_end(a);
    va_start(a,f); vfprintf(sum,f,a); va_end(a); fflush(sum); };

  // run, P(XCET40), P(XCET43), tag threshold  — pressures from the resolved registry
  struct RunDef { int run; double p40, p43, cthr; };
  RunDef R[] = {
    {15,0.400,0.400,100},{24,1.300,1.300,100},{2526,1.300,1.300,100},{27,0.210,0.210,50},
    {9001,0.150,0.156,40},{30,0.062,0.060,40},{31,0.062,0.060,40},{32,0.149,0.146,40},
    {33,0.206,0.194,40},{34,1.314,1.304,100},{35,1.313,1.303,100},{37,0.400,0.405,100},
    {38,0.590,0.553,100},{39,0.593,0.593,100},{40,0.593,0.593,100},{41,0.452,0.439,100},
    {42,0.225,0.220,40},{43,0.152,0.156,40},{44,0.090,0.088,40} };
  const int NR = sizeof(R)/sizeof(R[0]);
  const Long64_t MAXEV = 20000;

  TGraphErrors *gN[2], *gPk[2], *gA1[2];
  for (int c = 0; c < 2; ++c) { gN[c] = new TGraphErrors(); gPk[c] = new TGraphErrors(); gA1[c] = new TGraphErrors(); }
  const int MAXR = 32;
  double rA1[MAXR][2], rA1e[MAXR][2], rS1[MAXR][2], rMean[MAXR][2], rMeanE[MAXR][2], rPkAmp[MAXR][2], rP[MAXR][2], rEff[MAXR][2];
  long rN[MAXR][2];
  for (int i = 0; i < MAXR; ++i) for (int c = 0; c < 2; ++c) { rA1[i][c] = -1; rS1[i][c] = -1; rMean[i][c] = -1; rPkAmp[i][c] = -1; rEff[i][c] = -1; rN[i][c] = 0; }
  out("# run  ctr  P[bar]  A1pe[ADC]  <amp>probe  probeN\n");

  for (int ir = 0; ir < NR; ++ir) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", R[ir].run));
    if (!f || f->IsZombie()) { out("# run %d missing\n", R[ir].run); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP];
    t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
    t->SetBranchAddress("channel", ch);
    Long64_t nEnt = std::min(t->GetEntries(), MAXEV);

    TH1F *h1[2], *hp[2]; double sumP[2] = {0,0}; long nP[2] = {0,0}, nAb[2] = {0,0};
    for (int c = 0; c < 2; ++c) {
      h1[c] = new TH1F(Form("h1_%d_%d",R[ir].run,c),"",120,0,240);  h1[c]->SetDirectory(nullptr);
      hp[c] = new TH1F(Form("hp_%d_%d",R[ir].run,c),"",200,0,1600); hp[c]->SetDirectory(nullptr);
    }
    for (Long64_t i = 0; i < nEnt; ++i) {
      t->GetEntry(i);
      double a[2] = { ampOf(ch[0],200), ampOf(ch[1],200) };
      for (int c = 0; c < 2; ++c) {
        int o = 1 - c;
        if (a[o] < 25) h1[c]->Fill(a[c]);                       // anti-tag: 1-pe spike sample
        if (a[o] > R[ir].cthr) { hp[c]->Fill(a[c]); sumP[c] += a[c]; ++nP[c];
          if (a[c] > THR) ++nAb[c]; }                           // tag-and-probe (+ measured tag efficiency)
      }
    }
    f->Close();

    for (int c = 0; c < 2; ++c) {
      rP[ir][c] = c ? R[ir].p43 : R[ir].p40;
      // probe mean amplitude (includes zeros: Poisson-exact) — stored regardless of 1-pe fit
      if (nP[c] > 30) { rMean[ir][c] = sumP[c]/nP[c]; rMeanE[ir][c] = hp[c]->GetRMS()/std::sqrt((double)nP[c]); rN[ir][c] = nP[c];
        rEff[ir][c] = (double)nAb[c]/nP[c]; }
      // electron-hump peak position (amplitude units), where resolved
      { int c0 = hp[c]->FindBin(c ? 45 : 100), pb2 = c0; double pv2 = 0;
        for (int b = c0; b <= hp[c]->GetNbinsX(); ++b) if (hp[c]->GetBinContent(b) > pv2) { pv2 = hp[c]->GetBinContent(b); pb2 = b; }
        if (pv2 > 15 && hp[c]->GetBinCenter(pb2) > (c ? 60 : 130)) {
          double m2 = hp[c]->GetBinCenter(pb2), s2 = 0.5*m2;
          TF1 g2("g2","gaus", 40, 1600); g2.SetParameters(pv2, m2, s2);
          for (int it = 0; it < 3; ++it) { hp[c]->Fit(&g2,"QRN","", std::max(40.0, m2-1.3*s2), m2+1.3*s2);
            m2 = g2.GetParameter(1); s2 = std::fabs(g2.GetParameter(2)); }
          rPkAmp[ir][c] = m2; } }
      // 1-pe fit: XCET40 plain Gaussian in a clean window; XCET43 pedestal-expo + Gaussian composite
      if (c == 0) {
        int b0 = h1[0]->FindBin(27), b1 = h1[0]->FindBin(95), pb = b0; double pv = 0;
        for (int b = b0; b <= b1; ++b) if (h1[0]->GetBinContent(b) > pv) { pv = h1[0]->GetBinContent(b); pb = b; }
        double m = h1[0]->GetBinCenter(pb);
        TF1 g1("g1","gaus", 25, 120); g1.SetParameters(pv, m, 12);
        for (int it = 0; it < 3; ++it) { h1[0]->Fit(&g1,"QRN","", m-18, m+18); m = g1.GetParameter(1); }
        if (pv >= 8 && m > 25 && m < 110) { rA1[ir][0] = m; rA1e[ir][0] = g1.GetParError(1); rS1[ir][0] = std::fabs(g1.GetParameter(2)); }
      } else {
        TH1F *hr = (TH1F*)h1[1]->Clone(Form("hr%d", R[ir].run)); hr->Rebin(2);   // 4 ADC-eq bins
        TF1 cf("cf","expo(0)+gaus(2)", 6, 60);
        double ped0 = std::max(1.0, hr->GetBinContent(hr->FindBin(7)));
        cf.SetParameters(std::log(ped0), -0.25, hr->GetBinContent(hr->FindBin(18)), 18, 5);
        cf.SetParLimits(3, 12, 32); cf.SetParLimits(4, 2.5, 10);
        hr->Fit(&cf, "QRNL", "", 6, 55);
        double m = cf.GetParameter(3), amp = cf.GetParameter(2);
        double pedAt = std::exp(cf.GetParameter(0) + cf.GetParameter(1)*m);
        if (amp > 5 && amp > 0.3*pedAt && m > 12 && m < 31) { rA1[ir][1] = m; rA1e[ir][1] = cf.GetParError(3); rS1[ir][1] = std::fabs(cf.GetParameter(4)); }
        delete hr;
      }
      out("%5d  %d  %.3f  %s  %s (probeN %ld)\n", R[ir].run, c?43:40, rP[ir][c],
          rA1[ir][c] > 0 ? Form("%5.1f+/-%.1f", rA1[ir][c], rA1e[ir][c]) : "  --  ",
          rMean[ir][c] > 0 ? Form("%7.1f+/-%.1f", rMean[ir][c], rMeanE[ir][c]) : "  --  ", rN[ir][c]);
      delete h1[c]; delete hp[c];
    }
    printf("run %d done\n", R[ir].run);
  }

  // gain rulers: per-run where fitted, median fallback otherwise (gain stability
  // justified by XCET40's per-run constancy, +-5% RMS over 5 days at fixed HV)
  double A1med[2], S1med[2];
  for (int c = 0; c < 2; ++c) {
    std::vector<double> v, vs;
    for (int i = 0; i < NR; ++i) if (rA1[i][c] > 0) { v.push_back(rA1[i][c]); vs.push_back(rS1[i][c]); }
    std::sort(v.begin(), v.end()); std::sort(vs.begin(), vs.end());
    A1med[c] = v.empty() ? -1 : v[v.size()/2];
    S1med[c] = vs.empty() ? 0.30*A1med[c] : vs[vs.size()/2];
    if (c == 1 && v.size() < 3) out("NOTE XCET43: 1-pe resolved in only %zu run(s) — ruler carries a ~15%% absolute-scale systematic\n", v.size());
    out("A1pe median XCET%d = %.1f ADC-eq, sigma1pe = %.1f ADC-eq (%zu runs fitted)\n", c?43:40, A1med[c], S1med[c], v.size());
    for (int i = 0; i < NR; ++i) {
      if (rMean[i][c] <= 0 || A1med[c] <= 0) continue;
      if (R[i].run == 34) continue;   // pion-contaminated probe (1.3 bar tag radiates on pions at +5 GeV) — excluded from the calibration fit
      double A1 = rA1[i][c] > 0 ? rA1[i][c] : A1med[c];
      double Nm = rMean[i][c]/A1, Nme = std::sqrt(std::pow(rMeanE[i][c]/A1,2) + std::pow(0.08*Nm,2));   // 8% floor: run-to-run scatter (see summary caveat)
      int n = gN[c]->GetN(); gN[c]->SetPoint(n, rP[i][c], Nm); gN[c]->SetPointError(n, 0, Nme);
      if (rPkAmp[i][c] > 0) { int n2 = gPk[c]->GetN(); gPk[c]->SetPoint(n2, rP[i][c], rPkAmp[i][c]/A1); gPk[c]->SetPointError(n2, 0, 0.04*rPkAmp[i][c]/A1); }
      if (rA1[i][c] > 0) { int n3 = gA1[c]->GetN(); gA1[c]->SetPoint(n3, i, rA1[i][c]); gA1[c]->SetPointError(n3, 0, rA1e[i][c]); }
    }
  }
  // fits through the origin; fit error chi2-scaled, 1-pe ruler scale systematic quoted separately
  double slope[2], slopeE[2], scaleE[2];
  for (int c = 0; c < 2; ++c) {
    TF1 lin("lin","[0]*x", 0, 1.5);
    gN[c]->Fit(&lin, "QRN");
    double sf = lin.GetNDF() > 0 ? std::sqrt(std::max(1.0, lin.GetChisquare()/lin.GetNDF())) : 1.0;
    slope[c] = lin.GetParameter(0); slopeE[c] = lin.GetParError(0)*sf;
    scaleE[c] = std::round(slope[c]*(c ? 0.15 : 0.05)*10.0)/10.0;
    out("XCET%d: <Npe> = (%.1f +/- %.1f fit +/- %.1f scale) pe/bar * P   (mean estimator, chi2/ndf %.1f/%d, fit err x%.2f chi2-scaled; scale = %d%% 1-pe ruler)\n",
        c?43:40, slope[c], slopeE[c], scaleE[c], lin.GetChisquare(), lin.GetNDF(), sf, c?15:5);
  }
  out("CAVEAT XCET43: residuals are pressure-structured (not random run-to-run scatter) — under investigation; see FINDINGS\n");
  // efficiency: Poisson-only upper bound vs threshold-folded prediction
  out("\n# tag efficiency: Poisson-only UPPER BOUND vs FOLDED (1-pe ruler x sqrt(n) width, %g ADC-eq software threshold)\n", THR);
  for (double P : {0.06, 0.09, 0.15, 0.21, 0.40, 0.60}) {
    double b40 = 1 - std::exp(-slope[0]*P), b43 = 1 - std::exp(-slope[1]*P);
    double f40 = effFold(slope[0]*P, A1med[0], S1med[0], THR), f43 = effFold(slope[1]*P, A1med[1], S1med[1], THR);
    out("P=%.2f bar: bound 40/43/coinc %5.1f/%5.1f/%5.1f%%   folded %5.1f/%5.1f/%5.1f%%\n",
        P, 100*b40, 100*b43, 100*b40*b43, 100*f40, 100*f43, 100*f40*f43);
  }
  out("\n# MEASURED probe efficiency above %g ADC-eq (tag-and-probe; run 34 pion-contaminated, excluded from fit/plot)\n", THR);
  for (int i = 0; i < NR; ++i) for (int c = 0; c < 2; ++c) {
    if (rEff[i][c] < 0) continue;
    double e = rEff[i][c], ee = std::sqrt(std::max(e*(1-e), 1.0/rN[i][c])/rN[i][c]);
    out("%5d  XCET%d  P=%.3f  eff = %.3f +/- %.3f  (N=%ld)%s\n", R[i].run, c?43:40, rP[i][c], e, ee, rN[i][c],
        R[i].run == 34 ? "  [excluded]" : "");
  }

  // ---- canvas ----
  TCanvas c2("c2","c2",2000,900); c2.Divide(2,1,0.004,0.004);
  c2.cd(1);
  TH1F *fr1 = gPad->DrawFrame(0, 0, 1.45, 42, ";XCET pressure [bar];#LTN_{pe}#GT per electron");
  int colc[2] = {rad::cTeal(), rad::cBlue()};
  TLegend *l1 = new TLegend(0.16,0.58,0.62,0.90); l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextFont(43); l1->SetTextSize(21);
  for (int c = 0; c < 2; ++c) {
    TF1 *lin = new TF1(Form("l%d",c),"[0]*x",0,1.40); lin->SetParameter(0, slope[c]);   // drawn over the fitted data range only
    lin->SetLineColor(colc[c]); lin->SetLineWidth(2); lin->SetLineStyle(7); lin->Draw("same");
    gN[c]->SetMarkerStyle(c?21:20); gN[c]->SetMarkerSize(1.2); gN[c]->SetMarkerColor(colc[c]); gN[c]->SetLineColor(colc[c]);
    gN[c]->Draw("P same");
    gPk[c]->SetMarkerStyle(c?25:24); gPk[c]->SetMarkerSize(1.3); gPk[c]->SetMarkerColor(colc[c]); gPk[c]->SetLineColor(colc[c]);
    gPk[c]->Draw("P same");
    l1->AddEntry(gN[c], Form("XCET%d  (%.1f #pm %.1f_{fit} #pm %.1f_{scale}) pe/bar", c?43:40, slope[c], slopeE[c], scaleE[c]), "pl");
  }
  TGraph *keyF = new TGraph(1); keyF->SetPoint(0,-1,-1); keyF->SetMarkerStyle(20); keyF->SetMarkerSize(1.2); keyF->SetMarkerColor(kGray+2);
  TGraph *keyO = new TGraph(1); keyO->SetPoint(0,-1,-1); keyO->SetMarkerStyle(24); keyO->SetMarkerSize(1.3); keyO->SetMarkerColor(kGray+2);
  keyF->Draw("P same"); keyO->Draw("P same");   // off-frame; legend glyphs only
  l1->AddEntry(keyF, "filled: mean incl. zeros (Poisson-exact)", "p");
  l1->AddEntry(keyO, "open: resolved e-hump fit", "p");
  l1->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(26);
  tx.DrawLatex(0.14,0.94,"Absolute photoelectron yield vs pressure (tag-and-probe / 1-pe ruler)");
  c2.cd(2); gPad->SetLogy(0);
  TH1F *fr2 = gPad->DrawFrame(0, 0.0, 0.65, 1.05, ";XCET pressure [bar];tag efficiency");
  TLine *gl = nullptr;
  for (double P : {0.06, 0.09, 0.15, 0.21}) {                    // campaign operating pressures
    TLine *L = new TLine(P, 0, P, 1.0); L->SetLineColor(kGray+1); L->SetLineStyle(3); L->Draw(); gl = L;
  }
  TF1 *efF[2];
  for (int c = 0; c < 2; ++c) {
    const double sl = slope[c], A1 = A1med[c], s1 = S1med[c];
    efF[c] = new TF1(Form("efF%d",c), [sl,A1,s1](double *x, double *) { return effFold(sl*x[0], A1, s1, THR); }, 0.002, 0.65, 0);
    efF[c]->SetNpx(400); efF[c]->SetLineColor(colc[c]); efF[c]->SetLineWidth(3); efF[c]->Draw("same");
  }
  const double sl0 = slope[0], sl1 = slope[1], A10 = A1med[0], A11 = A1med[1], s10 = S1med[0], s11 = S1med[1];
  TF1 *ecF = new TF1("ecF", [=](double *x, double *) { return effFold(sl0*x[0],A10,s10,THR)*effFold(sl1*x[0],A11,s11,THR); }, 0.002, 0.65, 0);
  ecF->SetNpx(400); ecF->SetLineColor(rad::cRed()); ecF->SetLineWidth(3); ecF->SetLineStyle(7); ecF->Draw("same");
  TF1 *ecB = new TF1("ecB","(1-exp(-[0]*x))*(1-exp(-[1]*x))",0.002,0.65);
  ecB->SetParameters(slope[0], slope[1]); ecB->SetLineColor(kGray+2); ecB->SetLineWidth(2); ecB->SetLineStyle(5); ecB->Draw("same");
  TGraphErrors *gE[2];
  for (int c = 0; c < 2; ++c) {
    gE[c] = new TGraphErrors();
    for (int i = 0; i < NR; ++i) {
      if (R[i].run == 34 || rEff[i][c] < 0) continue;
      double e = rEff[i][c]; int n = gE[c]->GetN();
      gE[c]->SetPoint(n, rP[i][c], e);
      gE[c]->SetPointError(n, 0, std::sqrt(std::max(e*(1-e), 1.0/rN[i][c])/rN[i][c]));
    }
    gE[c]->SetMarkerStyle(c?21:20); gE[c]->SetMarkerSize(1.1); gE[c]->SetMarkerColor(colc[c]); gE[c]->SetLineColor(colc[c]);
    gE[c]->Draw("P same");
  }
  TLegend *l2 = new TLegend(0.34,0.15,0.93,0.48); l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextFont(43); l2->SetTextSize(20);
  for (int c = 0; c < 2; ++c) l2->AddEntry(gE[c], Form("XCET%d (curve: predicted, points: measured)", c?43:40), "pl");
  l2->AddEntry(ecF, "coincidence, predicted", "l");
  l2->AddEntry(ecB, "coincidence, no-threshold upper bound", "l");
  l2->AddEntry(gl, "campaign operating pressures", "l");
  l2->Draw();
  tx.DrawLatex(0.14,0.94,"Tag efficiency vs pressure (1-pe ruler #oplus 40 ADC-eq threshold)");
  c2.SaveAs("Output/summary/XCETCalib.png");
  fclose(sum);
  printf("Wrote Output/summary/XCETCalib.png + summary\n");
  gSystem->Exit(0);
}
