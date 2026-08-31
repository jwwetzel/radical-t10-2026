// PulseShapes.C — mean tagged-electron pulse shape per module, same beam.
// LuAG run 27 and DSB1 run 33 were both taken at -7 GeV: identical beam,
// identical readout — the shape difference is the material.
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TProfile.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include "TH1F.h"
#include "TF1.h"
#include <cstdio>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095, NS = 0.2;   // 5 GS/s
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

static void meanShape(int run, double cthr, TProfile *pr, double &rise, double &tau, double &tfrac)
{
  TFile *f = TFile::Open(Form("data/download/run_%d.root", run));
  TTree *t = (TTree*)f->Get("pulse");
  Float_t ch[NSLOT][NSAMP]; t->SetBranchAddress("channel", ch);
  Long64_t n = t->GetEntries();
  long used = 0;
  for (Long64_t i = 0; i < n && used < 4000; ++i) {
    t->GetEntry(i);
    auto amp=[&](int s,int pol,int be,int &pk){ double b=0; for(int k=0;k<be;++k) b+=ch[s][k]; b/=be;
      double ex=ch[s][0]; pk=0; for(int k=1;k<NSAMP;++k){ if(pol>0? ch[s][k]>ex : ch[s][k]<ex){ex=ch[s][k];pk=k;} }
      return (pol>0? ex-b : b-ex)*MV2ADC; };
    int d;
    if (amp(0,-1,200,d) < cthr || amp(1,-1,200,d) < cthr) continue;
    for (int j = 0; j < 4; ++j) {
      int pk; double a = amp(HGs[j],+1,40,pk);
      if (a < 800 || a > 2800 || pk < 60 || pk > 700) continue;   // clean unclipped prompt pulses
      double b = 0; for (int k = 0; k < 40; ++k) b += ch[HGs[j]][k]; b /= 40;
      for (int s = -25; s < 400; ++s) {
        int k = pk + s; if (k < 0 || k >= NSAMP) continue;
        pr->Fill(s*NS, (ch[HGs[j]][k]-b)*MV2ADC/a);
      }
      ++used;
    }
  }
  f->Close();
  // rise 10->90 (electronics core), tail decay constant and tail fraction
  int b10=-1,b90=-1; int pkb = pr->GetXaxis()->FindBin(0.0);
  for (int b = 1; b <= pkb; ++b) { double v = pr->GetBinContent(b);
    if (b10 < 0 && v > 0.10) b10 = b; if (b90 < 0 && v > 0.90) b90 = b; }
  rise = (b90 > 0 && b10 > 0) ? pr->GetBinCenter(b90)-pr->GetBinCenter(b10) : -1;
  TF1 ex("ex","expo", 8, 45);
  pr->Fit(&ex, "QRN", "", 8, 45);
  tau = -1.0/ex.GetParameter(1);
  double tot = 0, tail = 0;
  for (int b = 1; b <= pr->GetNbinsX(); ++b) { double v = std::max(0.0, pr->GetBinContent(b));
    tot += v; if (pr->GetBinCenter(b) > 8) tail += v; }
  tfrac = tot > 0 ? tail/tot : -1;
  printf("run %d: %ld pulses, rise %.1f ns, tail tau %.1f ns, tail(>8ns) fraction %.1f%%\n",
         run, used, rise, tau, 100*tfrac);
}

void PulseShapes()
{
  SetRadStyle();
  TProfile *pL = new TProfile("pL","",212,-5.0,79.8);   // 0.4 ns bins: 2 samples/bin, no comb  pL->SetDirectory(nullptr);
  TProfile *pD = new TProfile("pD","",212,-5.0,79.8);
  TProfile *pE = new TProfile("pE","",212,-5.0,79.8);  pE->SetDirectory(nullptr);  pD->SetDirectory(nullptr);
  double rL,tauL,tfL,rD,tauD,tfD,rE,tauE,tfE;
  meanShape(27, 50, pL, rL, tauL, tfL);   // LuAG,  -7 GeV
  meanShape(33, 40, pD, rD, tauD, tfD);   // DSB1,  -7 GeV
  meanShape(42, 40, pE, rE, tauE, tfE);   // EJ199, -7 GeV
  TCanvas c("c","c",1500,950);
  TH1F *fr = gPad->DrawFrame(-5, -0.12, 79.8, 1.32,
    ";time from pulse peak [ns];amplitude / peak");
  pL->SetLineColor(rad::cRed());  pL->SetLineWidth(4);
  pD->SetLineColor(rad::cTeal()); pD->SetLineWidth(4);
  pE->SetLineColor(rad::cAmber()); pE->SetLineWidth(4);
  pL->Draw("hist same L"); pD->Draw("hist same L"); pE->Draw("hist same L");
  TLegend *l = new TLegend(0.34,0.52,0.93,0.80); l->SetBorderSize(0); l->SetTextFont(43); l->SetTextSize(24);
  l->AddEntry(pD, Form("DSB1:  tail #tau = %.1f ns,  %.0f%% of light after 8 ns", tauD, 100*tfD), "l");
  l->AddEntry(pL, Form("LuAG:  tail #tau = %.1f ns,  %.0f%% of light after 8 ns", tauL, 100*tfL), "l");
  l->AddEntry(pE, Form("EJ199: tail #tau = %.1f ns,  %.0f%% of light after 8 ns", tauE, 100*tfE), "l");
  l->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);
  tx.SetTextSize(30); tx.DrawLatex(0.13,0.945,"Mean tagged-electron pulse shape by capillary channel");
  tx.SetTextSize(21); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.865,"same #minus7 GeV beam, same LYSO:Ce tiles, same readout (runs 27/33/42) #upoint unclipped, peak-aligned, peak-normalized");
  gSystem->mkdir("Output/summary", true);
  c.SaveAs("Output/summary/PulseShapes.png");
  gSystem->Exit(0);
}
