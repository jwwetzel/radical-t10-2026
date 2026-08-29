// ElectronID.C — offline electron selection for the >7 GeV scan points,
// developed tag-and-probe style on the XCET-tagged runs (1/3/5 GeV).
//
// Discriminating variables (chosen to survive at energies where the XCET
// tag does not exist):
//   share  = max single-capillary LG / Sigma LG   (shower spreads, MIP doesn't)
//   ratio  = Sigma HG_prompt / Sigma LG_slow      (prompt vs slow light balance)
//   sumLG  = Sigma LG                              (energy-dependent: window
//                                                   scales with beam energy)
//   mcp    = TR0 amplitude                         (species-correlated here)
//
// On a tagged run this macro measures, for any cut set: efficiency on
// XCET-tagged electrons and acceptance of the off-coincidence (MIP/hadron)
// sample. The energy-scalable selection is reported separately from the
// energy-window selection.
//
// Usage: root -l -b -q 'macros/ElectronID.C+(15)'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include "radStyle.h"

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

void ElectronID(int run = 15)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(run <= 13 ? TString::Format("data/run_%d.root", run)
                                     : TString::Format("data/download/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("no file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NSLOT][NSAMP];
  t->SetBranchStatus("*", 0); t->SetBranchStatus("channel", 1);
  t->SetBranchAddress("channel", ch);
  const Long64_t nEnt = t->GetEntries();
  const double toADC = run <= 13 ? 1.0 : MV2ADC;

  FILE *sum = fopen(outDir + "/ElectronID_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };

  // variable histograms, tagged e (T) vs off-coincidence (O), module-hit only
  TH1F *hShT = new TH1F("hShT", ";max LG / #Sigma LG;fraction / bin", 50, 0, 1);
  TH1F *hShO = (TH1F*)hShT->Clone("hShO");
  TH1F *hRaT = new TH1F("hRaT", ";#Sigma HG / #Sigma LG;fraction / bin", 60, 0, 12);
  TH1F *hRaO = (TH1F*)hRaT->Clone("hRaO");
  TH1F *hSuT = new TH1F("hSuT", ";#Sigma LG [ADC-eq];fraction / bin", 90, 0, 4500);
  TH1F *hSuO = (TH1F*)hSuT->Clone("hSuO");

  long nTag = 0, nOff = 0;                       // module-hit populations
  long passT_E = 0, passO_E = 0;                 // energy-window selection
  long passT_S = 0, passO_S = 0;                 // energy-scalable selection
  // cuts (5 GeV values; sumLG window scales with beam energy)
  const double RA_LO = 2.0, RA_HI = 3.9, SUM_LO = 1800, SUM_HI = 4300;

  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    auto amp = [&](int s, int be, int pol){
      double b = 0; for (int k = 0; k < be; ++k) b += ch[s][k]; b /= be;
      float ex = ch[s][0];
      for (int k = 0; k < NSAMP; ++k){ float v = ch[s][k]; if (pol>0 ? v>ex : v<ex) ex = v; }
      return (pol>0 ? ex-b : b-ex) * toADC; };
    const bool tag = amp(0,BASE_CTR,-1) > 150 && amp(1,BASE_CTR,-1) > 150;
    double S = 0, mx = 0, H = 0;
    for (int j = 0; j < 4; ++j) {
      double l = amp(LGs[j], BASE_MOD, +1);
      S += l; if (l > mx) mx = l;
      H += amp(HGs[j], BASE_MOD, +1);
    }
    if (S < 300) continue;                        // module-hit events only
    double share = mx / S, ratio = H / S;
    (tag ? hShT : hShO)->Fill(share);
    (tag ? hRaT : hRaO)->Fill(ratio);
    (tag ? hSuT : hSuO)->Fill(S);
    (tag ? nTag : nOff)++;
    const bool selS = ratio > RA_LO && ratio < RA_HI;                   // energy-free
    const bool selE = selS && S > SUM_LO && S < SUM_HI;                 // + energy window
    if (selS) (tag ? passT_S : passO_S)++;
    if (selE) (tag ? passT_E : passO_E)++;
    if (i % 5000 == 0) printf("  %lld/%lld\n", i, nEnt);
  }

  out("ElectronID run %d (tag-and-probe vs XCET coincidence, module-hit events)\n", run);
  out("populations: tagged e %ld, off-coincidence %ld\n\n", nTag, nOff);
  out("energy-scalable selection (HG/LG in [%.1f, %.1f]):\n", RA_LO, RA_HI);
  out("  electron efficiency  %.1f%%\n", 100.0*passT_S/nTag);
  out("  MIP/hadron acceptance %.1f%%  (rejection x%.1f)\n",
      100.0*passO_S/nOff, double(nOff)/std::max(1L,passO_S));
  out("\n+ energy window (SumLG in [%.0f, %.0f], scales with beam E):\n", SUM_LO, SUM_HI);
  out("  electron efficiency  %.1f%%\n", 100.0*passT_E/nTag);
  out("  MIP/hadron acceptance %.1f%%  (rejection x%.1f)\n",
      100.0*passO_E/nOff, double(nOff)/std::max(1L,passO_E));

  // styled figure
  TCanvas c("c", "c", 1900, 560); c.Divide(3, 1, 0.004, 0.004);
  TH1F* pairs[3][2] = {{hShT,hShO},{hRaT,hRaO},{hSuT,hSuO}};
  const char *ttl[3] = {"shower sharing", "prompt / slow balance", "total LG energy"};
  for (int p = 0; p < 3; ++p) {
    c.cd(p + 1);
    TH1F *hT = pairs[p][0], *hO = pairs[p][1];
    hT->Scale(1.0/nTag); hO->Scale(1.0/nOff);
    hO->SetLineColor(rad::cBlue()); hO->SetFillColor(0); hO->SetLineWidth(3);
    hT->SetLineColor(rad::cRed()); hT->SetFillColor(rad::cBand()); hT->SetLineWidth(3);
    double mx2 = std::max(hT->GetMaximum(), hO->GetMaximum()) * 1.25;
    hO->SetMaximum(mx2); hO->Draw("hist"); hT->Draw("hist same");
    TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(21);
    l.DrawLatex(0.17, 0.86, ttl[p]);
    if (p == 0) {
      TLegend *lg = new TLegend(0.55, 0.68, 0.94, 0.84);
      lg->AddEntry(hT, "XCET-tagged e", "lf");
      lg->AddEntry(hO, "off-coincidence", "l");
      lg->Draw();
    }
  }
  c.SaveAs(outDir + "/ElectronID.png");
  fclose(sum);
  printf("Wrote %s/ElectronID.png\n", outDir.Data());
}
