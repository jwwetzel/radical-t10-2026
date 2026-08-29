// EnergyScan.C — first energy-scan trends from the XCET-tagged points.
// Datasets: run 2526 (1 GeV, runs 25+26 merged), run 2223 (3 GeV, runs
// 22+23 merged), run 15 (5 GeV anchor). All new-format (mV, slots).
//
// Per point: tagged-electron Sum-LG peak (iterative Gaussian core fit),
// sigma/E, and the srCFD 4-capillary mean shower time vs TR0 (per-dataset
// wall-aware transfer calibration, threshold 0.15 x HG_true, tebSigma-style
// robust width). Trends: response linearity, sigma/E vs E, sigma_t vs E.
//
// Usage: root -l -b -q 'macros/EnergyScan.C+'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TGraphErrors.h"
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
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};
static const double SRCFD_FRAC = 0.15, THR_MIN = 20.0 * MV2ADC;

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
static double leTime(const float *w, const float *tax, int pol, double baseMV, double thrADC)
{
  const double thr = pol > 0 ? baseMV + thrADC / MV2ADC : baseMV - thrADC / MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s)
    if (pol > 0 ? w[s] >= thr : w[s] <= thr) {
      double v0 = w[s-1], v1 = w[s];
      if (v1 == v0) return tax[s];
      return tax[s-1] + (thr - v0) / (v1 - v0) * (tax[s] - tax[s-1]);
    }
  return -1e9;
}
static double tebSigma(std::vector<double> &v, double *errOut = nullptr)
{
  if (v.size() < 50) return -1;
  double rc = 0, rw = 0;
  { double s=0,s2=0; for (double x : v){s+=x;s2+=x*x;}
    rc=s/v.size(); rw=std::sqrt(std::max(0.0,s2/v.size()-rc*rc)); }
  long nC = v.size();
  for (int it = 0; it < 5; ++it) {
    double s=0,s2=0; long n=0;
    for (double x : v) if (std::fabs(x-rc) < 2.5*rw){s+=x;s2+=x*x;++n;}
    if (n < 30) break;
    rc=s/n; rw=std::sqrt(std::max(0.0,s2/n-rc*rc)); nC=n;
  }
  if (!(rw > 1e-6) || !std::isfinite(rc) || !std::isfinite(rw)) return -1;
  const double robust = rw*1000.0/0.9546;
  TH1F h("teb","",120,rc-4*rw,rc+4*rw);
  for (double x : v) h.Fill(x);
  TF1 g("g","gaus",rc-2*rw,rc+2*rw);
  g.SetParameters(h.GetMaximum(),rc,rw);
  h.Fit(&g,"QRN");
  double gf = 1000.0*std::fabs(g.GetParameter(2)), ge = 1000.0*g.GetParError(2);
  bool useG = gf > 0.5*robust && gf < 2.0*robust;
  if (errOut) *errOut = useG ? ge : robust/std::sqrt(2.0*nC);
  return useG ? gf : robust;
}

