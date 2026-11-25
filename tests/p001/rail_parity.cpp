#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "dsp/Amp5150.h"
using namespace vamp;
int main(int argc,char** argv){
 if(argc!=2)return 2;
 FILE* f=std::fopen(argv[1],"wb"); if(!f)return 2;
 constexpr double pi=3.14159265358979323846;
 for(double fs:{88200.,176400.,384000.}){
  Preamp5150 pre; PowerAmp5150 pa;
  pre.setChannelTrims(54.,1.,1.); pre.setTubeSet(TubeSet::make(0)); pa.setTubeSet(TubeSet::make(0));
  pre.setControls(.6,.5,.5,.6,.3);pa.setControls(.5,.5);pre.prepare(fs);pa.prepare(fs);
  auto settle=[&](double sec){for(int i=0;i<int(fs*sec);++i)processAmp5150(pre,pa,0.,1.);};
  pa.beginSupplySettle(); for(int p=0;p<3;++p){settle(.1);pa.resetSupplyAverage();settle(.1);pa.applySupplyDC();}settle(.15);pa.endSupplySettle();
  pre.setModeNow(Preamp5150::Red);
  for(int i=0;i<int(fs*.22);++i){
   double t=i/fs; double x=0.28*std::sin(2*pi*82.4*t)*std::exp(-25*std::fmod(t,.055))+.17*std::sin(2*pi*137.2*t);
   if(i==int(.035*fs))pre.setMode(Preamp5150::Blue);
   if(i==int(.043*fs))pre.setCrunch(false);
   if(i==int(.050*fs))pre.setMode(Preamp5150::Clean);
   if(i==int(.065*fs))pre.setMode(Preamp5150::Red);
   if(i==int(.10*fs))pre.setControls(.9,.2,.7,.4,.75);
   if(i==int(.14*fs))pre.setBright(false);
   if(i==int(.16*fs))pre.setMode(Preamp5150::Blue);
   if(t>.18)x=0;
   const double y=processAmp5150(pre,pa,x,1.);
   double i1,i2;pre.railCurrents(i1,i2);
   double record[]={y,pre.getLastPreampOut(),pa.getRailV1(),pa.getRailV2(),i1,i2};
   for(double v:record)if(!std::isfinite(v))return 3;
   if(std::fwrite(record,sizeof(double),6,f)!=6)return 4;
  }
  std::fprintf(stderr,"%.0f Hz %d records\n",fs,int(fs*.22));
 }
 return std::fclose(f)!=0;
}
