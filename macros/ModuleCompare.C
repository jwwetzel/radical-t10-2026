// ModuleCompare.C — cross-module summary figures from the FINAL scan fits.
// Numbers are the published per-point results of EnergyScan.C (LuAG) and
// EnergyScanDSB1.C (DSB1); this macro documents them side by side.
#include "radStyle.h"
#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TF1.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include "TH1F.h"
#include "TPad.h"

void ModuleCompare()
{
  SetRadStyle();
  // LuAG (runs 2526,24,15,27,9001,30) — median timing, contained showers
  double eL[6]  = {1,3,5,7,9,11};
  double tL[6]  = {412,274,228,191,189,227}, tLe[6] = {11,5,6,7,12,32};
  double pL[6]  = {0,1686,2962,4814,5782,5696}, pLe[6] = {0,25,37,51,160,117};
  // DSB1 (runs 36,35,33,32,31) — 5 GeV pending run 37
  double eD[6]  = {1,3,5,7,9,11};
  double tD[6]  = {366,202,156,140,133,181}, tDe[6] = {4,4,8,6,5,17};
  double pD[6]  = {0,3763,6769,9045,9946,10721}, pDe[6] = {0,73,138,90,144,734};
  // EJ199 (runs 39-44) — 11 GeV response fit unstable (202 e): timing kept (open), response omitted
  double eJ[6]  = {1,3,5,7,9,11};
  double tJ[6]  = {1367,626,380,278,249,266}, tJe[6] = {24,13,18,10,17,37};
  double pJ[4]  = {1354,2740,4658,5704}, pJe[4] = {42,74,97,159};
  double eJr[4] = {3,5,7,9};

  auto mkG = [](int n, double *x, double *y, double *ye, int col, int mk){
    TGraphErrors *g = new TGraphErrors(n);
    for (int i = 0; i < n; ++i){ g->SetPoint(i,x[i],y[i]); g->SetPointError(i,0,ye[i]); }
    g->SetMarkerColor(col); g->SetLineColor(col); g->SetMarkerStyle(mk); g->SetMarkerSize(1.6);
    g->SetLineWidth(2); return g; };

  // ---------- hero: timing ----------
  TCanvas ch("ch","ch",1500,950);
  TH1F *fr = gPad->DrawFrame(0, 0, 12.3, 530,
    ";beam energy [GeV];shower-time resolution  #sigma_{t}  [ps]");
  TF1 *fL = new TF1("fL","sqrt(402*402/x+134*134)",0.7,12.3);
  TF1 *fD = new TF1("fD","361/sqrt(x)",0.7,12.3);
  fL->SetLineColor(rad::cRed());  fL->SetLineWidth(3); fL->SetLineStyle(7);
  fD->SetLineColor(rad::cTeal()); fD->SetLineWidth(4);
  fL->Draw("same"); fD->Draw("same");
  TGraphErrors *gL = mkG(6,eL,tL,tLe,rad::cRed(),21);
  TGraphErrors *gD = mkG(6,eD,tD,tDe,rad::cTeal(),20);
  gL->Draw("P same"); gD->Draw("P same");
  // 11 GeV points open: e- purity uncertain above ~10 GeV/c (T10 composition)
  auto open11 = [&](double x, double y, int col, int solid, int openmk){
    TGraph *w = new TGraph(1); w->SetPoint(0,x,y); w->SetMarkerStyle(solid);
    w->SetMarkerColor(kWhite); w->SetMarkerSize(1.45); w->Draw("P same");
    TGraph *o = new TGraph(1); o->SetPoint(0,x,y); o->SetMarkerStyle(openmk);
    o->SetMarkerColor(col); o->SetMarkerSize(1.6); o->Draw("P same"); };
  open11(eL[5], tL[5], rad::cRed(), 21, 25); open11(eD[5], tD[5], rad::cTeal(), 20, 24);
  TLegend *l = new TLegend(0.42,0.62,0.93,0.90);
  l->SetBorderSize(0); l->SetTextFont(43); l->SetTextSize(26);
  l->AddEntry(gD, "DSB1:  #sigma_{t} = (361#pm4) ps/#sqrt{E}   (b unresolved)", "pl");
  l->AddEntry(gL, "LuAG:  #sigma_{t} = (402#pm13)/#sqrt{E} #oplus (134#pm11) ps", "pl");
  l->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);
  tx.SetTextSize(30); tx.DrawLatex(0.13,0.945,"Shower timing, 14#times14 mm shower-max sampling modules");
  tx.SetTextSize(21); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.921,"CERN PS T10, tagged electrons, 1#font[122]{-}11 GeV #upoint reference included, unsubtracted #upoint open points: e^{#font[122]{-}} purity uncertain above ~10 GeV/c");
  gSystem->mkdir("Output/summary", true);
  ch.SaveAs("Output/summary/Hero_timing.png");

  // ---------- two-panel: timing + response (3 modules; log-y timing) ----------
  TGraphErrors *gJ = mkG(6,eJ,tJ,tJe,rad::cAmber(),22);
  TGraphErrors *qJ = mkG(4,eJr,pJ,pJe,rad::cAmber(),22);
  TF1 *fJ = new TF1("fJ","1218/sqrt(x)",0.7,12.3);
  fJ->SetLineColor(rad::cAmber()); fJ->SetLineWidth(2); fJ->SetLineStyle(3);
  TCanvas c2("c2","c2",2000,900); c2.Divide(2,1,0.004,0.004);
  c2.cd(1); gPad->SetLogy();
  TH1F *fr1 = gPad->DrawFrame(0, 80, 12.3, 1900, ";beam energy [GeV];#sigma_{t} [ps]");
  fL->Draw("same"); fD->Draw("same");
  gL->Draw("P same"); gD->Draw("P same"); gJ->Draw("P same");
  open11(eL[5], tL[5], rad::cRed(), 21, 25); open11(eD[5], tD[5], rad::cTeal(), 20, 24);
  open11(eJ[5], tJ[5], rad::cAmber(), 22, 26);
  TLegend *l1 = new TLegend(0.36,0.62,0.93,0.90); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(22);
  l1->AddEntry(gD, "DSB1: (361#pm4)/#sqrt{E} ps", "pl");
  l1->AddEntry(gL, "LuAG: (402#pm13)/#sqrt{E} #oplus (134#pm11) ps", "pl");
  l1->AddEntry(gJ, "EJ199: photostatistics-limited (no stable trend)", "p");
  l1->Draw();
  c2.cd(2);
  TH1F *fr2 = gPad->DrawFrame(0, 0, 12.3, 12500, ";beam energy [GeV];#SigmaLG peak [ADC-eq]");
  TGraphErrors *qL = mkG(5,&eL[1],&pL[1],&pLe[1],rad::cRed(),21);
  TGraphErrors *qD = mkG(5,&eD[1],&pD[1],&pDe[1],rad::cTeal(),20);
  qL->Draw("P same"); qD->Draw("P same"); qJ->Draw("P same");
  open11(eL[5], pL[5], rad::cRed(), 21, 25); open11(eD[5], pD[5], rad::cTeal(), 20, 24);
  TLegend *l2 = new TLegend(0.16,0.70,0.72,0.90); l2->SetBorderSize(0); l2->SetTextFont(43); l2->SetTextSize(22);
  l2->AddEntry(qD, "DSB1  (~2.1#times the light)", "p");
  l2->AddEntry(qL, "LuAG", "p");
  l2->AddEntry(qJ, "EJ199  (11 GeV fit unstable, omitted)", "p");
  l2->Draw();
  c2.SaveAs("Output/summary/ModuleCompare.png");
  printf("Wrote Output/summary/Hero_timing.png + ModuleCompare.png\n");
  gSystem->Exit(0);
}
