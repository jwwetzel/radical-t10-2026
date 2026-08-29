// MCPThreshold.C — measure the effective trigger threshold from the MCP
// amplitude spectrum: the DAQ triggers on TR0, so the recorded MCP pulses
// have a hard minimum amplitude = the discriminator threshold.
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include "radStyle.h"
void MCPThreshold(int run = 15)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *f = TFile::Open(run <= 13 ? TString::Format("data/run_%d.root", run)
                                   : TString::Format("data/download/run_%d.root", run));
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][1024];
  t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
  t->SetBranchAddress("channel", ch);
  const double toADC = run <= 13 ? 1.0 : 4.095;
  const int sA = run <= 13 ? 16 : 8, sB = 17, bwin = run <= 13 ? 80 : 40;
  TH1F *hA = new TH1F("hA", ";MCP peak amplitude [ADC-eq];events", 250, 0, 4000);
  TH1F *hB = (TH1F*)hA->Clone("hB");
  for (Long64_t i = 0; i < t->GetEntries(); ++i) {
    t->GetEntry(i);
    auto amp = [&](int s){
      double b=0; for(int k=0;k<bwin;++k) b+=ch[s][k]; b/=bwin;
      float mn=ch[s][0]; for(int k=0;k<1024;++k) if(ch[s][k]<mn) mn=ch[s][k];
      return (b-mn)*toADC; };
    hA->Fill(amp(sA)); hB->Fill(amp(sB));
  }
  // threshold = turn-on edge: first bin (above noise region) exceeding 5% of peak
  auto edge = [&](TH1F *h){
    double pk = h->GetMaximum();
    for (int b = 3; b <= h->GetNbinsX(); ++b)
      if (h->GetBinContent(b) > 0.05*pk) return h->GetBinLowEdge(b);
    return 0.0; };
  double eA = edge(hA), eB = edge(hB);
  TCanvas c("c","c",1500,620); c.Divide(2,1,0.004,0.004);
  for (int p = 0; p < 2; ++p) {
    c.cd(p+1); gPad->SetLogy();
    TH1F *h = p ? hB : hA; double e = p ? eB : eA;
    if (p == 1) h->GetXaxis()->SetRangeUser(0, 2000);   // zoom on the turn-on
    h->SetLineColor(rad::cInk()); h->SetFillColor(rad::cFill());
    h->Draw("hist");
    TLine *l = new TLine(e, 0.7, e, h->GetMaximum()*1.2);
    l->SetLineColor(rad::cRed()); l->SetLineWidth(3); l->Draw();
    TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(20);
    tx.DrawLatex(0.17, 0.86, p ? "TR0 group 1  (zoom)" : "TR0 group 0");
    tx.SetTextColor(rad::cRed()); tx.SetTextSize(18);
    tx.DrawLatex(0.17, 0.79, Form("turn-on: %.0f ADC-eq = %.0f mV", e, e/4.095));
  }
  c.SaveAs(outDir + "/MCPThreshold.png");
  printf("run %d: TR0 g0 turn-on %.0f ADC-eq (%.0f mV), TR0 g1 %.0f ADC-eq (%.0f mV)\n",
         run, eA, eA/4.095, eB, eB/4.095);
  // sharpness: quantiles of the edge
  double q[3] = {0.005, 0.01, 0.05}, vA[3], vB[3];
  hA->GetQuantiles(3, vA, q); hB->GetQuantiles(3, vB, q);
  printf("  g0 quantiles 0.5/1/5%%: %.0f / %.0f / %.0f ADC-eq\n", vA[0], vA[1], vA[2]);
  printf("  g1 quantiles 0.5/1/5%%: %.0f / %.0f / %.0f ADC-eq\n", vB[0], vB[1], vB[2]);
}
