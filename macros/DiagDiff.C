// DiagDiff.C — Randy's diagonal-difference estimator: reference-free timing.
//
// Every capillary time is measured against the MCP, so every published
// sigma_t carries the MCP+DRS reference jitter. Trick: average the two
// diagonal capillary pairs (NW+SE = caps 4+7, NE+SW = caps 5+6) and take
// the DIFFERENCE of the averages — the common reference cancels exactly,
// event by event, and the diagonal choice also cancels beam-position drift
// to first order. For four equal capillaries:
//     sigma(diff)/2  =  intrinsic 4-capillary-average resolution.
// On the same events we also compute the published-style sigma(mean - MCP);
// the quadrature gap between the two is the implied reference jitter,
// which must come out ~constant across energies if everything is consistent.
//
// Usage: root -l -b -q 'macros/DiagDiff.C+(0)'   // 0=LuAG 1=DSB1 2=EJ199
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TF1.h"
#include "TSystem.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15}; // 4=TL(NW) 5=TR(NE) 6=BL(SW) 7=BR(SE)
static const double SRCFD_FRAC = 0.15, THR_MIN = 20.0 * MV2ADC;

struct Pulse { double base, amp; };
static Pulse pulseOf(const float *w, int pol, int be)
{
  Pulse r; r.base = 0;
  for (int s = 0; s < be; ++s) r.base += w[s]; r.base /= be;
  float pk = w[0];
  for (int s = 1; s < NSAMP; ++s) if (pol > 0 ? w[s] > pk : w[s] < pk) pk = w[s];
  r.amp = (pol > 0 ? pk - r.base : r.base - pk) * MV2ADC;
  return r;
}
static double leTime(const float *w, const float *tax, int pol, double baseMV, double thrADC)
{
  const double thr = pol > 0 ? baseMV + thrADC/MV2ADC : baseMV - thrADC/MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s)
    if (pol > 0 ? w[s] >= thr : w[s] <= thr) {
      double v0 = w[s-1], v1 = w[s];
      if (v1 == v0) return tax[s];
      return tax[s-1] + (thr - v0)/(v1 - v0)*(tax[s] - tax[s-1]);
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
  if (!(rw > 1e-6) || !std::isfinite(rc)) return -1;
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

void DiagDiff(int mod = 0)
{
  SetRadStyle();
  const char *MODS[3] = {"LuAG","DSB1","EJ199"};
  const int   NRUNS[3] = {6,6,6};
  int  RUNS[3][6] = {{2526,24,15,27,9001,30},{38,35,37,33,32,31},{39,40,41,42,43,44}};
  double EE[3][6] = {{1,3,5,7,9,11},{1,3,5,7,9,11},{1,3,5,7,9,11}};
  double CT[3][6] = {{100,100,100,50,40,40},{100,100,100,40,40,40},{100,100,100,40,40,40}};
  double FL[3][6] = {{180,300,420,540,660,3800},{180,1500,2500,4000,6000,6500},{180,500,1200,540,660,780}};

  gSystem->mkdir("Output/summary", true);
  FILE *sum = fopen(Form("Output/summary/DiagDiff_%s.txt", MODS[mod]), "w");
  auto out = [&](const char *f, ...) { va_list a; va_start(a,f); vfprintf(stdout,f,a); va_end(a);
    va_start(a,f); vfprintf(sum,f,a); va_end(a); fflush(sum); };

  out("# %s: E  N4  sigma_meanIncl[ps] err  sigma_intr[ps] err  implied_ref[ps]\n", MODS[mod]);
  for (int ip = 0; ip < NRUNS[mod]; ++ip) {
    int run = RUNS[mod][ip]; double cthr = CT[mod][ip], SMIN = FL[mod][ip];
    TFile *f = TFile::Open(Form("data/download/run_%d.root", run));
    if (!f || f->IsZombie()) { out("%g SKIP no file\n", EE[mod][ip]); continue; }
    TTree *t = (TTree*)f->Get("pulse");
    static float ch[NSLOT][NSAMP], tax[2][NSAMP];
    t->SetBranchStatus("*",0); t->SetBranchStatus("channel",1); t->SetBranchStatus("times",1);
    t->SetBranchAddress("channel", ch); t->SetBranchAddress("times", tax);
    Long64_t nEnt = t->GetEntries();

    // pass 1: wall-aware transfer (as EnergyScan)
    TH1F *hA[4]; TH2F *hHL[4];
    for (int j = 0; j < 4; ++j) {
      hA[j] = new TH1F(Form("dA%d",j),"",128,0,3200);
      hHL[j] = new TH2F(Form("dL%d",j),"",120,0,1200,128,0,3200);
    }
    std::vector<char> isB(nEnt);
    for (Long64_t i = 0; i < nEnt; ++i) {
      t->GetEntry(i);
      Pulse c0 = pulseOf(ch[0],-1,BASE_CTR), c1 = pulseOf(ch[1],-1,BASE_CTR);
      isB[i] = c0.amp > cthr && c1.amp > cthr;
      if (!isB[i]) continue;
      for (int j = 0; j < 4; ++j) {
        Pulse l = pulseOf(ch[LGs[j]],+1,BASE_MOD), h = pulseOf(ch[HGs[j]],+1,BASE_MOD);
        hA[j]->Fill(h.amp); hHL[j]->Fill(l.amp, h.amp);
      }
    }
    double wall[4], a[4], b[4];
    for (int j = 0; j < 4; ++j) {
      double q = 0.995; hA[j]->GetQuantiles(1, &wall[j], &q);
      TProfile *pr = hHL[j]->ProfileX(Form("dp%d",j));
      double lgMax = 1200, ceil_ = 0.72*wall[j];
      for (int bb = pr->FindBin(60); bb <= pr->GetNbinsX(); ++bb)
        if (pr->GetBinEntries(bb) > 3 && pr->GetBinContent(bb) > ceil_) { lgMax = pr->GetBinCenter(bb); break; }
      TF1 fl("fl","pol1",30,lgMax); pr->Fit(&fl,"QRN","",30,lgMax);
      a[j] = fl.GetParameter(0); b[j] = fl.GetParameter(1);
      delete hA[j]; delete hHL[j];
    }

    // pass 2: same gates as the scans, but require ALL FOUR capillary times
    std::vector<double> dMean, dDiag;
    for (Long64_t i = 0; i < nEnt; ++i) {
      if (!isB[i]) continue;
      t->GetEntry(i);
      double S = 0;
      for (int j = 0; j < 4; ++j) S += pulseOf(ch[LGs[j]],+1,BASE_MOD).amp;
      if (S <= SMIN) continue;
      Pulse m1 = pulseOf(ch[17],-1,BASE_MOD);
      if (m1.amp < 300) continue;
      double t1 = leTime(ch[17], tax[1], -1, m1.base, 0.20*m1.amp);
      if (t1 < -1e8) continue;
      double tc[4]; bool ok = true;
      for (int j = 0; j < 4; ++j) {
        Pulse l = pulseOf(ch[LGs[j]],+1,BASE_MOD), h = pulseOf(ch[HGs[j]],+1,BASE_MOD);
        double thr = SRCFD_FRAC*(a[j] + b[j]*l.amp);
        if (thr < THR_MIN || thr > 0.9*wall[j] || h.amp < thr) { ok = false; break; }
        tc[j] = leTime(ch[HGs[j]], tax[1], +1, h.base, thr);
        if (tc[j] < -1e8) { ok = false; break; }
      }
      if (!ok) continue;
      dMean.push_back(0.25*(tc[0]+tc[1]+tc[2]+tc[3]) - t1);
      dDiag.push_back(0.5*(tc[0]+tc[3]) - 0.5*(tc[1]+tc[2]));  // (NW+SE)/2 - (NE+SW)/2
    }
    f->Close();

    double eM, eD;
    double sM = tebSigma(dMean, &eM);
    double sD = tebSigma(dDiag, &eD);
    double intr = sD > 0 ? sD/2.0 : -1, intrE = eD/2.0;
    double ref = (sM > 0 && intr > 0 && sM > intr) ? std::sqrt(sM*sM - intr*intr) : -1;
    out("%g  %zu  %.1f %.1f   %.1f %.1f   %.1f\n",
        EE[mod][ip], dMean.size(), sM, eM, intr, intrE, ref);
  }
  fclose(sum);
  printf("done %s\n", MODS[mod]);
  gSystem->Exit(0);
}
