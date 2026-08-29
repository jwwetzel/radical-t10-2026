#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "radStyle.h"
void AssessRun25() {
  SetRadStyle();
  TFile *f = TFile::Open("data/download/run_25.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][1024];
  int evnum;
  t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1); t->SetBranchStatus("event",1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("event", &evnum);
  Long64_t N = t->GetEntries();
  long gaps=0, nTag=0, nX40=0, nX43=0;
  int prev=-1;
  TH1F *hSe = new TH1F("hSe","tagged electrons;#Sigma LG [ADC-eq];events",100,0,2000);
  TH1F *hSo = new TH1F("hSo","",100,0,2000);
  TH1F *hX0 = new TH1F("hX0","XCET40 amplitude;amp [ADC-eq];events",100,0,3000);
  TH1F *hX1 = (TH1F*)hX0->Clone("hX1");
  long eOnMod=0, hadMod=0, nMiss=0;
  for (Long64_t i=0;i<N;++i){
    t->GetEntry(i);
    if (prev>=0 && evnum!=prev+1) ++gaps;
    prev=evnum;
    auto amp=[&](int s,int be,int pol){
      double b=0; for(int k=0;k<be;++k) b+=ch[s][k]; b/=be;
      float ex=ch[s][0]; for(int k=0;k<1024;++k){float v=ch[s][k]; if(pol>0?v>ex:v<ex) ex=v;}
      return (pol>0?ex-b:b-ex)*4.095; };
    double a0=amp(0,200,-1), a1=amp(1,200,-1);
    hX0->Fill(a0); hX1->Fill(a1);
    bool tag = a0>150 && a1>150;
    if (a0>150) ++nX40; if (a1>150) ++nX43;
    if (tag) ++nTag;
    double S=0; for(int j=4;j<=7;++j) S+=amp(j,40,+1);
    (tag?hSe:hSo)->Fill(S);
    if (tag && S<120) ++nMiss;
    if (tag && S>250) ++eOnMod;          // on-module electrons (peak region, see spectrum)
    if (!tag && S>250) ++hadMod;
  }
  printf("entries %lld (manifest 3264), event-number gaps %ld\n", N, gaps);
  printf("XCET40 fired %.1f%%, XCET43 fired %.1f%%, coincidence %.1f%% (%ld)\n",
         100.*nX40/N, 100.*nX43/N, 100.*nTag/N, nTag);
  printf("tagged & on-module (SumLG>250): %ld;  tagged & SumLG<120 (miss): %ld\n", eOnMod, nMiss);
  printf("untagged & on-module (hadron/MIP sample): %ld\n", hadMod);
  TCanvas c("c","c",1500,600); c.Divide(2,1,0.004,0.004);
  c.cd(1);
  hSo->SetLineColor(rad::cBlue()); hSo->SetLineWidth(3);
  hSe->SetLineColor(rad::cRed()); hSe->SetFillColor(rad::cBand()); hSe->SetLineWidth(3);
  hSe->Draw("hist"); hSo->Draw("hist same");
  TLegend *lg=new TLegend(0.55,0.7,0.94,0.86);
  lg->AddEntry(hSe,"XCET tagged","lf"); lg->AddEntry(hSo,"untagged","l"); lg->Draw();
  c.cd(2); gPad->SetLogy();
  hX0->SetLineColor(rad::cTeal()); hX0->SetLineWidth(3); hX0->Draw("hist");
  hX1->SetLineColor(rad::cAmber()); hX1->SetLineWidth(3); hX1->Draw("hist same");
  TLegend *l2=new TLegend(0.6,0.7,0.94,0.86);
  l2->AddEntry(hX0,"XCET 40","l"); l2->AddEntry(hX1,"XCET 43","l"); l2->Draw();
  c.SaveAs("Output/run_25/assess.png");
}
