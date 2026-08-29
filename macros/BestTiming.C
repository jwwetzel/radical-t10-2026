// BestTiming.C — saturation-recovered CFD ("srCFD"), following
// github.com/jwwetzel/radical (reduce/Reducer.C + lib/physics/RadTiming.h):
//
//   1. HG_true = a + b * LG_peak, calibrated in the LINEAR region only
//      (wall-aware, as in TransferFit.C / calibHGLG.C).
//   2. Threshold = 0.15 * HG_true; guards: thr above noise (> 20 mV eq)
//      and safely below the clip shelf (< 0.9 * wall). The crossing is a
//      leading-edge threshold with linear interpolation — a clipped top
//      never matters because the steep edge below the shelf is timed.
//   3. Reference: TR0 (MCP) group-1 copy, CFD at 20% (WaveformUtils default).
//   4. Width estimator: tebSigma — 5 iterations of 2.5-sigma truncation,
//      debias /0.9546, Gaussian-core cross-check, robust fallback.
//
// Repo's (DW-UP)/2 reference-free method needs two-ended readout; the T10
// module is single-ended, so all times are MCP-referenced here.
//
// Usage: root -l -b -q 'macros/BestTiming.C+(15)'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TProfile.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TSystem.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <algorithm>
#include "radStyle.h"

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const double SRCFD_FRAC = 0.15;                 // repo: lgcfd_frac
static const double THR_MIN = 20.0 * MV2ADC;           // repo: > 20 mV
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};

struct Pulse { double base, amp; int pkS; };
static Pulse pulseOf(const float *w, int pol, int baseEnd)
{
  Pulse r; r.base = 0;
  for (int s = 0; s < baseEnd; ++s) r.base += w[s];
  r.base /= baseEnd;
  float pkV = w[0]; r.pkS = 0;
  for (int s = 0; s < NSAMP; ++s) { float v = w[s];
    if (pol > 0 ? v > pkV : v < pkV) { pkV = v; r.pkS = s; } }
  r.amp = (pol > 0 ? pkV - r.base : r.base - pkV) * MV2ADC;
  return r;
}
// leading-edge crossing of an absolute threshold (ADC-eq above baseline)
static double leTime(const float *w, const float *tax, int pol, double baseMV, double thrADC)
{
  const double thr = pol > 0 ? baseMV + thrADC / MV2ADC : baseMV - thrADC / MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s) {
    if (pol > 0 ? w[s] >= thr : w[s] <= thr) {
      double v0 = w[s-1], v1 = w[s];
      if (v1 == v0) return tax[s];
      return tax[s-1] + (thr - v0) / (v1 - v0) * (tax[s] - tax[s-1]);
    }
  }
  return -1e9;
}

// tebSigma (RadTiming.h): robust truncated width + Gaussian-core cross-check.
// Returns sigma in ps (input in ns); -1 if < 50 events.
static double tebSigma(std::vector<double> &v, double *errOut = nullptr)
{
  if (v.size() < 50) return -1;
  double rc = 0, rw = 0;
  { double s = 0, s2 = 0; for (double x : v) { s += x; s2 += x*x; }
    rc = s / v.size(); rw = std::sqrt(std::max(0.0, s2/v.size() - rc*rc)); }
  long nCore = v.size();
  for (int it = 0; it < 5; ++it) {
    double s = 0, s2 = 0; long n = 0;
    for (double x : v) if (std::fabs(x - rc) < 2.5 * rw) { s += x; s2 += x*x; ++n; }
    if (n < 30) break;
    rc = s / n; rw = std::sqrt(std::max(0.0, s2/n - rc*rc)); nCore = n;
  }
  const double robust = rw * 1000.0 / 0.9546;          // ps, truncation-debiased
  // Gaussian-core cross-check
  TH1F h("teb", "", 120, rc - 4*rw, rc + 4*rw);
  for (double x : v) h.Fill(x);
  TF1 g("gteb", "gaus", rc - 2*rw, rc + 2*rw);
  g.SetParameters(h.GetMaximum(), rc, rw);
  h.Fit(&g, "QRN");
  double gfit = 1000.0 * std::fabs(g.GetParameter(2));
  double gerr = 1000.0 * g.GetParError(2);
  bool useG = (gfit > 0.5 * robust && gfit < 2.0 * robust);
  if (errOut) *errOut = useG ? gerr : robust / std::sqrt(2.0 * nCore);
  return useG ? gfit : robust;
}

