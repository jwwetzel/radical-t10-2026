// XCETScanPlot.C — XCET tagging overview across the scan: coincidence rate
// vs threshold for every energy (left) and the XCET40 amplitude spectra
// (right), showing how the electron peak slides down as the pressure is
// scaled with 1/p^2. Chosen per-energy thresholds are marked.
//
// Usage: root -l -b -q 'macros/XCETScanPlot.C+'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TGraph.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include <cstdio>
#include "radStyle.h"

static const int NSAMP = 1024;
static const double MV2ADC = 4.095;

void XCETScanPlot()
{
  SetRadStyle();
  gSystem->mkdir("Output/scan", true);
  const int NP = 5;
  int runs[NP] = {2526, 24, 15, 27, 2829};
  double E[NP] = {1, 3, 5, 7, 9};
  double chosen[NP] = {100, 100, 100, 50, 40};
  int cols[NP] = {rad::cTeal(), rad::cAmber(), rad::cRed(), rad::cBlue(), rad::cInk()};

  const int NT = 27;
  TGraph *gc[NP]; TH1F *hx[NP];
  for (int ip = 0; ip < NP; ++ip) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", runs[ip]));
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[18][NSAMP];
    t->SetBranchStatus("*", 0); t->SetBranchStatus("channel", 1);
    t->SetBranchAddress("channel", ch);
    Long64_t N = t->GetEntries();
    hx[ip] = new TH1F(Form("hx%d", ip), ";XCET40 amplitude [ADC-eq];fraction / bin", 150, 0, 900);
    hx[ip]->SetDirectory(nullptr);
    long coin[NT] = {0};
    for (Long64_t i = 0; i < N; ++i) {
      t->GetEntry(i);
      auto amp = [&](int s){
        double b = 0; for (int k = 0; k < 200; ++k) b += ch[s][k]; b /= 200;
        float mn = ch[s][0]; for (int k = 0; k < NSAMP; ++k) if (ch[s][k] < mn) mn = ch[s][k];
        return (b - mn) * MV2ADC; };
      double a0 = amp(0), a1 = amp(1);
      hx[ip]->Fill(a0);
      for (int k = 0; k < NT; ++k) { double thr = 20 + 10*k;
        if (a0 > thr && a1 > thr) ++coin[k]; }
    }
    gc[ip] = new TGraph(NT);
    for (int k = 0; k < NT; ++k) gc[ip]->SetPoint(k, 20 + 10*k, 100.0*coin[k]/N);
    hx[ip]->Scale(1.0 / N);
    f->Close();
    printf("%.0f GeV done\n", E[ip]);
  }

  TCanvas c("c", "c", 1700, 640); c.Divide(2, 1, 0.004, 0.004);
  c.cd(1); gPad->SetLogy();
  TLegend *lg = new TLegend(0.60, 0.55, 0.94, 0.88);
  for (int ip = 0; ip < NP; ++ip) {
    gc[ip]->SetLineColor(cols[ip]); gc[ip]->SetLineWidth(3);
    gc[ip]->SetMarkerColor(cols[ip]); gc[ip]->SetMarkerStyle(20); gc[ip]->SetMarkerSize(0.9);
    if (ip == 0) {
      gc[ip]->SetTitle(";coincidence threshold [ADC-eq];XCET40 #cap XCET43 rate [%]");
      gc[ip]->GetXaxis()->SetLimits(0, 300);
      gc[ip]->SetMinimum(0.02); gc[ip]->SetMaximum(200);
      gc[ip]->Draw("APL");
    } else gc[ip]->Draw("PL same");
    lg->AddEntry(gc[ip], Form("%.0f GeV  (cut %.0f)", E[ip], chosen[ip]), "l");
    TLine *lm = new TLine(chosen[ip], 0.02, chosen[ip], gc[ip]->Eval(chosen[ip]));
    lm->SetLineColor(cols[ip]); lm->SetLineStyle(3); lm->SetLineWidth(2); lm->Draw();
  }
  lg->Draw();
  TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(21);
  l.DrawLatex(0.16, 0.86, "tag rate vs threshold  #font[42]{(plateau = clean tag)}");
  c.cd(2); gPad->SetLogy();
  double mx = 0; for (int ip = 0; ip < NP; ++ip) mx = std::max(mx, hx[ip]->GetMaximum());
  for (int ip = NP-1; ip >= 0; --ip) {
    hx[ip]->SetLineColor(cols[ip]); hx[ip]->SetLineWidth(3);
    hx[ip]->SetMaximum(2*mx); hx[ip]->SetMinimum(2e-6);
    hx[ip]->Draw(ip == NP-1 ? "hist" : "hist same");
  }
  l.DrawLatex(0.16, 0.86, "XCET40 spectra  #font[42]{(peak slides with pressure #propto 1/p^{2})}");
  c.SaveAs("Output/scan/XCETScan.png");
  printf("Wrote Output/scan/XCETScan.png\n");
  fflush(nullptr);
  gSystem->Exit(0);
}
