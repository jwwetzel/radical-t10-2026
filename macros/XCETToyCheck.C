// XCETToyCheck.C — toy Monte Carlo closure test of the counting-bootstrap
// solver, calling the REAL code: this file #includes XCETTagProbe.C, so
// f0Model / lamOf / solveBoot below are the exact compiled functions the
// analysis used — not a re-implementation. Only the fake/miss correction
// algebra (3 lines) is re-typed here, verbatim from the macro.
//
// Verdict sought: with events generated at known (lambda, A1, fakes, miss,
// noise), does the pipeline return A1? And do the quoted systematic recipes
// cover deliberate mis-specifications (wrong r, wrong fake rate, non-Gaussian
// 1-pe low tail)?
//
// Usage: root -l -b -q 'macros/XCETToyCheck.C+'
#include "XCETTagProbe.C"
#include "TRandom3.h"

static TRandom3 rng(20260901);

static double promptNoise(double zcut)
{
  if (rng.Uniform() < 0.01) return zcut + rng.Uniform(0.2, 6.0);   // leak-up 1% by construction
  double v = rng.Gaus(3.0, 1.5);
  if (v < 0) v = 0; if (v > zcut - 0.2) v = zcut - 0.2;
  return v;
}

static double gausPos(double m, double s)
{ double v = rng.Gaus(m, s); return v < 0.3 ? 0.3 : v; }

struct ToyCfg {
  const char *name;
  double A1t, lam, zcut, bPed;   // truth + per-counter noise metrology
  double rTrue;                  // generated relative 1-pe width
  double phiTrue, phiAssumed;    // generated vs assumed fake fraction
  bool skew1pe;                  // heavier-than-Gaussian low tail on the 1-pe
  int N;
};

void XCETToyCheck()
{
  const double FM = 0.0005, BPED_S = 3.0, RSOLVE = 0.36, LEAK = 0.01;
  const int NTOY = 300;
  ToyCfg cfgs[] = {
    {"43-like nominal lam1.5", 21.8, 1.5, 15.7, 7.3, 0.36, 0.030, 0.030, false, 800},
    {"43-like nominal lam2.5", 21.8, 2.5, 15.7, 7.3, 0.36, 0.030, 0.030, false, 800},
    {"40-like nominal lam1.5", 40.2, 1.5,  5.6, 7.9, 0.36, 0.000, 0.000, false, 500},
    {"40-like nominal lam3.0", 40.2, 3.0,  5.6, 7.9, 0.36, 0.000, 0.000, false, 900},
    {"43 lam4.0 validity edge", 21.8, 4.0, 15.7, 7.3, 0.36, 0.030, 0.030, false, 1200},
    {"43 STRESS r=0.30 gen, 0.36 solve", 21.8, 1.8, 15.7, 7.3, 0.30, 0.030, 0.030, false, 800},
    {"43 STRESS fakes 2x under-assumed", 21.8, 1.8, 15.7, 7.3, 0.36, 0.060, 0.030, false, 800},
    {"43 STRESS skewed 1-pe low tail",   21.8, 1.8, 15.7, 7.3, 0.36, 0.030, 0.030, true,  800},
    {"40 STRESS skewed 1-pe low tail",   40.2, 1.8,  5.6, 7.9, 0.36, 0.000, 0.000, true,  800},
  };
  printf("# %d toys/config; solver = the compiled solveBoot/f0Model/lamOf from XCETTagProbe.C\n", NTOY);
  printf("%-36s  trueA1   <A1boot>   bias%%    toyRMS%%   solve-fail%%\n", "# config");
  for (auto &cf : cfgs) {
    double sum = 0, sum2 = 0; int nOK = 0, nFail = 0;
    for (int it = 0; it < NTOY; ++it) {
      long nZ = 0; double sM = 0;
      for (int i = 0; i < cf.N; ++i) {
        double aP, aG;
        double u = rng.Uniform();
        if (u < cf.phiTrue + FM) { aP = promptNoise(cf.zcut); aG = gausPos(cf.bPed, BPED_S); }
        else {
          int n = rng.Poisson(cf.lam);
          if (n == 0) { aP = promptNoise(cf.zcut); aG = gausPos(cf.bPed, BPED_S); }
          else {
            double amp;
            if (cf.skew1pe && n == 1) {          // asymmetric: heavier low side, same mode
              double g = rng.Gaus(0, cf.rTrue*cf.A1t);
              amp = cf.A1t + (g < 0 ? 1.5*g : 0.7*g);
            } else
              amp = n*cf.A1t + rng.Gaus(0, std::sqrt((double)n)*cf.rTrue*cf.A1t);
            if (amp < 0.5) amp = 0.5;
            aP = amp; aG = amp;
          }
        }
        if (aP < cf.zcut) ++nZ;
        sM += aG;
      }
      double f0o = (double)nZ/cf.N, meanG = sM/cf.N;
      // correction algebra, verbatim from XCETTagProbe.C solveCorr
      double phix = cf.phiAssumed, fM = FM;
      double f0c = std::max(1e-4, ((f0o - phix)/(1 - phix) - fM)/(1 - fM));
      double mc  = ((meanG - phix*cf.bPed)/(1 - phix) - fM*cf.bPed)/(1 - fM);
      double lam, A1;
      if (!solveBoot(f0c, mc, cf.bPed, cf.zcut, RSOLVE, LEAK, cf.A1t, lam, A1)) { ++nFail; continue; }
      sum += A1; sum2 += A1*A1; ++nOK;
    }
    if (nOK < 10) { printf("%-36s  %.1f     -- solver failed in %d/%d toys --\n", cf.name, cf.A1t, nFail, NTOY); continue; }
    double m = sum/nOK, rms = std::sqrt(std::max(0.0, sum2/nOK - m*m));
    printf("%-36s  %5.1f    %6.2f    %+6.2f    %6.2f     %.1f\n",
           cf.name, cf.A1t, m, 100*(m-cf.A1t)/cf.A1t, 100*rms/cf.A1t, 100.0*nFail/NTOY);
  }
  gSystem->Exit(0);
}
