// Tertiary11.C — what ARE the 11 GeV tags? The T10 composition tables say the
// electron fraction reaches zero above ~10 GeV/c, yet both modules record a
// real (accidental-free) XCET coincidence at -11 GeV. Hypotheses:
//   (a) soft tertiary electrons travelling with the beam (tables right),
//   (b) genuine ~11 GeV electrons the tables miss (as our in-situ counts
//       exceeded the tables at 7 and 9 GeV).
// Discriminators, all from data in hand:
//   1. SumLG spectrum of tagged events, classed miss/partial/contained, with
//      the 9 GeV tagged spectrum overlaid — soft electrons cannot make a
//      contained peak at ~the 9 GeV position;
//   2. energy-equivalent of the contained peak vs the module response curve
//      (incl. shower-max migration);
//   3. MCP amplitude spectra: tagged-contained vs tagged-miss vs untagged pi
//      (MCP pulse height is species-correlated);
//   4. (from the scans) contained timing: DSB1 181 ps at "11 GeV" vs 366 ps
//      at 1 GeV — contained tags time like hard showers.
#include "radStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include <cmath>
#include <cstdio>

static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int LGs[4] = {4,5,6,7};

static double ampOf(const float *w, int pol, int be)
{
  double b = 0; for (int k = 0; k < be; ++k) b += w[k]; b /= be;
  float ex = w[0];
  for (int k = 1; k < NSAMP; ++k) if (pol > 0 ? w[k] > ex : w[k] < ex) ex = w[k];
  return (pol > 0 ? ex - b : b - ex) * MV2ADC;
}
static double longoF(double E, double b = 0.5, double Ec = 10.0)
{
  auto frac = [&](double e){ double tm = std::log(e*1000/Ec) - 0.5, a = b*tm + 1;
    double t1 = (std::log(5000/Ec) - 0.5) - 0.75, t2 = t1 + 1.5, s = 0;
    for (int i = 0; i < 200; ++i) { double t = t1 + (t2-t1)*(i+0.5)/200;
      s += std::exp((a-1)*std::log(b*t) - b*t + std::log(b) - std::lgamma(a)); }
    return s*(t2-t1)/200; };
  return frac(E)/frac(5.0);
}

struct RunSet { TH1F *hs[3]; TH1F *hm[3]; TH1F *h9; long n[3]; double peak, peakE; };

static RunSet process(int run11, int run9, double cthr, double floorC, double smax)
{
  RunSet R;
  const char *cls[3] = {"miss","partial","contained"};
  for (int c = 0; c < 3; ++c) {
    R.hs[c] = new TH1F(Form("hs%d_%d",run11,c),"",160,0,smax); R.hs[c]->SetDirectory(nullptr);
    R.hm[c] = new TH1F(Form("hm%d_%d",run11,c),"",100,0,2500); R.hm[c]->SetDirectory(nullptr);
    R.n[c] = 0;
  }
  R.h9 = new TH1F(Form("h9_%d",run9),"",160,0,smax); R.h9->SetDirectory(nullptr);
  // -11 GeV run
  {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", run11));
    TTree *t = (TTree*)f->Get("pulse");
    Float_t ch[NSLOT][NSAMP]; t->SetBranchAddress("channel", ch);
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
      t->GetEntry(i);
      bool tag = ampOf(ch[0],-1,200) > cthr && ampOf(ch[1],-1,200) > cthr;
      double mcp = ampOf(ch[17],-1,40);
      if (!tag) {                                     // untagged = ~97% pi at -11 GeV
        if (i % 5 == 0) R.hm[0]->Fill(mcp);           // subsample for shape
        continue;
      }
      double S = 0; for (int j = 0; j < 4; ++j) S += ampOf(ch[LGs[j]],+1,40);
      int c = S < 400 ? 1 : (S < floorC ? -1 : 2);    // hm: 0=pi,1=tag-miss,2=tag-contained
      if (S < 400)            { R.hs[0]->Fill(S); ++R.n[0]; }
      else if (S < floorC)    { R.hs[1]->Fill(S); ++R.n[1]; }
      else                    { R.hs[2]->Fill(S); ++R.n[2]; }
      if (c > 0) R.hm[c]->Fill(mcp);
    }
    f->Close();
  }
  // 9 GeV overlay (tagged, contained region only for shape)
  {
    TFile *f = TFile::Open(Form("data/download/run_%d.root", run9));
    TTree *t = (TTree*)f->Get("pulse");
    Float_t ch[NSLOT][NSAMP]; t->SetBranchAddress("channel", ch);
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
      t->GetEntry(i);
      if (ampOf(ch[0],-1,200) < 40 || ampOf(ch[1],-1,200) < 40) continue;
      double S = 0; for (int j = 0; j < 4; ++j) S += ampOf(ch[LGs[j]],+1,40);
      R.h9->Fill(S);
    }
    f->Close();
  }
  // contained peak fit
  TF1 g("g","gaus", floorC, smax);
  double m = R.hs[2]->GetBinCenter(R.hs[2]->GetMaximumBin()), s = 0.15*m;
  for (int it = 0; it < 3; ++it) {
    R.hs[2]->Fit(&g, "QRN", "", std::max(floorC, m-1.6*s), m+1.6*s);
    m = g.GetParameter(1); s = std::fabs(g.GetParameter(2));
  }
  R.peak = m; R.peakE = g.GetParError(1);
  return R;
}

