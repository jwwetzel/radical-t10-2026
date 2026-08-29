// ResolutionV2.C — timing & energy analysis for the run-14+ format (mV, slots).
//
// Slots: 0/1 XCET, 2 scint, 4-7 LG caps (TL,TR,BL,BR), 8 TR0 grp0,
// 13-16 HG partners of caps 5,4,7,6 (i.e. LG4-s14, LG5-s13, LG6-s16, LG7-s15),
// 17 TR0 grp1. times[g][1024] is the per-event DRS time axis, g = slot/9.
//
// Outputs: HG-vs-LG transfer fits (clip recovery), TR0 copy-vs-copy jitter,
// per-capillary CFD timing vs the group-1 TR0, MCP-free capillary pair,
// amplitude-binned timing, and the electron Sum-LG peak fit.
//
// Usage: root -l -b -q 'macros/ResolutionV2.C+(15)'

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TSystem.h"
#include "TMath.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include "radStyle.h"

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const float CHER_THR = 150.0f;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const double CFD_FRAC = 0.30;
static const float HG_LIN_MAX = 2500.0f, LG_MIN_FIT = 30.0f, LG_MAX_FIT = 400.0f;

static const int LGs[4] = {4, 5, 6, 7};       // TL TR BL BR
static const int HGs[4] = {14, 13, 16, 15};   // partners, same order

struct ChInfo { double base, amp; int pkS; };

static ChInfo chAmp(const float *w, int pol, int baseEnd)
{
  ChInfo r; r.base = 0;
  for (int s = 0; s < baseEnd; ++s) r.base += w[s];
  r.base /= baseEnd;
  float pkV = w[0]; r.pkS = 0;
  for (int s = 0; s < NSAMP; ++s) {
    float v = w[s];
    if (pol > 0 ? (v > pkV) : (v < pkV)) { pkV = v; r.pkS = s; }
  }
  r.amp = (pol > 0 ? pkV - r.base : r.base - pkV) * MV2ADC;
  return r;
}

static double cfdTime(const float *w, const float *tax, int pol, double baseMV,
                      double ampADC, int pkS, double frac)
{
  const double thr = pol > 0 ? baseMV + frac * ampADC / MV2ADC
                             : baseMV - frac * ampADC / MV2ADC;
  int s = pkS;
  while (s > 0 && (pol > 0 ? w[s - 1] >= thr : w[s - 1] <= thr)) --s;
  if (s == 0) return -1;
  const double v0 = w[s - 1], v1 = w[s];
  if (v1 == v0) return tax[s];
  return tax[s - 1] + (thr - v0) / (v1 - v0) * (tax[s] - tax[s - 1]);
}

static TF1 *coreFit(TH1F *h, const char *name)
{
  const int pb = h->GetMaximumBin();
  double m = h->GetBinCenter(pb), pk = h->GetBinContent(pb);
  int lo = pb, hi = pb;
  while (lo > 1 && h->GetBinContent(lo) > pk / 2) --lo;
  while (hi < h->GetNbinsX() && h->GetBinContent(hi) > pk / 2) ++hi;
  double s = std::max((h->GetBinCenter(hi) - h->GetBinCenter(lo)) / 2.355, h->GetBinWidth(1));
  TF1 *g = new TF1(name, "gaus", m - 2 * s, m + 2 * s);
  g->SetParameters(pk, m, s);
  for (int it = 0; it < 3; ++it) {
    h->Fit(g, "QNR", "", m - 2 * s, m + 2 * s);
    m = g->GetParameter(1); s = std::fabs(g->GetParameter(2));
  }
  h->Fit(g, "QR", "", m - 2 * s, m + 2 * s);
  return g;
}

