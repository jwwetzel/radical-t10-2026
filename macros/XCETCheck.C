// XCETCheck.C — XCET amplitude spectra and coincidence vs threshold.
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include "radStyle.h"
void XCETCheck(int run = 27)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *f = TFile::Open(TString::Format("data/download/run_%d.root", run));
  TTree *t = (TTree*)f->Get("pulse");
  const double toADC = run <= 13 ? 1.0 : 4.095;
  static float ch[18][1024];
  t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
  t->SetBranchAddress("channel", ch);
  TH1F *h0 = new TH1F("h0", ";XCET amplitude [ADC-eq];events", 150, 0, 600);
  TH1F *h1 = (TH1F*)h0->Clone("h1");
  const int NT = 6; double thr[NT] = {40, 60, 80, 100, 150, 200};
  long coin[NT] = {0}, s40[NT] = {0}, s43[NT] = {0};
  Long64_t N = t->GetEntries();
  for (Long64_t i = 0; i < N; ++i) {
    t->GetEntry(i);
    auto amp=[&](int s){
      double b=0; for(int k=0;k<200;++k) b+=ch[s][k]; b/=200;
      float mn=ch[s][0]; for(int k=0;k<1024;++k) if(ch[s][k]<mn) mn=ch[s][k];
      return (b-mn)*toADC; };
    double a0=amp(0), a1=amp(1);
    h0->Fill(a0); h1->Fill(a1);
    for (int k=0;k<NT;++k){ if(a0>thr[k])++s40[k]; if(a1>thr[k])++s43[k];
      if(a0>thr[k]&&a1>thr[k])++coin[k]; }
  }
  printf("run %d, %lld events — XCET rates vs threshold:\n", run, N);
  printf("%8s %10s %10s %12s\n","thr","XCET40","XCET43","coincidence");
  for (int k=0;k<NT;++k)
    printf("%8.0f %9.2f%% %9.2f%% %11.2f%%\n", thr[k],
           100.*s40[k]/N, 100.*s43[k]/N, 100.*coin[k]/N);
  TCanvas c("c","c",1400,600); c.Divide(2,1,0.004,0.004);
  for (int p=0;p<2;++p){
    c.cd(p+1); gPad->SetLogy();
    TH1F *h = p? h1:h0;
    h->SetLineColor(rad::cTeal()); h->SetLineWidth(3); h->Draw("hist");
    TLine *l=new TLine(150,0.7,150,h->GetMaximum()); l->SetLineColor(rad::cRed());
    l->SetLineWidth(3); l->SetLineStyle(2); l->Draw();
    TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(20);
    tx.DrawLatex(0.17,0.86, p? "XCET 43  (0.21 bar)":"XCET 40  (0.21 bar)");
    tx.SetTextColor(rad::cRed()); tx.SetTextSize(17);
    tx.DrawLatex(0.17,0.79,"dashed: current 150 ADC-eq cut");
  }
  c.SaveAs(outDir + "/XCETCheck.png");
}
