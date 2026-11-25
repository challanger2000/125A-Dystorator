#include <cmath>
#include <cstdio>
#include <cfenv>
#include "dsp/PowerAmp5150.h"
int main(){
 vamp::KorenPentode tube;
 volatile double screens[]={-485.,-1.,0.,1.,100.,485.};
 int failures=0;
 for(bool exact:{false,true}){
  tube.exact=exact;
  for(double screen:screens){
   std::feclearexcept(FE_ALL_EXCEPT);
   auto s=vamp::KorenPentode::screenTerms(screen);
   double ip,dv,dg;tube.plate(250.,0.,s,ip,dv,dg);
   double sc=tube.screen(250.,0.,screen);
   bool finite=std::isfinite(s.inv)&&std::isfinite(s.scale)&&std::isfinite(ip)&&std::isfinite(dv)&&std::isfinite(dg)&&std::isfinite(sc);
   int faults=std::fetestexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW);
   if(!finite||faults||(screen<=0.&&(s.inv!=0.||s.scale!=0.||ip!=0.||dv!=0.||dg!=0.||sc!=0.))||(screen>0.&&!(ip>0.&&dv>0.&&dg>0.&&sc>0.))){std::printf("FAIL exact=%d screen=%g inv=%g ip=%g flags=%d\n",exact,screen,s.inv,ip,faults);++failures;}
  }
 }
 std::printf("%d/12 cases passed\n",12-failures);return failures?1:0;
}
