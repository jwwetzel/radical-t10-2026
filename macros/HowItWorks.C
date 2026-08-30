// HowItWorks.C — one real tagged-electron event from run 15, annotated as a
// four-step walkthrough of the measurement. For the run book summary page.
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TGraph.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include "TH1F.h"
#include <cstdio>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095, NS = 0.2;

void HowItWorks()
{
  SetRadStyle();
  TFile *f = TFile::Open("data/download/run_15.root");
  TTree *t = (TTree*)f->Get("pulse");
  Float_t ch[NSLOT][NSAMP]; t->SetBranchAddress("channel", ch);
  const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};
  auto amp=[&](int s,int pol,int be){ double b=0; for(int k=0;k<be;++k) b+=ch[s][k]; b/=be;
    double ex=ch[s][0]; for(int k=1;k<NSAMP;++k){ if(pol>0? ch[s][k]>ex : ch[s][k]<ex) ex=ch[s][k]; }
    return (pol>0? ex-b : b-ex)*MV2ADC; };
  // pick a photogenic event: tagged, mid-range contained shower, no HG clip
  Long64_t pick = -1;
  for (Long64_t i = 0; i < t->GetEntries(); ++i) {
    t->GetEntry(i);
    if (amp(0,-1,200) < 300 || amp(1,-1,200) < 300) continue;
    double S = 0; bool clip = false;
    for (int j = 0; j < 4; ++j) { S += amp(LGs[j],+1,40); if (amp(HGs[j],+1,40) > 2900) clip = true; }
    if (clip || S < 2800 || S > 3300) continue;
    pick = i; break;
  }
  printf("event %lld\n", pick);
  t->GetEntry(pick);
  auto base=[&](int s,int be){ double b=0; for(int k=0;k<be;++k) b+=ch[s][k]; b/=be; return b; };
  auto graphOf=[&](int s,int pol,int be,int k0,int k1){ double b=base(s,be);
    TGraph *g = new TGraph();
    for (int k = k0; k < k1; ++k) g->SetPoint(g->GetN(), k*NS, (pol>0?1:-1)*(ch[s][k]-b)*MV2ADC);
    g->SetLineWidth(3); return g; };

  int K0 = 20, K1 = 260;
  TCanvas c("c","c",2000,1150); c.Divide(2,2,0.004,0.006);
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);

  c.cd(1);
  TGraph *g0 = graphOf(0,-1,200,K0,K1), *g1 = graphOf(1,-1,200,K0,K1);
  TH1F *f1 = gPad->DrawFrame(K0*NS, -80, K1*NS, 1.15*std::max(g0->GetYaxis()->GetXmax(), 900.), ";time [ns];amplitude [ADC-eq]");
  g0->SetLineColor(rad::cTeal()); g1->SetLineColor(rad::cBlue());
  g0->Draw("L same"); g1->Draw("L same");
  TLegend *l1 = new TLegend(0.55,0.72,0.93,0.88); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(20);
  l1->AddEntry(g0,"XCET040","l"); l1->AddEntry(g1,"XCET043","l"); l1->Draw();
  tx.SetTextSize(24); tx.DrawLatex(0.13,0.94,"#font[62]{1 #upoint Is it an electron?}");
  tx.SetTextSize(19); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.895,"both gas Cherenkov counters fire #rightarrow only electrons radiate at this pressure");
  tx.SetTextColor(kBlack);

  c.cd(2);
  TGraph *gm = graphOf(17,-1,40,K0,K1);
  TH1F *f2 = gPad->DrawFrame(K0*NS, -100, K1*NS, 1900, ";time [ns];amplitude [ADC-eq]");
  gm->SetLineColor(rad::cInk()); gm->Draw("L same");
  tx.SetTextSize(24); tx.DrawLatex(0.13,0.94,"#font[62]{2 #upoint When did it arrive?}");
  tx.SetTextSize(19); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.895,"the MCP is the ~10 ps reference clock: every capillary time is measured against it");
  tx.SetTextColor(kBlack);

  c.cd(3);
  TH1F *f3 = gPad->DrawFrame(K0*NS, -60, K1*NS, 1250, ";time [ns];amplitude [ADC-eq]");
  int cols[4] = {rad::cTeal(), rad::cAmber(), rad::cRed(), rad::cBlue()};
  TLegend *l3 = new TLegend(0.62,0.60,0.93,0.88); l3->SetBorderSize(0); l3->SetTextFont(43); l3->SetTextSize(19);
  double S = 0;
  for (int j = 0; j < 4; ++j) {
    TGraph *g = graphOf(LGs[j],+1,40,K0,K1);
    g->SetLineColor(cols[j]); g->Draw("L same");
    double a = amp(LGs[j],+1,40); S += a;
    l3->AddEntry(g, Form("capillary %d  (%.0f)", LGs[j], a), "l");
  }
  l3->Draw();
  tx.SetTextSize(24); tx.DrawLatex(0.13,0.94,"#font[62]{3 #upoint How much energy?}");
  tx.SetTextSize(19); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.895,Form("the shower spreads over the 2#times2 capillaries; #SigmaLG = %.0f ADC-eq #propto energy", S));
  tx.SetTextColor(kBlack);

  c.cd(4);
  TGraph *gh = graphOf(HGs[0],+1,40,K0,K1);
  double aH = amp(HGs[0],+1,40);
  TH1F *f4 = gPad->DrawFrame(K0*NS, -0.1*aH, K1*NS, 1.25*aH, ";time [ns];amplitude [ADC-eq]");
  gh->SetLineColor(rad::cTeal()); gh->Draw("L same");
  double thr = 0.15*aH;
  TLine lt(K0*NS, thr, K1*NS, thr); lt.SetLineColor(rad::cRed()); lt.SetLineStyle(7); lt.SetLineWidth(3); lt.DrawClone();
  tx.SetTextSize(24); tx.DrawLatex(0.13,0.94,"#font[62]{4 #upoint The precise moment}");
  tx.SetTextSize(19); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.895,"the high-gain copy crosses a threshold at 15% of its (predicted) peak #rightarrow #Deltat to the MCP; median of 4 capillaries = shower time");
  tx.SetTextColor(kBlack);

  gSystem->mkdir("Output/summary", true);
  c.SaveAs("Output/summary/HowItWorks.png");
  f->Close();
  gSystem->Exit(0);
}