void BestTiming(int run = 15)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(TString::Format("data/download/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("no file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NSLOT][NSAMP], tax[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tax);
  const Long64_t nEnt = t->GetEntries();

  FILE *sum = fopen(outDir + "/BestTiming_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };

  // ---- pass 1: wall + wall-aware transfer calibration (as TransferFit.C) ----
  TH1F *hA[4]; TH2F *hHL[4];
  for (int j = 0; j < 4; ++j) {
    hA[j] = new TH1F(Form("hA%d",j), "", 128, 0, 3200);
    hHL[j] = new TH2F(Form("hL%d",j), "", 120, 0, 1200, 128, 0, 3200);
  }
  std::vector<char> isBeam(nEnt);
  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    Pulse c0 = pulseOf(ch[0], -1, BASE_CTR), c1 = pulseOf(ch[1], -1, BASE_CTR);
    isBeam[i] = (c0.amp > 150 && c1.amp > 150);
    if (!isBeam[i]) continue;
    for (int j = 0; j < 4; ++j) {
      Pulse l = pulseOf(ch[LGs[j]], +1, BASE_MOD), h = pulseOf(ch[HGs[j]], +1, BASE_MOD);
      hA[j]->Fill(h.amp); hHL[j]->Fill(l.amp, h.amp);
    }
    if (i % 5000 == 0) printf("  pass1 %lld/%lld\n", i, nEnt);
  }
  double wall[4], a[4], b[4];
  out("srCFD (github.com/jwwetzel/radical): frac %.2f x LG-predicted HG_true\n", SRCFD_FRAC);
  for (int j = 0; j < 4; ++j) {
    double q = 0.995; hA[j]->GetQuantiles(1, &wall[j], &q);
    TProfile *pr = hHL[j]->ProfileX(Form("bp%d", j));
    double lgMax = 1200, ceil_ = 0.72 * wall[j];
    for (int bb = pr->FindBin(60); bb <= pr->GetNbinsX(); ++bb)
      if (pr->GetBinEntries(bb) > 3 && pr->GetBinContent(bb) > ceil_) { lgMax = pr->GetBinCenter(bb); break; }
    TF1 fl(Form("bf%d", j), "pol1", 30, lgMax);
    pr->Fit(&fl, "QRN", "", 30, lgMax);
    a[j] = fl.GetParameter(0); b[j] = fl.GetParameter(1);
    out("cap %d: wall %.0f, HG_true = %.0f + %.2f*LG (fit LG 30-%.0f)\n", LGs[j], wall[j], a[j], b[j], lgMax);
  }

  // ---- pass 2: srCFD times ----
  std::vector<double> dt[4], dtAvg;
  long nBeam = 0, nThrHi = 0;
  for (Long64_t i = 0; i < nEnt; ++i) {
    if (!isBeam[i]) continue;
    t->GetEntry(i);
    ++nBeam;
    Pulse m1 = pulseOf(ch[17], -1, BASE_MOD);
    if (m1.amp < 300) continue;
    double t1 = leTime(ch[17], tax[1], -1, m1.base, 0.20 * m1.amp);   // MCP CFD 20%
    if (t1 < -1e8) continue;
    double tsum = 0; int nOK = 0;
    for (int j = 0; j < 4; ++j) {
      Pulse l = pulseOf(ch[LGs[j]], +1, BASE_MOD), h = pulseOf(ch[HGs[j]], +1, BASE_MOD);
      double HGtrue = a[j] + b[j] * l.amp;
      double thr = SRCFD_FRAC * HGtrue;
      if (thr < THR_MIN) continue;                       // repo guard: above noise
      if (thr > 0.9 * wall[j]) { ++nThrHi; continue; }   // repo guard: below shelf
      if (h.amp < thr) continue;                         // edge must reach threshold
      double tc = leTime(ch[HGs[j]], tax[1], +1, h.base, thr);
      if (tc < -1e8) continue;
      dt[j].push_back(tc - t1);
      tsum += tc; ++nOK;
    }
    if (nOK == 4) dtAvg.push_back(tsum / 4.0 - t1);
    if (i % 5000 == 0) printf("  pass2 %lld/%lld\n", i, nEnt);
  }

  out("\nbeam %ld; events with threshold above shelf guard: %ld\n\n", nBeam, nThrHi);
  out("=== srCFD timing vs TR0 (tebSigma) ===\n");
  double e;
  for (int j = 0; j < 4; ++j) {
    double s = tebSigma(dt[j], &e);
    out("cap %d: sigma = %6.1f +/- %4.1f ps (N=%zu)\n", LGs[j], s, e, dt[j].size());
  }
  double sAvg = tebSigma(dtAvg, &e);
  out("4-capillary mean shower time: sigma = %.1f +/- %.1f ps (N=%zu)  [incl. MCP + DRS ref]\n",
      sAvg, e, dtAvg.size());

  // ---- styled figure: per-cap + shower-time distributions ----
  TCanvas c("c", "c", 1500, 620); c.Divide(2, 1, 0.004, 0.004);
  c.cd(1);
  // center distributions for display
  TH1F *hd[4]; int cols[4] = {rad::cTeal(), rad::cBlue(), rad::cAmber(), rad::cRed()};
  double med0 = 0;
  { std::vector<double> v = dt[0]; std::nth_element(v.begin(), v.begin()+v.size()/2, v.end()); med0 = v[v.size()/2]; }
  TLegend *lg = new TLegend(0.66, 0.60, 0.94, 0.88);
  for (int j = 0; j < 4; ++j) {
    hd[j] = new TH1F(Form("hd%d", j), ";#Deltat #minus median [ns];events / 40 ps", 100, -2, 2);
    std::vector<double> v = dt[j];
    std::nth_element(v.begin(), v.begin()+v.size()/2, v.end());
    double med = v[v.size()/2];
    for (double x : dt[j]) hd[j]->Fill(x - med);
    hd[j]->SetLineColor(cols[j]); hd[j]->SetLineWidth(3);
    hd[j]->Draw(j ? "hist same" : "hist");
    lg->AddEntry(hd[j], Form("cap %d  (%.0f ps)", LGs[j], tebSigma(dt[j])), "l");
  }
  lg->Draw();
  TLatex hx; hx.SetNDC(); hx.SetTextFont(43); hx.SetTextSize(21);
  hx.DrawLatex(0.16, 0.86, "srCFD per capillary vs TR0");
  c.cd(2);
  TH1F *ha = new TH1F("ha", ";#Deltat #minus median [ns];events / 40 ps", 100, -2, 2);
  { std::vector<double> v = dtAvg; std::nth_element(v.begin(), v.begin()+v.size()/2, v.end());
    double med = v[v.size()/2]; for (double x : dtAvg) ha->Fill(x - med); }
  ha->SetLineColor(rad::cInk()); ha->SetFillColor(rad::cFill()); ha->Draw("hist");
  TF1 *gA = new TF1("gA", "gaus", -2, 2);
  gA->SetParameters(ha->GetMaximum(), 0, sAvg/1000.0);
  ha->Fit(gA, "QR", "", -2.5*sAvg/1000, 2.5*sAvg/1000);
  gA->SetLineColor(rad::cRed()); gA->Draw("same");
  hx.DrawLatex(0.16, 0.86, "4-capillary mean shower time");
  hx.SetTextSize(18); hx.SetTextColor(rad::cRed());
  hx.DrawLatex(0.16, 0.79, Form("#sigma = %.0f #pm %.0f ps  (incl. MCP+DRS)", sAvg, e));
  c.SaveAs(outDir + "/BestTiming.png");
  fclose(sum);
  printf("Wrote %s/BestTiming.png\n", outDir.Data());
}
