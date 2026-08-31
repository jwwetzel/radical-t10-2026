// MakeMergedRuns.C — reproduces every derived ROOT file in data/download/.
// Documents provenance (audit item): run the whole thing with
//   root -l -b -q 'macros/MakeMergedRuns.C+'
// Requires the parent runs in data/download/ (symlinks to data/DATA/...).
#include "TFile.h"
#include "TTree.h"
#include "TSystem.h"
#include <cstdio>
#include <vector>

static void concat(const char *out, std::vector<const char*> parents)
{
  TFile *fo = TFile::Open(out, "RECREATE");
  TTree *merged = nullptr;
  for (const char *p : parents) {
    TFile *fp = TFile::Open(p);
    TTree *t = (TTree*)fp->Get("pulse");
    fo->cd();
    if (!merged) merged = t->CloneTree(-1, "fast");
    else merged->CopyEntries(t);
    fp->Close();
  }
  fo->cd(); merged->Write(); 
  printf("%s: %lld entries\n", out, merged->GetEntries());
  fo->Close();
}

void MakeMergedRuns()
{
  // simple concatenations
  concat("data/download/run_2223.root", {"data/download/run_22.root", "data/download/run_23.root"});
  concat("data/download/run_2526.root", {"data/download/run_25.root", "data/download/run_26.root"});
  concat("data/download/run_2829.root", {"data/download/run_28.root", "data/download/run_29.root"});
  // run_29good = run 29 DQ-good slices: entries [0,10000) + [20000,25000) + [30000,35000)
  // (drift-quiet periods per the run-29 DriftStudy slicing; see FINDINGS "9 GeV DQ")
  {
    TFile *fp = TFile::Open("data/download/run_29.root");
    TTree *t = (TTree*)fp->Get("pulse");
    TFile *fo = TFile::Open("data/download/run_29good.root", "RECREATE");
    TTree *g = t->CloneTree(0);
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
      bool keep = (i < 10000) || (i >= 20000 && i < 25000) || (i >= 30000 && i < 35000);
      if (!keep) continue;
      t->GetEntry(i); g->Fill();
    }
    fo->cd(); g->Write(); printf("run_29good.root: %lld entries\n", g->GetEntries());
    fo->Close(); fp->Close();
  }
  // run_9001 = run 28 (all) + run_29good
  concat("data/download/run_9001.root", {"data/download/run_28.root", "data/download/run_29good.root"});
  gSystem->Exit(0);
}
