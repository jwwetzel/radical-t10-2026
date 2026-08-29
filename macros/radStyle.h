// radStyle.h — house plot style for the T10 analysis: bold, simple, clear.
#pragma once
#include "TStyle.h"
#include "TROOT.h"
#include "TColor.h"
#include "TH1.h"
#include "TF1.h"
#include "TLatex.h"
#include "TGaxis.h"

// palette
namespace rad {
  inline int cInk()    { static int c = TColor::GetColor("#1A2027"); return c; }
  inline int cTeal()   { static int c = TColor::GetColor("#0E7C86"); return c; }
  inline int cRed()    { static int c = TColor::GetColor("#C6402F"); return c; }
  inline int cAmber()  { static int c = TColor::GetColor("#D9930D"); return c; }
  inline int cBlue()   { static int c = TColor::GetColor("#2B5DA8"); return c; }
  inline int cGrey()   { static int c = TColor::GetColor("#9AA6AD"); return c; }
  inline int cViolet() { static int c = TColor::GetColor("#6B4FA1"); return c; }
  inline int cFill()   { static int c = TColor::GetColorTransparent(TColor::GetColor("#0E7C86"), 0.22); return c; }
  inline int cBand()   { static int c = TColor::GetColorTransparent(TColor::GetColor("#D9930D"), 0.18); return c; }
}

inline void SetRadStyle()
{
  TStyle *st = new TStyle("rad", "RADiCAL T10");
  st->SetOptStat(0); st->SetOptTitle(1); st->SetOptFit(0);
  // canvas / pads
  st->SetCanvasColor(0); st->SetPadColor(0); st->SetFrameFillColor(0);
  st->SetCanvasBorderMode(0); st->SetPadBorderMode(0); st->SetFrameBorderMode(0);
  st->SetPadTopMargin(0.085); st->SetPadBottomMargin(0.13);
  st->SetPadLeftMargin(0.125); st->SetPadRightMargin(0.045);
  st->SetPadTickX(1); st->SetPadTickY(1);
  // fonts: 43 = Helvetica, size in px
  const int f = 43;
  st->SetTitleFont(f, "XYZ"); st->SetLabelFont(f, "XYZ");
  st->SetTitleSize(21, "XYZ"); st->SetLabelSize(18, "XYZ");
  st->SetTitleOffset(1.15, "X"); st->SetTitleOffset(1.55, "Y");
  st->SetTitleFont(f, ""); st->SetTitleSize(24, "");     // pad title
  st->SetTitleBorderSize(0); st->SetTitleFillColor(0);
  st->SetTitleX(0.125); st->SetTitleAlign(11);           // left-aligned, flush with frame
  st->SetTitleStyle(0);
  // frame & lines
  st->SetFrameLineWidth(2); st->SetLineWidth(2);
  st->SetHistLineWidth(3);  st->SetFuncWidth(3);
  st->SetHistLineColor(rad::cInk());
  st->SetMarkerStyle(20); st->SetMarkerSize(0.9);
  st->SetEndErrorSize(0);
  st->SetLegendBorderSize(0); st->SetLegendFillColor(0);
  st->SetLegendFont(f); st->SetLegendTextSize(18);
  st->SetGridColor(rad::cGrey()); st->SetGridStyle(3); st->SetGridWidth(1);
  st->SetNdivisions(507, "XYZ");
  TGaxis::SetMaxDigits(4);
  gROOT->SetStyle("rad"); gROOT->ForceStyle();
}

// small caption helper (bottom-left of current pad)
inline void RadTag(const char *txt)
{
  TLatex l; l.SetNDC(); l.SetTextFont(43); l.SetTextSize(15);
  l.SetTextColor(rad::cGrey());
  l.DrawLatex(0.13, 0.015, txt);
}
