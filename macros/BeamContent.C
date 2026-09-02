// BeamContent.C — measured electron content of the T10 beam vs momentum.
//
// For a beam physicist: the two XCETs, absolutely calibrated (XCETCalib.C),
// let us turn a raw coincidence-tag rate into an ABSOLUTE electron fraction.
//   raw tag rate   R(run) = N(both XCET > cthr) / N(triggers)
//   folded eff     eps(P) = product of the two threshold-folded single-counter
//                           efficiencies at that run's pressures (from the
//                           calibrated slopes + 1-pe rulers + SPE widths)
//   electron frac  f_e     = R / eps        (efficiency-corrected)
// Grouped by |p| and polarity, f_e(p) is a direct cross-check of the T10
// secondary-beam composition model. Momentum/polarity from runs/runs.json;
// pressures from the manifest-authoritative registry (same as XCETCalib.C).
//
// Two honest caveats carried through:
//  - eps uses the campaign's actual software tag (>40 ADC-eq global window),
//    so f_e is the electron fraction AMONG TRIGGERS (MCP-triggered sample),
//    not the raw spill composition. Stated as such.
//  - low-pressure eps is depressed by fake-tag dilution (see tagprobe.html);
//    f_e there is an UPPER bound on electrons (eps too low -> f_e too high).
//    Flagged per point.
//
// Usage: root -l -b -q 'macros/BeamContent.C+'
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TMath.h"
#include "TSystem.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095, THR = 40.0;
// calibrated rulers + SPE widths (XCETCalib.C medians)
static const double A1[2] = {40.2, 21.8}, SIG1[2] = {14.5, 7.9};
static const double SLOPE[2] = {19.0, 26.0};   // pe/bar

static double ampOf(const float *w)
{
  double b = 0; for (int k = 0; k < 200; ++k) b += w[k]; b /= 200;
  float mn = w[0]; for (int k = 1; k < NSAMP; ++k) if (w[k] < mn) mn = w[k];
  return (b - mn) * MV2ADC;
}

// threshold-folded single-counter tag efficiency at pressure P
static double effFold(int c, double P)
{
  double lam = SLOPE[c]*P, e = 0;
  for (int n = 1; n <= 120; ++n)
    e += TMath::PoissonI(n, lam) * 0.5*TMath::Erfc((THR - n*A1[c])/(std::sqrt(2.0*n)*SIG1[c]));
  return e;
}

