// HowItWorks.C — the measurement in four steps, using MEAN tagged-electron
// waveforms (run 15, 5 GeV) so every panel shows an exemplary signal.
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TProfile.h"
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
  const int LGs[4] = {4,5,6,7};
  // per-channel mean waveform over tagged electrons
  TProfile *pr[NSLOT];
  for (int s = 0; s < NSLOT; ++s) { pr[s] = new TProfile(Form("p%d",s),"",512,0,NSAMP*NS); pr[s]->SetDirectory(nullptr); }
  auto amp=[&](int s,int pol,int be){ double b=0; for(int k=0;k<be;++k) b+=ch[s][k]; b/=be;
    double ex=ch[s][0]; for(int k=1;k<NSAMP;++k){ if(pol>0? ch[s][k]>ex : ch[s][k]<ex) ex=ch[s][k]; }
    return (pol>0? ex-b : b-ex)*MV2ADC; };
  long used = 0;
  for (Long64_t i = 0; i < t->GetEntries() && used < 1500; ++i) {
    t->GetEntry(i);
    if (amp(0,-1,200) < 150 || amp(1,-1,200) < 150) continue;
    double S = 0; for (int j = 0; j < 4; ++j) S += amp(LGs[j],+1,40);
    if (S < 2400 || S > 3600) continue;                       // typical contained 5 GeV shower
    for (int s = 0; s < NSLOT; ++s) {
      double b = 0; int be = (s <= 2) ? 200 : 40;
      for (int k = 0; k < be; ++k) b += ch[s][k]; b /= be;
      double pol = (s==0 || s==1 || s==17 || s==8) ? -1 : +1; // Cherenkovs + MCP negative
      for (int k = 0; k < NSAMP; k += 2) pr[s]->Fill(k*NS, pol*(ch[s][k]-b)*MV2ADC);
    }
    ++used;
  }
  f->Close();
  printf("averaged %ld tagged electrons\n", used);

  TCanvas c("c","c",2000,1150); c.Divide(2,2,0.004,0.006);
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);
  auto head=[&](const char *a, const char *b){
    tx.SetTextSize(24); tx.SetTextColor(kBlack); tx.DrawLatex(0.13,0.945,a);
    tx.SetTextSize(18); tx.SetTextColor(rad::cGrey()); tx.DrawLatex(0.13,0.90,b);
    tx.SetTextColor(kBlack); };

  c.cd(1);
  double m0 = pr[0]->GetMaximum(), m1 = pr[1]->GetMaximum();
  gPad->DrawFrame(0, -0.08*std::max(m0,m1), 205, 1.45*std::max(m0,m1), ";time [ns];amplitude [ADC-eq]");
  pr[0]->SetLineColor(rad::cTeal()); pr[0]->SetLineWidth(3);
  pr[1]->SetLineColor(rad::cBlue()); pr[1]->SetLineWidth(3);
  pr[0]->Draw("hist same L"); pr[1]->Draw("hist same L");
  TLegend *l1 = new TLegend(0.60,0.66,0.93,0.84); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(20);
  l1->AddEntry(pr[0],"XCET040","l"); l1->AddEntry(pr[1],"XCET043","l"); l1->Draw();
  head("#font[62]{1 #upoint Is it an electron?}",
       "both gas Cherenkov counters fire — at this pressure only electrons radiate");

  c.cd(2);
  double mm = pr[17]->GetMaximum();
  gPad->DrawFrame(0, -0.08*mm, 205, 1.45*mm, ";time [ns];amplitude [ADC-eq]");
  pr[17]->SetLineColor(rad::cInk()); pr[17]->SetLineWidth(3); pr[17]->Draw("hist same L");
  head("#font[62]{2 #upoint When did it arrive?}",
       "the MCP is the ~10 ps reference clock; every capillary time is measured against it");

  c.cd(3);
  double ml = 0; for (int j = 0; j < 4; ++j) ml = std::max(ml, pr[LGs[j]]->GetMaximum());
  gPad->DrawFrame(0, -0.08*ml, 205, 1.45*ml, ";time [ns];amplitude [ADC-eq]");
  int cols[4] = {rad::cTeal(), rad::cAmber(), rad::cRed(), rad::cBlue()};
  TLegend *l3 = new TLegend(0.63,0.56,0.93,0.84); l3->SetBorderSize(0); l3->SetTextFont(43); l3->SetTextSize(19);
  for (int j = 0; j < 4; ++j) { pr[LGs[j]]->SetLineColor(cols[j]); pr[LGs[j]]->SetLineWidth(3);
    pr[LGs[j]]->Draw("hist same L"); l3->AddEntry(pr[LGs[j]], Form("capillary %d", LGs[j]), "l"); }
  l3->Draw();
  head("#font[62]{3 #upoint How much energy?}",
       "the shower shares its light over the 2#times2 capillaries; the summed peak #propto energy");

  c.cd(4);
  TProfile *ph = pr[14];                                       // HG partner of capillary 4
  double mh = ph->GetMaximum();
  gPad->DrawFrame(0, -0.08*mh, 205, 1.45*mh, ";time [ns];amplitude [ADC-eq]");
  ph->SetLineColor(rad::cTeal()); ph->SetLineWidth(3); ph->Draw("hist same L");
  TLine lt(0, 0.15*mh, 205, 0.15*mh); lt.SetLineColor(rad::cRed()); lt.SetLineStyle(7); lt.SetLineWidth(3); lt.DrawClone();
  tx.SetTextSize(18); tx.SetTextColor(rad::cRed());
  tx.DrawLatex(0.66, 0.30, "15% threshold");
  tx.SetTextColor(kBlack);
  head("#font[62]{4 #upoint The precise moment}",
       "the high-gain copy crosses 15% of its predicted peak; the shower time = median of 4 capillaries");

  gSystem->mkdir("Output/summary", true);
  c.SaveAs("Output/summary/HowItWorks.png");
  gSystem->Exit(0);
}
