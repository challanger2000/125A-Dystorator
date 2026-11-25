#include <cstdio>
#include <cmath>
#include <random>
#include <limits>
#include <algorithm>
#include "dsp/FastPowerPrototype.h"
template<int P> bool test(){vamp::FastPowerPrototype<P> tab;std::mt19937_64 rng(125);std::uniform_real_distribution<double> mant(1.,2.);double maxRel=0,maxOld=0;for(int i=0;i<500000;++i){int e=(i%201)-100;double x=std::ldexp(mant(rng),e);double y=tab.eval(x),ref=std::pow(x,tab.a),old=std::exp(tab.a*std::log(x));double er=std::abs(y/ref-1);maxRel=std::max(maxRel,er);maxOld=std::max(maxOld,std::abs(y/old-1));if(!std::isfinite(y)||er>2e-14)return false;} for(double x:{0.,-1.,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){double y=tab.eval(x),r=std::exp(tab.a*std::log(x));if(!(y==r||(std::isnan(y)&&std::isnan(r))))return false;}printf("P=%d max_rel_pow=%.17g max_rel_original=%.17g\n",P,maxRel,maxOld);return true;}
int main(){return test<0>()&&test<1>()?0:1;}