void ResolutionV2(int run = 15)
{
  SetRadStyle();
  TString outDir = TString::Format("Output/run_%d", run);
  gSystem->mkdir(outDir, true);
  TFile *fin = TFile::Open(TString::Format("data/download/run_%d.root", run));
  if (!fin || fin->IsZombie()) { printf("no data file\n"); return; }
  TTree *t = (TTree*)fin->Get("pulse");
  static float ch[NSLOT][NSAMP], tax[2][NSAMP];
  t->SetBranchStatus("*", 0);
  t->SetBranchStatus("channel", 1); t->SetBranchStatus("times", 1);
  t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tax);
  const Long64_t nEnt = t->GetEntries();

  FILE *sum = fopen(outDir + "/ResolutionV2_summary.txt", "w");
  auto out = [&](const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, sum); fputs(b, stdout);
  };
  out("ResolutionV2 run %d, %lld events, CFD %.2f\n\n", run, nEnt, CFD_FRAC);

  // rail levels (subsampled)
  double gmax[NSLOT]; for (int s = 0; s < NSLOT; ++s) gmax[s] = -1e9;
  for (Long64_t i = 0; i < nEnt; i += 7) { t->GetEntry(i);
    for (int j = 0; j < 4; ++j) for (int k = 0; k < NSAMP; ++k)
      if (ch[HGs[j]][k] > gmax[HGs[j]]) gmax[HGs[j]] = ch[HGs[j]][k]; }

  // ---------------- pass 1: transfer fits ----------------
  TH2F *hHL[4];
  for (int j = 0; j < 4; ++j)
    hHL[j] = new TH2F(Form("hHL_%d", j),
      Form("HG s%d vs LG %d (beam);LG peak [ADC-eq];HG peak [ADC-eq]", HGs[j], LGs[j]),
      120, 0, 1200, 120, 0, 4000);
  std::vector<char> isBeam(nEnt);
  std::vector<float> lgA(4*nEnt), hgA(4*nEnt);
  std::vector<char> hgClip(4*nEnt);
  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i);
    ChInfo c0 = chAmp(ch[0], -1, BASE_CTR), c1 = chAmp(ch[1], -1, BASE_CTR);
    isBeam[i] = (c0.amp > CHER_THR && c1.amp > CHER_THR);
    for (int j = 0; j < 4; ++j) {
      ChInfo l = chAmp(ch[LGs[j]], +1, BASE_MOD), h = chAmp(ch[HGs[j]], +1, BASE_MOD);
      lgA[4*i+j] = l.amp; hgA[4*i+j] = h.amp;
      bool clip = false;
      for (int k = 0; k < NSAMP; ++k) if (ch[HGs[j]][k] > gmax[HGs[j]] - 1.0) { clip = true; break; }
      hgClip[4*i+j] = clip;
      if (isBeam[i] && !clip && h.amp < HG_LIN_MAX) hHL[j]->Fill(l.amp, h.amp);
    }
    if (i % 5000 == 0) printf("  pass1 %lld/%lld\n", i, nEnt);
  }
  double p0[4], p1[4];
  out("HG vs LG transfer (fit LG %.0f-%.0f):\n", LG_MIN_FIT, LG_MAX_FIT);
  TCanvas cHL("cHL","cHL",1600,1200); cHL.Divide(2,2);
  for (int j = 0; j < 4; ++j) {
    cHL.cd(j+1); gPad->SetLogz();
    hHL[j]->Draw("colz");
    TProfile *pr = hHL[j]->ProfileX(Form("pr_%d", j));
    TF1 *fl = new TF1(Form("fl%d", j), "pol1", LG_MIN_FIT, LG_MAX_FIT);
    pr->Fit(fl, "QR", "", LG_MIN_FIT, LG_MAX_FIT);
    fl->SetRange(LG_MIN_FIT, 1200);
    pr->SetLineColor(kBlack); pr->Draw("same"); fl->SetLineColor(kRed); fl->Draw("same");
    p0[j] = fl->GetParameter(0); p1[j] = fl->GetParameter(1);
    out("  cap %d: HG = %7.2f + %6.3f * LG   (ratio %.2f)\n", LGs[j], p0[j], p1[j], p1[j]);
  }
  cHL.SaveAs(outDir + "/ResolutionV2_HGvsLG.png");

  // ---------------- pass 2: timing + energy ----------------
  TH1F *hMM = new TH1F("hMM","TR0 grp0 - TR0 grp1;#Deltat [ns];events",1200,-3,3);
  TH1F *hTC[4]; TH1F *hCC = new TH1F("hCC","cap5(s13) - cap4(s14);#Deltat [ns];events",500,-10,10);
  const double abEdge[5] = {300,600,1200,2400,12000};
  TH1F *hAB[4];
  for (int j = 0; j < 4; ++j) {
    hTC[j] = new TH1F(Form("hTC_%d",j),Form("cap %d CFD - TR0;#Deltat [ns];events",LGs[j]),500,-10,10);
    hAB[j] = new TH1F(Form("hAB_%d",j),Form("all caps - TR0, A %d-%d;#Deltat [ns];events",(int)abEdge[j],(int)abEdge[j+1]),500,-10,10);
  }
  TH1F *hSumLG = new TH1F("hSumLG","electrons #Sigma LG;#Sigma LG [ADC-eq];events",90,0,4500);
  long nClipUsed = 0, nBeam = 0;

  for (Long64_t i = 0; i < nEnt; ++i) {
    if (!isBeam[i]) continue;
    t->GetEntry(i);
    ++nBeam;
    ChInfo m0 = chAmp(ch[8], -1, BASE_MOD), m1 = chAmp(ch[17], -1, BASE_MOD);
    double t0 = cfdTime(ch[8],  tax[0], -1, m0.base, m0.amp, m0.pkS, CFD_FRAC);
    double t1 = cfdTime(ch[17], tax[1], -1, m1.base, m1.amp, m1.pkS, CFD_FRAC);
    if (m0.amp > 300 && m1.amp > 300 && t0 > 0 && t1 > 0) hMM->Fill(t0 - t1);

    double S = 0, tcap[4] = {-1,-1,-1,-1}, ainf[4] = {0,0,0,0};
    for (int j = 0; j < 4; ++j) {
      float l = lgA[4*i+j], h = hgA[4*i+j];
      bool clip = hgClip[4*i+j];
      double hInf = clip ? p0[j] + p1[j]*l : h;
      if (clip) ++nClipUsed;
      S += l; ainf[j] = hInf;
      if (hInf > 300 && m1.amp > 300 && t1 > 0) {
        ChInfo hg = chAmp(ch[HGs[j]], +1, BASE_MOD);
        int pk = hg.pkS;
        if (clip) { pk = 0; while (pk < NSAMP && ch[HGs[j]][pk] < gmax[HGs[j]] - 1.0) ++pk; }
        double tc = cfdTime(ch[HGs[j]], tax[1], +1, hg.base, hInf, pk, CFD_FRAC);
        tcap[j] = tc;
        if (tc > 0) { hTC[j]->Fill(tc - t1);
          for (int b = 0; b < 4; ++b) if (hInf >= abEdge[b] && hInf < abEdge[b+1]) hAB[b]->Fill(tc - t1); }
      }
    }
    if (tcap[0] > 0 && tcap[1] > 0 && ainf[0] > 500 && ainf[1] > 500) hCC->Fill(tcap[1] - tcap[0]);
    hSumLG->Fill(S);
    if (i % 5000 == 0) printf("  pass2 %lld/%lld\n", i, nEnt);
  }
  out("\nBeam events %ld; clipped HG recovered %ld\n\n=== TIMING ===\n", nBeam, nClipUsed);

  TCanvas cT("cT","cT",1800,1000); cT.Divide(3,2);
  cT.cd(1); TF1 *gMM = coreFit(hMM,"gMM"); hMM->Draw("hist"); gMM->Draw("same");
  out("TR0 copy-vs-copy (inter-group jitter): %.1f +/- %.1f ps (N=%.0f)\n",
      1e3*gMM->GetParameter(2), 1e3*gMM->GetParError(2), hMM->GetEntries());
  for (int j = 0; j < 4; ++j) {
    cT.cd(j+2); TF1 *g = coreFit(hTC[j], Form("gTC%d", j));
    hTC[j]->Draw("hist"); g->Draw("same");
    out("cap %d - TR0: sigma = %6.1f +/- %5.1f ps (N=%5.0f)\n", LGs[j],
        1e3*g->GetParameter(2), 1e3*g->GetParError(2), hTC[j]->GetEntries());
  }
  cT.cd(6); TF1 *gCC = coreFit(hCC,"gCC"); hCC->Draw("hist"); gCC->Draw("same");
  out("cap5-cap4 (MCP-free): %.1f +/- %.1f ps (N=%.0f) => %.1f ps/capillary\n",
      1e3*gCC->GetParameter(2), 1e3*gCC->GetParError(2), hCC->GetEntries(),
      1e3*gCC->GetParameter(2)/std::sqrt(2.));
  cT.SaveAs(outDir + "/ResolutionV2_timing.png");

  out("amplitude bins (all caps vs TR0):\n");
  for (int b = 0; b < 4; ++b) {
    TF1 *g = coreFit(hAB[b], Form("gAB%d", b));
    out("  A %5d-%5d: %6.1f +/- %5.1f ps (N=%5.0f)\n", (int)abEdge[b], (int)abEdge[b+1],
        1e3*g->GetParameter(2), 1e3*g->GetParError(2), hAB[b]->GetEntries());
  }

  out("\n=== ENERGY (electrons) ===\n");
  TCanvas cE("cE","cE",900,650);
  int pb = 0; double pv = 0;
  for (int b = hSumLG->FindBin(1500); b <= hSumLG->GetNbinsX(); ++b)
    if (hSumLG->GetBinContent(b) > pv) { pv = hSumLG->GetBinContent(b); pb = b; }
  double m = hSumLG->GetBinCenter(pb), s = 700;
  TF1 gE("gE","gaus", m-2*s, m+2*s);
  for (int it = 0; it < 3; ++it) { hSumLG->Fit(&gE,"QNR","",m-1.7*s,m+1.7*s);
    m = gE.GetParameter(1); s = std::fabs(gE.GetParameter(2)); }
  hSumLG->Fit(&gE,"QR","",m-1.7*s,m+1.7*s);
  hSumLG->Draw("hist"); gE.Draw("same");
  cE.SaveAs(outDir + "/ResolutionV2_energy.png");
  out("Sum LG peak: mean %.0f +/- %.0f, sigma %.0f +/- %.0f => sigma/E = %.1f%% +/- %.1f%%\n",
      gE.GetParameter(1), gE.GetParError(1), gE.GetParameter(2), gE.GetParError(2),
      100*gE.GetParameter(2)/gE.GetParameter(1),
      100*gE.GetParameter(2)/gE.GetParameter(1)*
        std::sqrt(std::pow(gE.GetParError(2)/gE.GetParameter(2),2)+std::pow(gE.GetParError(1)/gE.GetParameter(1),2)));
  fclose(sum);
  printf("\nWrote %s/ResolutionV2_{summary.txt,HGvsLG,timing,energy}.png\n", outDir.Data());
}
