#include "TFile.h"
#include "TTree.h"
#include "TH2F.h"
#include "TGraph.h"
#include "TCanvas.h"
#include "TLatex.h"
#include "radStyle.h"
void waves2() {
  SetRadStyle();
  TFile *f = TFile::Open("data/run_12.root");
  TTree *t = (TTree*)f->Get("pulse");
  static float ch[18][1024];
  t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
  t->SetBranchAddress("channel", ch);
  int chans[6] = {2, 4, 6, 12, 16, 17};
  const char* lbl[6] = {"ch 2  scint A","ch 4  low gain","ch 6  low gain","ch 12  high gain","ch 16  MCP copy g0","ch 17  MCP copy g1"};
  int cols[8] = {rad::cTeal(), rad::cBlue(), rad::cAmber(), rad::cRed(),
                 rad::cInk(), rad::cGrey(), rad::cTeal(), rad::cRed()};
  TCanvas c("c","c",1900,1000); c.Divide(3,2,0.004,0.004);
  for (int p = 0; p < 6; ++p) {
    c.cd(p+1);
    TH2F *fr = new TH2F(Form("fr%d",p), ";sample;ADC", 10,0,1024, 10,0,4096);
    fr->Draw();
    TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(21);
    l.DrawLatex(0.16,0.86,lbl[p]);
    for (int e = 0; e < 8; ++e) {
      t->GetEntry(100 + e*997);
      TGraph *g = new TGraph(1024);
      for (int s = 0; s < 1024; ++s) g->SetPoint(s, s, ch[chans[p]][s]);
      g->SetLineColor(cols[e%8]); g->SetLineWidth(2); g->Draw("l same");
    }
  }
  c.SaveAs("Output/run_12/example_waveforms.png");
}
