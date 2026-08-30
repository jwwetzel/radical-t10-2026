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
  double eD[5]  = {1,3,7,9,11};
  double tD[5]  = {363,202,140,133,181}, tDe[5] = {6,4,6,5,17};
  double pD[5]  = {0,3763,9045,9946,10721}, pDe[5] = {0,73,90,144,734};

  auto mkG = [](int n, double *x, double *y, double *ye, int col, int mk){
    TGraphErrors *g = new TGraphErrors(n);
    for (int i = 0; i < n; ++i){ g->SetPoint(i,x[i],y[i]); g->SetPointError(i,0,ye[i]); }
    g->SetMarkerColor(col); g->SetLineColor(col); g->SetMarkerStyle(mk); g->SetMarkerSize(1.6);
    g->SetLineWidth(2); return g; };

  // ---------- hero: timing ----------
  TCanvas ch("ch","ch",1500,950);
  TH1F *fr = gPad->DrawFrame(0, 0, 12.3, 470,
    ";beam energy [GeV];shower-time resolution  #sigma_{t}  [ps]");
  TF1 *fL = new TF1("fL","sqrt(389*389/x+138*138)",0.7,12.3);
  TF1 *fD = new TF1("fD","sqrt(323*323/x+79*79)",0.7,12.3);
  fL->SetLineColor(rad::cRed());  fL->SetLineWidth(3); fL->SetLineStyle(7);
  fD->SetLineColor(rad::cTeal()); fD->SetLineWidth(4);
  fL->Draw("same"); fD->Draw("same");
  TGraphErrors *gL = mkG(6,eL,tL,tLe,rad::cRed(),24);
  TGraphErrors *gD = mkG(5,eD,tD,tDe,rad::cTeal(),20);
  gL->Draw("P same"); gD->Draw("P same");
  TLegend *l = new TLegend(0.42,0.62,0.93,0.90);
  l->SetBorderSize(0); l->SetTextFont(43); l->SetTextSize(26);
  l->AddEntry(gD, "DSB1:  #sigma_{t} = 323 ps/#sqrt{E} #oplus 79 ps", "pl");
  l->AddEntry(gL, "LuAG:  #sigma_{t} = 389 ps/#sqrt{E} #oplus 138 ps", "pl");
  l->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);
  tx.SetTextSize(30); tx.DrawLatex(0.13,0.945,"Shower timing, 14#times14 mm shower-max sampling modules");
  tx.SetTextSize(21); tx.SetTextColor(rad::cGrey());
  tx.DrawLatex(0.13,0.905,"CERN PS T10, tagged electrons, 1#font[122]{-}11 GeV #upoint MCP + DRS reference included, unsubtracted #upoint median 4-capillary combination");
  gSystem->mkdir("Output/summary", true);
  ch.SaveAs("Output/summary/Hero_timing.png");

  // ---------- two-panel: timing + response ----------
  TCanvas c2("c2","c2",2000,900); c2.Divide(2,1,0.004,0.004);
  c2.cd(1);
  TH1F *fr1 = gPad->DrawFrame(0, 0, 12.3, 470, ";beam energy [GeV];#sigma_{t} [ps]");
  fL->Draw("same"); fD->Draw("same"); gL->Draw("P same"); gD->Draw("P same");
  TLegend *l1 = new TLegend(0.38,0.66,0.93,0.90); l1->SetBorderSize(0); l1->SetTextFont(43); l1->SetTextSize(22);
  l1->AddEntry(gD, "DSB1: 323/#sqrt{E} #oplus 79 ps", "pl");
  l1->AddEntry(gL, "LuAG: 389/#sqrt{E} #oplus 138 ps", "pl");
  l1->Draw();
  c2.cd(2);
  TH1F *fr2 = gPad->DrawFrame(0, 0, 12.3, 12500, ";beam energy [GeV];#SigmaLG peak [ADC-eq]");
  TGraphErrors *qL = mkG(5,&eL[1],&pL[1],&pLe[1],rad::cRed(),24);
  TGraphErrors *qD = mkG(4,&eD[1],&pD[1],&pDe[1],rad::cTeal(),20);
  qL->Draw("P same"); qD->Draw("P same");
  TLegend *l2 = new TLegend(0.16,0.70,0.72,0.90); l2->SetBorderSize(0); l2->SetTextFont(43); l2->SetTextSize(22);
  l2->AddEntry(qD, "DSB1  (~2.1#times the light)", "p");
  l2->AddEntry(qL, "LuAG", "p");
  l2->Draw();
  c2.SaveAs("Output/summary/ModuleCompare.png");
  printf("Wrote Output/summary/Hero_timing.png + ModuleCompare.png\n");
  gSystem->Exit(0);
}