void EnergyScan()
{
  SetRadStyle();
  gSystem->mkdir("Output/scan", true);
  FILE *sum = fopen("Output/scan/EnergyScan_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
    fflush(sum); fflush(stdout);
  };

  const int NP = 4;
  int runs[NP] = {2526, 24, 15, 27};
  double E[NP] = {1.0, 3.0, 5.0, 7.0};
  int cols[NP] = {rad::cTeal(), rad::cAmber(), rad::cRed(), rad::cBlue()};
  double pkE[NP], pkEe[NP], sgE[NP], sgEe[NP], tS[NP], tSe[NP];
  long nE[NP];
  TH1F *hS[NP];

  for (int ip = 0; ip < NP; ++ip) {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", runs[ip]));
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP], tax[2][NSAMP];
    t->SetBranchStatus("*", 0);
    t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
    t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tax);
    const Long64_t nEnt = t->GetEntries();

    // pass 1: wall + transfer per capillary (beam events)
    TH1F *hA[4]; TH2F *hHL[4];
    for (int j = 0; j < 4; ++j) {
      hA[j] = new TH1F(Form("hA%d_%d",ip,j),"",128,0,3200);
      hHL[j] = new TH2F(Form("hL%d_%d",ip,j),"",120,0,1200,128,0,3200);
    }
    std::vector<char> isB(nEnt);
    for (Long64_t i = 0; i < nEnt; ++i) {
      t->GetEntry(i);
      if (i % 2000 == 0) { printf("  [%d GeV] pass1 %lld/%lld\n", (int)E[ip], i, nEnt); fflush(stdout); }
      Pulse c0 = pulseOf(ch[0],-1,BASE_CTR), c1 = pulseOf(ch[1],-1,BASE_CTR);
      isB[i] = c0.amp > 150 && c1.amp > 150;
      if (!isB[i]) continue;
      for (int j = 0; j < 4; ++j) {
        Pulse l = pulseOf(ch[LGs[j]],+1,BASE_MOD), h = pulseOf(ch[HGs[j]],+1,BASE_MOD);
        hA[j]->Fill(h.amp); hHL[j]->Fill(l.amp, h.amp);
      }
    }
    double wall[4], a[4], b[4];
    for (int j = 0; j < 4; ++j) {
      double q = 0.995; hA[j]->GetQuantiles(1, &wall[j], &q);
      TProfile *pr = hHL[j]->ProfileX(Form("p%d_%d",ip,j));
      double lgMax = 1200, ceil_ = 0.72*wall[j];
      for (int bb = pr->FindBin(60); bb <= pr->GetNbinsX(); ++bb)
        if (pr->GetBinEntries(bb) > 3 && pr->GetBinContent(bb) > ceil_) { lgMax = pr->GetBinCenter(bb); break; }
      TF1 fl("fl","pol1",30,lgMax);
      pr->Fit(&fl,"QRN","",30,lgMax);
      a[j] = fl.GetParameter(0); b[j] = fl.GetParameter(1);
    }

    // pass 2: spectra + timing
    hS[ip] = new TH1F(Form("hS%d",ip), ";#Sigma LG [ADC-eq];fraction / bin", 150, 0, 6500);
    hS[ip]->SetDirectory(nullptr);          // survive f->Close(): the crash was a use-after-free here
    std::vector<double> dtA;
    long nOnMod = 0;
    const double SMIN = 60.0 * E[ip] + 120;    // miss/noise floor, scales mildly
    for (Long64_t i = 0; i < nEnt; ++i) {
      if (i % 2000 == 0) { printf("  [%d GeV] pass2 %lld/%lld\n", (int)E[ip], i, nEnt); fflush(stdout); }
      if (!isB[i]) continue;
      t->GetEntry(i);
      double S = 0;
      for (int j = 0; j < 4; ++j) S += pulseOf(ch[LGs[j]],+1,BASE_MOD).amp;
      if (S > SMIN) { hS[ip]->Fill(S); ++nOnMod; }
      // srCFD shower time
      Pulse m1 = pulseOf(ch[17],-1,BASE_MOD);
      if (m1.amp < 300) continue;
      double t1 = leTime(ch[17], tax[1], -1, m1.base, 0.20*m1.amp);
      if (t1 < -1e8) continue;
      double ts = 0; int nOK = 0;
      for (int j = 0; j < 4; ++j) {
        Pulse l = pulseOf(ch[LGs[j]],+1,BASE_MOD), h = pulseOf(ch[HGs[j]],+1,BASE_MOD);
        double thr = SRCFD_FRAC * (a[j] + b[j]*l.amp);
        if (thr < THR_MIN || thr > 0.9*wall[j] || h.amp < thr) continue;
        double tc = leTime(ch[HGs[j]], tax[1], +1, h.base, thr);
        if (tc > -1e8) { ts += tc; ++nOK; }
      }
      if (nOK >= 2) dtA.push_back(ts/nOK - t1);
    }
    nE[ip] = nOnMod;

    // peak fit: seed from maximum above the miss ridge
    if (hS[ip]->GetEntries() < 50) { pkE[ip]=0; pkEe[ip]=0; sgE[ip]=0; sgEe[ip]=0;
      out("%.0f GeV: too few electrons for peak fit\n", E[ip]); tS[ip]=-1; tSe[ip]=0; f->Close(); continue; }
    int pb = hS[ip]->FindBin(SMIN + 60); double pv = 0; int pbb = pb;
    for (int bb = pb; bb <= hS[ip]->GetNbinsX(); ++bb)
      if (hS[ip]->GetBinContent(bb) > pv) { pv = hS[ip]->GetBinContent(bb); pbb = bb; }
    double m = hS[ip]->GetBinCenter(pbb), s = 0.35*m;
    TF1 *g = new TF1(Form("g%d",ip), "gaus", m-2*s, m+2*s);
    g->SetParameters(pv, m, s);
    g->SetParLimits(1, SMIN + 30, 4400);          // mean stays on the physical peak
    g->SetParLimits(2, 25, 0.8*m);                // width bounded, cannot swallow the ridge
    for (int it = 0; it < 3; ++it) {
      hS[ip]->Fit(g, "QNR", "", std::max(SMIN, m-1.7*s), m+1.7*s);
      m = g->GetParameter(1); s = std::fabs(g->GetParameter(2));
    }
    hS[ip]->Fit(g, "QR", "", std::max(SMIN, m-1.7*s), m+1.7*s);
    pkE[ip] = g->GetParameter(1); pkEe[ip] = g->GetParError(1);
    sgE[ip] = std::fabs(g->GetParameter(2)); sgEe[ip] = g->GetParError(2);
    double e;
    tS[ip] = tebSigma(dtA, &e); tSe[ip] = e;
    out("%.0f GeV (run %d): N(e,on-module) %ld | peak %.0f +/- %.0f, sigma %.0f => sigma/E %.1f +/- %.1f %% | shower-time sigma %.0f +/- %.0f ps (N=%zu)\n",
        E[ip], runs[ip], nOnMod, pkE[ip], pkEe[ip], sgE[ip],
        100*sgE[ip]/pkE[ip],
        100*sgE[ip]/pkE[ip]*std::sqrt(std::pow(sgEe[ip]/sgE[ip],2)+std::pow(pkEe[ip]/pkE[ip],2)),
        tS[ip], tSe[ip], dtA.size());
    f->Close();
  }

  // ---- trend canvas ----
  TCanvas c("c","c",2000,1050); c.Divide(2,2,0.004,0.006);
  // (1) spectra overlaid
  c.cd(1);
  double ymax = 0;
  for (int ip = 0; ip < NP; ++ip) { hS[ip]->Scale(1.0/std::max(1L,nE[ip])); ymax = std::max(ymax, hS[ip]->GetMaximum()); }
  for (int ip = NP-1; ip >= 0; --ip) {
    hS[ip]->SetLineColor(cols[ip]); hS[ip]->SetLineWidth(3);
    hS[ip]->SetMaximum(1.2*ymax);
    hS[ip]->Draw(ip == NP-1 ? "hist" : "hist same");
  }
  TLegend *lg = new TLegend(0.62,0.62,0.94,0.87);
  for (int ip = 0; ip < NP; ++ip) lg->AddEntry(hS[ip], Form("%.0f GeV (%ld e)", E[ip], nE[ip]), "l");
  lg->Draw();
  TLatex hx; hx.SetNDC(); hx.SetTextFont(43); hx.SetTextSize(22);
  hx.DrawLatex(0.16,0.86,"tagged electrons, #Sigma LG");
  // (2) response linearity
  c.cd(2);
  TGraphErrors *gr = new TGraphErrors(NP);
  for (int ip = 0; ip < NP; ++ip) { gr->SetPoint(ip, E[ip], pkE[ip]); gr->SetPointError(ip, 0, pkEe[ip]); }
  gr->SetTitle(";beam energy [GeV];#Sigma LG peak [ADC-eq]");
  gr->SetMarkerStyle(20); gr->SetMarkerSize(1.4); gr->SetMarkerColor(rad::cInk()); gr->SetLineWidth(2);
  gr->GetXaxis()->SetLimits(0, 8); gr->SetMinimum(0); gr->SetMaximum(5200);
  gr->Draw("AP");
  TF1 *lin = new TF1("lin","[0]*x",0,8);
  lin->SetParameter(0, pkE[2]/E[2]); lin->SetLineColor(rad::cGrey()); lin->SetLineStyle(7); lin->Draw("same");
  hx.DrawLatex(0.16,0.86,"response  #font[42]{(dashed: linear through 5 GeV)}");
  // (3) sigma/E vs E
  c.cd(3);
  TGraphErrors *gs = new TGraphErrors(NP);
  for (int ip = 0; ip < NP; ++ip) {
    double r = 100*sgE[ip]/pkE[ip];
    gs->SetPoint(ip, E[ip], r);
    gs->SetPointError(ip, 0, r*std::sqrt(std::pow(sgEe[ip]/sgE[ip],2)+std::pow(pkEe[ip]/pkE[ip],2)));
  }
  gs->SetTitle(";beam energy [GeV];#sigma/E [%]");
  gs->SetMarkerStyle(20); gs->SetMarkerSize(1.4); gs->SetMarkerColor(rad::cInk()); gs->SetLineWidth(2);
  gs->GetXaxis()->SetLimits(0, 8); gs->SetMinimum(0);
  gs->Draw("AP");
  hx.DrawLatex(0.16,0.86,"width  #font[42]{(position-smearing dominated)}");
  // (4) timing vs E
  c.cd(4);
  TGraphErrors *gt = new TGraphErrors(NP);
  for (int ip = 0; ip < NP; ++ip) { gt->SetPoint(ip, E[ip], tS[ip]); gt->SetPointError(ip, 0, tSe[ip]); }
  gt->SetTitle(";beam energy [GeV];shower-time #sigma [ps]");
  gt->SetMarkerStyle(20); gt->SetMarkerSize(1.4); gt->SetMarkerColor(rad::cInk()); gt->SetLineWidth(2);
  gt->GetXaxis()->SetLimits(0, 8); gt->SetMinimum(0);
  gt->Draw("AP");
  // closed-form a/sqrt(E) (+) b from the 1 and 5 GeV points (no Minuit: it
  // segfaulted here inside a drawn-graph fit; two points, two parameters)
  double a2 = (tS[0]*tS[0] - tS[2]*tS[2]) * E[0]*E[2] / (E[2]-E[0]);
  double b2 = tS[2]*tS[2] - a2/E[2];
  TF1 *ft = new TF1("ft","sqrt([0]*[0]/x+[1]*[1])",0.5,6);
  ft->SetParameters(std::sqrt(std::max(a2,0.0)), std::sqrt(std::max(b2,0.0)));
  ft->SetLineColor(rad::cTeal()); ft->SetLineWidth(3); ft->Draw("same");
  out("\ntiming trend (1&5 GeV solve): sigma_t = %.0f ps/sqrt(E) (+) %.0f ps const (incl. MCP+DRS ref); 3 GeV point: %.0f ps predicted %.0f\n",
      std::sqrt(std::max(a2,0.0)), std::sqrt(std::max(b2,0.0)), tS[1],
      std::sqrt(a2/E[1] + std::max(b2,0.0)));
  hx.DrawLatex(0.16,0.86,"srCFD 4-cap shower time  #font[42]{(incl. MCP+DRS)}");
  c.SaveAs("Output/scan/EnergyScan.png");
  fclose(sum);
  printf("Wrote Output/scan/EnergyScan.png, EnergyScan_summary.txt\n");
  fflush(nullptr);
  gSystem->Exit(0);   // skip ROOT teardown: cleanup after a batch macro has
                      // twice livelocked this session (TCanvas cleanup spiral)
}
