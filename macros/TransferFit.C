// TransferFit.C — HG-vs-LG transfer calibration with the clip wall made
// explicit (run-14+ format). Top row: HG beam-amplitude spectra showing the
// saturation wall; bottom row: HG vs LG with the fitted window shaded, the
// fit solid inside its range, dashed where extrapolated, and the wall marked.
// Convention follows jwwetzel/radical reduce/Reducer.C: fit only events with
// HG below the linear ceiling (repo: 30-700 mV of an ~820 mV shelf; here the
// wall is measured per channel and the ceiling is 0.72 x wall).
//
// Usage: root -l -b -q 'macros/TransferFit.C+(15)'
// Writes Output/run_<N>/TransferFit.png and TransferFit_summary.txt

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TBox.h"
#include "TLine.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include <cmath>
#include "radStyle.h"

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

void TransferFit(int run = 15)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(TString::Format("data/download/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("no file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NSLOT][NSAMP];
  t->SetBranchStatus("*", 0); t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t nEnt = t->GetEntries();

  auto amp = [&](int s, int be, int pol){
    double b = 0; for (int k = 0; k < be; ++k) b += ch[s][k]; b /= be;
    float ex = ch[s][0];
    for (int k = 0; k < NSAMP; ++k){ float v = ch[s][k]; if (pol>0 ? v>ex : v<ex) ex = v; }
    return (pol>0 ? ex-b : b-ex) * MV2ADC; };

  TH1F *hAmp[4]; TH2F *hHL[4];
  for (int j = 0; j < 4; ++j) {
    hAmp[j] = new TH1F(Form("hAmp_%d", j),
      Form(";HG peak [ADC-eq];events", LGs[j] * 0), 128, 0, 3200);
    hHL[j] = new TH2F(Form("hHL_%d", j),
      Form(";LG peak [ADC-eq];HG peak [ADC-eq]", LGs[j] * 0), 120, 0, 1200, 128, 0, 3200);
  }
  // pass 1: fill everything (beam events only)
  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    if (!(amp(0,BASE_CTR,-1) > 150 && amp(1,BASE_CTR,-1) > 150)) continue;
    for (int j = 0; j < 4; ++j) {
      double l = amp(LGs[j], BASE_MOD, +1), h = amp(HGs[j], BASE_MOD, +1);
      hAmp[j]->Fill(h); hHL[j]->Fill(l, h);
    }
    if (i % 5000 == 0) printf("  %lld/%lld\n", i, nEnt);
  }

  FILE *sum = fopen(outDir + "/TransferFit_summary.txt", "w");
  fprintf(sum, "TransferFit run %d (wall-aware; fit only HG < 0.72 x wall)\n", run);

  TCanvas c("c", "c", 2100, 1000); c.Divide(4, 2, 0.004, 0.008);
  double wall[4], fitMaxHG[4], p0[4], p1[4], lgMax[4];
  TProfile *prs[4];
  for (int j = 0; j < 4; ++j) {
    // wall = 99.5% quantile of the HG beam spectrum (the clip shelf)
    double q = 0.995; hAmp[j]->GetQuantiles(1, &wall[j], &q);
    fitMaxHG[j] = 0.72 * wall[j];
    // transfer fit first (no drawing side effects): linear region only
    prs[j] = hHL[j]->ProfileX(Form("pr_%d", j));
    lgMax[j] = 1200;
    for (int b = prs[j]->FindBin(60); b <= prs[j]->GetNbinsX(); ++b)
      if (prs[j]->GetBinEntries(b) > 3 && prs[j]->GetBinContent(b) > fitMaxHG[j]) { lgMax[j] = prs[j]->GetBinCenter(b); break; }
    TF1 *fl = new TF1(Form("fl%d", j), "pol1", 30, lgMax[j]);
    prs[j]->Fit(fl, "QRN", "", 30, lgMax[j]);
    p0[j] = fl->GetParameter(0); p1[j] = fl->GetParameter(1);
  }
  for (int j = 0; j < 4; ++j) {
    // ---- top row: amplitude spectrum with wall + fit ceiling ----
    c.cd(j + 1); gPad->SetLogy();
    hAmp[j]->SetLineColor(rad::cInk()); hAmp[j]->SetFillColor(rad::cFill());
    hAmp[j]->Draw("hist");
    TLatex hd; hd.SetTextFont(43); hd.SetTextSize(20); hd.SetNDC();
    hd.DrawLatex(0.16, 0.86, Form("capillary %d  #font[42]{high gain, beam}", LGs[j]));
    TLine *lw = new TLine(wall[j], 0.7, wall[j], hAmp[j]->GetMaximum()*1.1);
    lw->SetLineColor(rad::cRed()); lw->SetLineWidth(3); lw->Draw();
    TLine *lf = new TLine(fitMaxHG[j], 0.7, fitMaxHG[j], hAmp[j]->GetMaximum()*1.1);
    lf->SetLineColor(rad::cAmber()); lf->SetLineWidth(3); lf->SetLineStyle(2); lf->Draw();
    TLatex tx; tx.SetTextFont(43); tx.SetTextSize(17);
    tx.SetTextColor(rad::cRed());
    tx.DrawLatex(wall[j]*0.72, hAmp[j]->GetMaximum()*0.5, Form("wall %.0f", wall[j]));

    // ---- bottom row: scatter + profile + fit window ----
    TProfile *pr = prs[j];
    c.cd(j + 5);
    hHL[j]->Draw("col");
    TLatex hd2; hd2.SetTextFont(43); hd2.SetTextSize(20); hd2.SetNDC();
    TBox *bx = new TBox(30, 0, lgMax[j], 3200);
    bx->SetFillColor(rad::cBand()); bx->Draw();
    TLine *lw2 = new TLine(0, wall[j], 1200, wall[j]);   // clip wall
    lw2->SetLineColor(rad::cRed()); lw2->SetLineWidth(2); lw2->Draw();
    pr->SetMarkerStyle(20); pr->SetMarkerSize(0.75);
    pr->SetMarkerColor(rad::cInk()); pr->SetLineColor(rad::cInk()); pr->SetLineWidth(2);
    pr->Draw("same");
    TF1 *fIn = new TF1(Form("fin%d", j), "pol1", 30, lgMax[j]);
    fIn->SetParameters(p0[j], p1[j]); fIn->SetLineColor(rad::cTeal()); fIn->SetLineWidth(4); fIn->Draw("same");
    TF1 *fEx = new TF1(Form("fex%d", j), "pol1", lgMax[j], 1200);
    fEx->SetParameters(p0[j], p1[j]); fEx->SetLineColor(rad::cTeal()); fEx->SetLineWidth(3);
    fEx->SetLineStyle(7); fEx->Draw("same");
    TLatex t2; t2.SetTextFont(43); t2.SetTextSize(17); t2.SetTextColor(rad::cInk());
    t2.DrawLatex(60, 2950, Form("HG = %.0f + %.2f#upointLG", p0[j], p1[j]));
    t2.SetTextColor(rad::cAmber());
    t2.DrawLatex(60, 2720, Form("fit: LG 30#font[122]{-}%.0f  (HG < 0.72#upointwall)", lgMax[j]));
    if (j == 0) {
      TLegend *lg = new TLegend(0.55, 0.17, 0.94, 0.36);
      lg->AddEntry(fIn, "fit (linear region)", "l");
      lg->AddEntry(fEx, "extrapolation", "l");
      lg->AddEntry(lw2, "clip wall", "l");
      lg->Draw();
    }
    fprintf(sum, "cap %d: wall %.0f  fit HG<%.0f (LG 30-%.0f)  HG = %.1f + %.3f*LG\n",
            LGs[j], wall[j], fitMaxHG[j], lgMax[j], p0[j], p1[j]);
    printf("cap %d: wall %.0f, fit LG 30-%.0f, HG = %.1f + %.3f*LG\n",
           LGs[j], wall[j], lgMax[j], p0[j], p1[j]);
  }
  c.SaveAs(outDir + "/TransferFit.png");
  fclose(sum);
  printf("Wrote %s/TransferFit.png\n", outDir.Data());
}
