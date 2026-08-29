// Diag9.C — split 9 GeV timing diagnosis: run 28 vs run 29.
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TProfile.h"
#include "TH2F.h"
#include "TF1.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include "radStyle.h"
static const int NSAMP=1024; static const double MV2ADC=4.095;
static const int BASE_MOD=40, BASE_CTR=200;
static const int LGs[4]={4,5,6,7}, HGs[4]={14,13,16,15};
struct P{double base,amp;int pk;};
static P pf(const float*w,int pol,int be){P r;r.base=0;for(int s=0;s<be;++s)r.base+=w[s];r.base/=be;
  float pv=w[0];r.pk=0;for(int s=0;s<NSAMP;++s){float v=w[s];if(pol>0?v>pv:v<pv){pv=v;r.pk=s;}}
  r.amp=(pol>0?pv-r.base:r.base-pv)*MV2ADC;return r;}
static double le(const float*w,const float*tx,int pol,double b,double thrA){
  double thr=pol>0?b+thrA/MV2ADC:b-thrA/MV2ADC;
  for(int s=BASE_MOD;s<NSAMP;++s) if(pol>0?w[s]>=thr:w[s]<=thr){
    double v0=w[s-1],v1=w[s]; if(v1==v0)return tx[s];
    return tx[s-1]+(thr-v0)/(v1-v0)*(tx[s]-tx[s-1]);}
  return -1e9;}
void Diag9(){
  SetRadStyle();
  const char* files[2]={"data/DATA/RUN_LUAG_9GEV/run_28.root","data/DATA/RUN_LUAG_9GEV/run_29.root"};
  for(int fi=0;fi<2;++fi){
    TFile*f=TFile::Open(files[fi]);
    TTree*t=(TTree*)f->Get("pulse");
    static float ch[18][NSAMP],tx[2][NSAMP];
    t->SetBranchStatus("*",0);t->SetBranchStatus("channel",1);t->SetBranchStatus("times",1);
    t->SetBranchAddress("channel",ch);t->SetBranchAddress("times",tx);
    Long64_t N=t->GetEntries();
    // transfer (crude): a,b from profile fit below 0.72*wall
    TH1F hA("hA","",128,0,3200); TH2F hHL("hHL","",120,0,1200,128,0,3200);
    std::vector<char> isB(N);
    double mcpS=0; long nB=0;
    for(Long64_t i=0;i<N;++i){t->GetEntry(i);
      P c0=pf(ch[0],-1,BASE_CTR),c1=pf(ch[1],-1,BASE_CTR);
      isB[i]=c0.amp>40&&c1.amp>40;
      if(!isB[i])continue; ++nB;
      mcpS+=pf(ch[17],-1,BASE_MOD).amp;
      for(int j=0;j<4;++j){P l=pf(ch[LGs[j]],1,BASE_MOD),h=pf(ch[HGs[j]],1,BASE_MOD);
        hA.Fill(h.amp); hHL.Fill(l.amp,h.amp);}}
    double wall,q=0.995; hA.GetQuantiles(1,&wall,&q);
    TProfile*pr=hHL.ProfileX("pd");
    double lgMax=1200,ce=0.72*wall;
    for(int b=pr->FindBin(60);b<=pr->GetNbinsX();++b)
      if(pr->GetBinEntries(b)>3&&pr->GetBinContent(b)>ce){lgMax=pr->GetBinCenter(b);break;}
    TF1 fl("fl","pol1",30,lgMax); pr->Fit(&fl,"QRN","",30,lgMax);
    double a=fl.GetParameter(0),b=fl.GetParameter(1);
    // shower time
    std::vector<double> dt;
    double pkS=0; long npk=0;
    for(Long64_t i=0;i<N;++i){if(!isB[i])continue;t->GetEntry(i);
      P m1=pf(ch[17],-1,BASE_MOD); if(m1.amp<300)continue;
      double t1=le(ch[17],tx[1],-1,m1.base,0.20*m1.amp); if(t1<-1e8)continue;
      double ts=0;int nOK=0;
      for(int j=0;j<4;++j){P l=pf(ch[LGs[j]],1,BASE_MOD),h=pf(ch[HGs[j]],1,BASE_MOD);
        double thr=0.15*(a+b*l.amp);
        if(thr<20*MV2ADC||thr>0.9*wall||h.amp<thr)continue;
        double tc=le(ch[HGs[j]],tx[1],1,h.base,thr);
        if(tc>-1e8){ts+=tc;++nOK; pkS+=h.pk; ++npk;}}
      if(nOK>=2)dt.push_back(ts/nOK-t1);}
    // robust width
    double rc=0,rw=0;{double s=0,s2=0;for(double x:dt){s+=x;s2+=x*x;}
      rc=s/dt.size();rw=std::sqrt(s2/dt.size()-rc*rc);}
    for(int it=0;it<5;++it){double s=0,s2=0;long n=0;
      for(double x:dt)if(std::fabs(x-rc)<2.5*rw){s+=x;s2+=x*x;++n;}
      if(n<30)break;rc=s/n;rw=std::sqrt(std::max(0.0,s2/n-rc*rc));}
    printf("%s: N=%lld beam(thr40)=%ld MCPmean=%.0f transfer=%.0f+%.2fLG wall=%.0f\n",
      files[fi], N, nB, mcpS/nB, a, b, wall);
    printf("   shower-time: N=%zu core sigma=%.0f ps, center=%.2f ns, HG peak-sample mean=%.0f\n",
      dt.size(), rw*1000/0.9546, rc, npk?pkS/npk:0);
  }
}