void BeamContent()
{
  SetRadStyle();
  gSystem->mkdir("Output/summary", true);
  FILE *sum = fopen("Output/summary/BeamContent_summary.txt", "w");
  auto out = [&](const char *f, ...) { va_list a; va_start(a,f); vfprintf(stdout,f,a); va_end(a);
    va_start(a,f); vfprintf(sum,f,a); va_end(a); fflush(sum); };

  // run, signed momentum [GeV/c], P40, P43, cthr  (momentum from runs.json; pressures manifest-authoritative)
  struct RunDef { int run; double p; double p40, p43, cthr; };
  RunDef R[] = {
    {15, 5,0.400,0.400,100},{24, 3,1.300,1.300,100},{2526,1,1.300,1.300,100},{27,-7,0.210,0.210,50},
    {9001,-9,0.150,0.156,40},{30,-11,0.062,0.060,40},{31,-11,0.062,0.060,40},{32,-9,0.149,0.146,40},
    {33,-7,0.206,0.194,40},{35, 3,1.313,1.303,100},{37, 5,0.400,0.405,100},
    {38, 1,0.590,0.553,100},{39, 1,0.593,0.593,100},{40, 3,0.593,0.593,100},{41, 5,0.452,0.439,100},
    {42,-7,0.225,0.220,40},{43,-9,0.152,0.156,40},{44,-11,0.090,0.088,40} };   // run 34 excluded (pion tag)
  const int NR = sizeof(R)/sizeof(R[0]);
  const Long64_t MAXEV = 20000;

  out("# MEASURED ELECTRON CONTENT OF THE T10 BEAM (BeamContent.C)\n");
  out("# f_e = (coincidence tag rate) / (threshold-folded coincidence efficiency)\n");
  out("# electron fraction AMONG MCP TRIGGERS; low-P points are UPPER bounds (fake-tag dilution)\n\n");
  out("# run  p[GeV/c]  P40/P43     Ntrig  Ncoinc   rawRate    eps_fold   f_e +/- stat   flag\n");

  // per-run results, and per-(momentum,polarity) aggregation
  struct Pt { double p; double fe, fee; int run; bool upper; };
  std::vector<Pt> pts;

  for (int ir = 0; ir < NR; ++ir) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", R[ir].run));
    if (!f || f->IsZombie()) { out("# run %d missing\n", R[ir].run); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP];
    t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1);
    t->SetBranchAddress("channel", ch);
    Long64_t nEnt = std::min(t->GetEntries(), MAXEV);
    long nCo = 0;
    for (Long64_t i = 0; i < nEnt; ++i) {
      t->GetEntry(i);
      double a40 = ampOf(ch[0]), a43 = ampOf(ch[1]);
      if (a40 > THR && a43 > THR) ++nCo;
    }
    f->Close();
    double raw = (double)nCo/nEnt;
    double eps = effFold(0, R[ir].p40) * effFold(1, R[ir].p43);
    double fe = eps > 0 ? raw/eps : -1;
    double rawE = std::sqrt(std::max(raw*(1-raw), 1.0/nEnt)/nEnt);
    double feE = eps > 0 ? rawE/eps : 0;
    bool upper = (std::min(R[ir].p40, R[ir].p43) < 0.16);   // fake-dilution regime
    const char *flag = upper ? "UPPER (dilution)" : "";
    out("%5d   %+3.0f    %.3f/%.3f  %6lld  %6ld   %.4f     %.4f     %.3f +/- %.3f  %s\n",
        R[ir].run, R[ir].p, R[ir].p40, R[ir].p43, (long long)nEnt, nCo, raw, eps, fe, feE, flag);
    if (fe > 0 && fe <= 1.5) pts.push_back({R[ir].p, std::min(1.0,fe), feE, R[ir].run, upper});
  }

  // aggregate by signed momentum
  out("\n# electron fraction vs momentum (weighted mean of runs at each setting; * = contains upper-bound points)\n");
  out("# p[GeV/c]   f_e     +/-     nRuns   note\n");
  std::vector<double> uP;
  for (auto &q : pts) if (std::find(uP.begin(),uP.end(),q.p)==uP.end()) uP.push_back(q.p);
  std::sort(uP.begin(), uP.end());
  struct Agg { double p, fe, fee; bool anyUpper; };
  std::vector<Agg> aggs;
  for (double pp : uP) {
    double sw=0, swx=0; int n=0; bool au=false;
    for (auto &q : pts) if (q.p==pp) { double w=1.0/(q.fee*q.fee+1e-6); sw+=w; swx+=w*q.fe; ++n; au=au||q.upper; }
    if (sw>0) { double m=swx/sw, e=1.0/std::sqrt(sw);
      aggs.push_back({pp,m,e,au});
      out("%+5.0f     %.3f   %.3f     %d      %s\n", pp, m, e, n, au?"* upper bound (electrons <= this)":""); }
  }

  // ---- canvas: f_e vs |p|, split by polarity ----
  TCanvas c("c","c",1500,900);
  gPad->SetTopMargin(0.13);
  TH1F *fr = gPad->DrawFrame(0, 0, 12.5, 1.02, ";beam momentum |p| [GeV/c];electron fraction among triggers  f_{e}");
  TGraphErrors *gPos = new TGraphErrors(), *gNeg = new TGraphErrors();
  TGraphErrors *gPosU = new TGraphErrors(), *gNegU = new TGraphErrors();
  for (auto &a : aggs) {
    bool neg = a.p < 0; double ap = std::fabs(a.p);
    TGraphErrors *g = a.anyUpper ? (neg?gNegU:gPosU) : (neg?gNeg:gPos);
    int k = g->GetN(); g->SetPoint(k, ap + (neg?0.06:-0.06), a.fe); g->SetPointError(k, 0, a.fee);
  }
  auto sty = [](TGraphErrors *g, int col, int mk){ g->SetMarkerStyle(mk); g->SetMarkerSize(1.7);
    g->SetMarkerColor(col); g->SetLineColor(col); g->SetLineWidth(2); };
  sty(gPos, rad::cTeal(), 20); sty(gPosU, rad::cTeal(), 24);
  sty(gNeg, rad::cBlue(), 21); sty(gNegU, rad::cBlue(), 25);
  gPos->Draw("P same"); gPosU->Draw("P same"); gNeg->Draw("P same"); gNegU->Draw("P same");
  TLegend *lg = new TLegend(0.50,0.66,0.90,0.90);
  lg->SetBorderSize(0); lg->SetFillStyle(0); lg->SetTextFont(43); lg->SetTextSize(22);
  lg->AddEntry(gPos, "positive beam (e^{+}), measured", "p");
  lg->AddEntry(gNeg, "negative beam (e^{-}), measured", "p");
  lg->AddEntry(gPosU, "open: upper bound (low-P dilution)", "p");
  lg->Draw();
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43); tx.SetTextSize(27);
  tx.DrawLatex(0.10,0.95,"Electron content of the T10 secondary beam, from XCET coincidence counting");
  tx.SetTextSize(18); tx.SetTextColor(kGray+2);
  tx.DrawLatex(0.10,0.905,"efficiency-corrected via the absolutely-calibrated XCETs; fraction among MCP triggers");
  c.SaveAs("Output/summary/BeamContent.png");
  fclose(sum);
  printf("Wrote Output/summary/BeamContent.png + summary\n");
  gSystem->Exit(0);
}
