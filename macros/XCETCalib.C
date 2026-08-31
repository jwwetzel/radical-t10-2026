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
// Fit <Npe> vs pressure through the origin per counter -> pe/bar, and
// predict per-counter and coincidence tag efficiencies for ANY pressure.
//
// Usage: root -l -b -q 'macros/XCETCalib.C+'
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TMath.h"
#include "TSystem.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;

static double ampOf(const float *w, int be)
{
  double b = 0; for (int k = 0; k < be; ++k) b += w[k]; b /= be;
  float mn = w[0];
  for (int k = 1; k < NSAMP; ++k) if (w[k] < mn) mn = w[k];
  return (b - mn) * MV2ADC;                        // XCETs are negative-going
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
  double rA1[MAXR][2], rA1e[MAXR][2], rMean[MAXR][2], rMeanE[MAXR][2], rPkAmp[MAXR][2], rP[MAXR][2];
  long rN[MAXR][2];
  for (int i = 0; i < MAXR; ++i) for (int c = 0; c < 2; ++c) { rA1[i][c] = -1; rMean[i][c] = -1; rPkAmp[i][c] = -1; rN[i][c] = 0; }
  out("# run  ctr  P[bar]  A1pe[ADC]  <amp>probe  probeN\n");

  for (int ir = 0; ir < NR; ++ir) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", R[ir].run));
    if (!f || f->IsZombie()) { out("# run %d missing\n", R[ir].run); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP];
    t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
    t->SetBranchAddress("channel", ch);
    Long64_t nEnt = std::min(t->GetEntries(), MAXEV);

    TH1F *h1[2], *hp[2]; double sumP[2] = {0,0}; long nP[2] = {0,0};
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
        if (a[o] > R[ir].cthr) { hp[c]->Fill(a[c]); sumP[c] += a[c]; ++nP[c]; }  // tag-and-probe
      }
    }
    f->Close();

    for (int c = 0; c < 2; ++c) {
      rP[ir][c] = c ? R[ir].p43 : R[ir].p40;
      // probe mean amplitude (includes zeros: Poisson-exact) — stored regardless of 1-pe fit
      if (nP[c] > 30) { rMean[ir][c] = sumP[c]/nP[c]; rMeanE[ir][c] = hp[c]->GetRMS()/std::sqrt((double)nP[c]); rN[ir][c] = nP[c]; }
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
        if (pv >= 8 && m > 25 && m < 110) { rA1[ir][0] = m; rA1e[ir][0] = g1.GetParError(1); }
      } else {
        TH1F *hr = (TH1F*)h1[1]->Clone(Form("hr%d", R[ir].run)); hr->Rebin(2);   // 4 ADC-eq bins
        TF1 cf("cf","expo(0)+gaus(2)", 6, 60);
        double ped0 = std::max(1.0, hr->GetBinContent(hr->FindBin(7)));
        cf.SetParameters(std::log(ped0), -0.25, hr->GetBinContent(hr->FindBin(18)), 18, 5);
        cf.SetParLimits(3, 12, 32); cf.SetParLimits(4, 2.5, 10);
        hr->Fit(&cf, "QRNL", "", 6, 55);
        double m = cf.GetParameter(3), amp = cf.GetParameter(2);
        double pedAt = std::exp(cf.GetParameter(0) + cf.GetParameter(1)*m);
        if (amp > 5 && amp > 0.3*pedAt && m > 12 && m < 31) { rA1[ir][1] = m; rA1e[ir][1] = cf.GetParError(3); }
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
  // justified by XCET40's per-run constancy, +-5% over 5 days at fixed HV)
  double A1med[2];
  for (int c = 0; c < 2; ++c) {
    std::vector<double> v;
    for (int i = 0; i < NR; ++i) if (rA1[i][c] > 0) v.push_back(rA1[i][c]);
    std::sort(v.begin(), v.end());
    A1med[c] = v.empty() ? -1 : v[v.size()/2];
    if (c == 1 && v.size() < 3) out("NOTE XCET43: 1-pe resolved in only %zu run(s) — ruler carries a ~15%% absolute-scale systematic\n", v.size());
    out("A1pe median XCET%d = %.1f ADC-eq (%zu runs fitted)\n", c?43:40, A1med[c], v.size());
    for (int i = 0; i < NR; ++i) {
      if (rMean[i][c] <= 0 || A1med[c] <= 0) continue;
      if (R[i].run == 34) continue;   // pion-contaminated probe (1.3 bar tag radiates on pions at +5 GeV) — excluded from the calibration fit
      double A1 = rA1[i][c] > 0 ? rA1[i][c] : A1med[c];
      double Nm = rMean[i][c]/A1, Nme = std::sqrt(std::pow(rMeanE[i][c]/A1,2) + std::pow(0.08*Nm,2));   // 8% floor: run-to-run beam-optics/trajectory scatter
      int n = gN[c]->GetN(); gN[c]->SetPoint(n, rP[i][c], Nm); gN[c]->SetPointError(n, 0, Nme);
      if (rPkAmp[i][c] > 0) { int n2 = gPk[c]->GetN(); gPk[c]->SetPoint(n2, rP[i][c], rPkAmp[i][c]/A1); gPk[c]->SetPointError(n2, 0, 0.04*rPkAmp[i][c]/A1); }
      if (rA1[i][c] > 0) { int n3 = gA1[c]->GetN(); gA1[c]->SetPoint(n3, i, rA1[i][c]); gA1[c]->SetPointError(n3, 0, rA1e[i][c]); }
    }
  }
  // fits through the origin
  double slope[2], slopeE[2];
  for (int c = 0; c < 2; ++c) {
    TF1 lin("lin","[0]*x", 0, 1.5);
    gN[c]->Fit(&lin, "QRN");
    slope[c] = lin.GetParameter(0); slopeE[c] = lin.GetParError(0);
    out("XCET%d: <Npe> = (%.1f +/- %.1f) pe/bar * P   (mean estimator, chi2/ndf %.1f/%d)\n",
        c?43:40, slope[c], slopeE[c], lin.GetChisquare(), lin.GetNDF());
  }
  // efficiency prediction table
  out("\n# predicted tag efficiency UPPER BOUND (Poisson zero-suppression only; software threshold not folded in)\n");
  for (double P : {0.06, 0.09, 0.15, 0.21, 0.40, 0.60}) {
    double e40 = 1 - std::exp(-slope[0]*P), e43 = 1 - std::exp(-slope[1]*P);
    out("P=%.2f bar: eff40 %.1f%%  eff43 %.1f%%  coincidence %.1f%%\n", P, 100*e40, 100*e43, 100*e40*e43);
  }

  // ---- canvas ----
  TCanvas c2("c2","c2",2000,900); c2.Divide(2,1,0.004,0.004);
  c2.cd(1);
  TH1F *fr1 = gPad->DrawFrame(0, 0, 1.45, 32, ";XCET pressure [bar];#LTN_{pe}#GT per electron");
  int colc[2] = {rad::cTeal(), rad::cBlue()};
  TLegend *l1 = new TLegend(0.16,0.64,0.72,0.90); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(21);
  for (int c = 0; c < 2; ++c) {
    TF1 *lin = new TF1(Form("l%d",c),"[0]*x",0,1.45); lin->SetParameter(0, slope[c]);
    lin->SetLineColor(colc[c]); lin->SetLineWidth(2); lin->SetLineStyle(7); lin->Draw("same");
    gN[c]->SetMarkerStyle(c?21:20); gN[c]->SetMarkerSize(1.2); gN[c]->SetMarkerColor(colc[c]); gN[c]->SetLineColor(colc[c]);
    gN[c]->Draw("P same");
    gPk[c]->SetMarkerStyle(c?25:24); gPk[c]->SetMarkerSize(1.3); gPk[c]->SetMarkerColor(colc[c]); gPk[c]->SetLineColor(colc[c]);
    gPk[c]->Draw("P same");
    l1->AddEntry(gN[c], Form("XCET%d: (%.1f#pm%.1f) pe/bar  (filled: mean, open: peak)", c?43:40, slope[c], slopeE[c]), "pl");
  }
  l1->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(26);
  tx.DrawLatex(0.14,0.94,"Absolute photoelectron yield vs pressure (tag-and-probe / 1-pe ruler)");
  c2.cd(2); gPad->SetLogy(0);
  TH1F *fr2 = gPad->DrawFrame(0, 0.0, 1.45, 1.05, ";XCET pressure [bar];tag efficiency");
  for (int c = 0; c < 2; ++c) {
    TF1 *ef = new TF1(Form("e%d",c),"1-exp(-[0]*x)",0.01,1.45); ef->SetParameter(0, slope[c]);
    ef->SetLineColor(colc[c]); ef->SetLineWidth(3); ef->Draw("same");
  }
  TF1 *ec = new TF1("ec","(1-exp(-[0]*x))*(1-exp(-[1]*x))",0.01,1.45);
  ec->SetParameters(slope[0], slope[1]); ec->SetLineColor(rad::cRed()); ec->SetLineWidth(3); ec->SetLineStyle(2); ec->Draw("same");
  TLegend *l2 = new TLegend(0.42,0.18,0.93,0.44); l2->SetBorderSize(0); l2->SetTextFont(43); l2->SetTextSize(21);
  l2->AddEntry((TObject*)nullptr, "predicted (Poisson, zero-suppression only):", "");
  TF1 *e40d = new TF1("e40d","1-exp(-[0]*x)",0,1); e40d->SetParameter(0,slope[0]);
  l2->AddEntry(ec, "two-counter coincidence", "l");
  l2->Draw();
  tx.DrawLatex(0.14,0.94,"Predicted tag efficiency vs pressure");
  c2.SaveAs("Output/summary/XCETCalib.png");
  fclose(sum);
  printf("Wrote Output/summary/XCETCalib.png + summary\n");
  gSystem->Exit(0);
}