void Tertiary11()
{
  SetRadStyle();
  gSystem->mkdir("Output/summary", true);
  FILE *sum = fopen("Output/summary/Tertiary11_summary.txt","w");
  auto out = [&](const char *f, ...) { va_list a; va_start(a,f); vfprintf(stdout,f,a); va_end(a);
    va_start(a,f); vfprintf(sum,f,a); va_end(a); };

  RunSet L = process(30, 9001, 40, 3800, 12000);      // LuAG
  RunSet D = process(31, 32,   40, 6500, 16000);      // DSB1

  // energy equivalent: solve peak = slope * E * longo(E)
  auto eEq = [&](double peak, double slope){ double E = peak/slope;
    for (int i = 0; i < 20; ++i) E = peak/(slope*longoF(E)); return E; };
  double eL = eEq(L.peak, 618.2), eD = eEq(D.peak, 1353.8);
  out("LuAG  run 30: miss %ld / partial %ld / contained %ld | contained peak %.0f+/-%.0f -> E-equivalent %.1f GeV\n",
      L.n[0], L.n[1], L.n[2], L.peak, L.peakE, eL);
  out("DSB1  run 31: miss %ld / partial %ld / contained %ld | contained peak %.0f+/-%.0f -> E-equivalent %.1f GeV (LG ceiling compresses)\n",
      D.n[0], D.n[1], D.n[2], D.peak, D.peakE, eD);
  out("timing (from scans): contained tags 227 ps (LuAG) / 181 ps (DSB1) vs 412/366 ps at 1 GeV — hard-shower-like\n");

  TCanvas c("c","c",2000,1150); c.Divide(2,2,0.004,0.006);
  TLatex tx; tx.SetNDC(); tx.SetTextFont(43);
  auto specPanel = [&](RunSet &R, const char *mod, double floorC, double smax, double eeq){
    gPad->SetLogy();
    for (int k = 0; k < 3; ++k) R.hs[k]->Rebin(2);
    R.h9->Rebin(2);
    R.hs[0]->SetLineColor(kGray+2);  R.hs[0]->SetLineWidth(2);
    R.hs[1]->SetLineColor(rad::cAmber()); R.hs[1]->SetLineWidth(2);
    R.hs[2]->SetLineColor(rad::cTeal());  R.hs[2]->SetLineWidth(3);
    double sc = R.hs[2]->Integral() > 0 ? R.hs[2]->Integral()/std::max(1.0, R.h9->Integral(R.h9->FindBin(floorC), 160)) : 1;
    R.h9->Scale(sc);
    R.h9->SetLineColor(rad::cRed()); R.h9->SetLineWidth(2); R.h9->SetLineStyle(7);
    TH1F *fr = (TH1F*)gPad->DrawFrame(0, 0.4, smax, 3*std::max(R.hs[0]->GetMaximum(), R.h9->GetMaximum()));
    fr->SetXTitle("#SigmaLG [ADC-eq]"); fr->SetYTitle("tagged events");
    R.hs[0]->Draw("hist same"); R.hs[1]->Draw("hist same"); R.hs[2]->Draw("hist same"); R.h9->Draw("hist same");
    TLegend *l = new TLegend(0.44,0.62,0.93,0.88); l->SetBorderSize(0); l->SetTextFont(43); l->SetTextSize(19);
    l->AddEntry(R.hs[0], Form("miss (%ld)", R.n[0]), "l");
    l->AddEntry(R.hs[1], Form("partial (%ld)", R.n[1]), "l");
    l->AddEntry(R.hs[2], Form("contained (%ld), E-eq %.1f GeV", R.n[2], eeq), "l");
    l->AddEntry(R.h9,  "9 GeV tags (scaled)", "l");
    l->Draw();
    tx.SetTextSize(23); tx.SetTextColor(kBlack);
    tx.DrawLatex(0.13,0.94, Form("#font[62]{%s: #minus11 GeV tagged #SigmaLG vs the 9 GeV shape}", mod));
  };
  c.cd(1); specPanel(L, "LuAG", 3800, 12000, eL);
  c.cd(2); specPanel(D, "DSB1", 6500, 16000, eD);

  auto mcpPanel = [&](RunSet &R, const char *mod){
    gPad->SetLogy();
    for (int k = 0; k < 3; ++k) if (R.hm[k]->Integral() > 0) R.hm[k]->Scale(1.0/R.hm[k]->Integral());
    R.hm[0]->SetLineColor(rad::cGrey()); R.hm[0]->SetLineWidth(2);
    R.hm[1]->SetLineColor(rad::cAmber()); R.hm[1]->SetLineWidth(2);
    R.hm[2]->SetLineColor(rad::cTeal()); R.hm[2]->SetLineWidth(3);
    double mx = std::max({R.hm[0]->GetMaximum(), R.hm[1]->GetMaximum(), R.hm[2]->GetMaximum()});
    TH1F *fr = (TH1F*)gPad->DrawFrame(0, 1e-4, 2500, 4*mx);
    fr->SetXTitle("MCP amplitude [ADC-eq]"); fr->SetYTitle("fraction");
    R.hm[0]->Draw("hist same"); R.hm[1]->Draw("hist same"); R.hm[2]->Draw("hist same");
    TLegend *l = new TLegend(0.52,0.64,0.93,0.88); l->SetBorderSize(0); l->SetTextFont(43); l->SetTextSize(19);
    l->AddEntry(R.hm[0], "untagged (~97% #pi)", "l");
    l->AddEntry(R.hm[1], "tagged, miss module", "l");
    l->AddEntry(R.hm[2], "tagged, contained", "l");
    l->Draw();
    tx.SetTextSize(23); tx.DrawLatex(0.13,0.94, Form("#font[62]{%s: MCP pulse height by class}", mod));
  };
  c.cd(3); mcpPanel(L, "LuAG");
  c.cd(4); mcpPanel(D, "DSB1");

  c.SaveAs("Output/summary/Tertiary11.png");
  fclose(sum);
  printf("Wrote Output/summary/Tertiary11.png + summary\n");
  gSystem->Exit(0);
}