void Diag9Drift(){
  SetRadStyle();
  TFile*f=TFile::Open("data/DATA/RUN_LUAG_9GEV/run_29.root");
  TTree*t=(TTree*)f->Get("pulse");
  static float ch[18][NSAMP],tx[2][NSAMP];
  t->SetBranchStatus("*",0);t->SetBranchStatus("channel",1);t->SetBranchStatus("times",1);
  t->SetBranchAddress("channel",ch);t->SetBranchAddress("times",tx);
  Long64_t N=t->GetEntries();
  const double a=183,b=2.59,wall=3181;
  const int NCH2=8; std::vector<double> dt[NCH2];
  for(Long64_t i=0;i<N;++i){t->GetEntry(i);
    P c0=pf(ch[0],-1,BASE_CTR),c1=pf(ch[1],-1,BASE_CTR);
    if(!(c0.amp>40&&c1.amp>40))continue;
    P m1=pf(ch[17],-1,BASE_MOD); if(m1.amp<300)continue;
    double t1=le(ch[17],tx[1],-1,m1.base,0.20*m1.amp); if(t1<-1e8)continue;
    double ts=0;int nOK=0;
    for(int j=0;j<4;++j){P l=pf(ch[LGs[j]],1,BASE_MOD),h=pf(ch[HGs[j]],1,BASE_MOD);
      double thr=0.15*(a+b*l.amp);
      if(thr<20*MV2ADC||thr>0.9*wall||h.amp<thr)continue;
      double tc=le(ch[HGs[j]],tx[1],1,h.base,thr);
      if(tc>-1e8){ts+=tc;++nOK;}}
    if(nOK>=2)dt[(int)(i*NCH2/N)].push_back(ts/nOK-t1);
  }
  printf("run 29 in 8 time slices (chronological):\n");
  for(int k=0;k<NCH2;++k){
    if(dt[k].size()<20){printf("  slice %d: N=%zu (too few)\n",k,dt[k].size());continue;}
    double rc=0,rw=0;{double s=0,s2=0;for(double x:dt[k]){s+=x;s2+=x*x;}
      rc=s/dt[k].size();rw=std::sqrt(s2/dt[k].size()-rc*rc);}
    for(int it=0;it<5;++it){double s=0,s2=0;long n=0;
      for(double x:dt[k])if(std::fabs(x-rc)<2.5*rw){s+=x;s2+=x*x;++n;}
      if(n<20)break;rc=s/n;rw=std::sqrt(std::max(0.0,s2/n-rc*rc));}
    printf("  slice %d: N=%3zu  center=%+7.0f ps  core sigma=%4.0f ps\n",
      k,dt[k].size(),rc*1000,rw*1000/0.9546);
  }
}
