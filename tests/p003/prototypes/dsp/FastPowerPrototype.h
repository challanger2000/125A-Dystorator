#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
namespace vamp {
// Experimental positive binary64 fractional power, degree-five local expansion.
template<int P> struct FastPowerPrototype {
 static constexpr double a = P==0 ? 1.4-1.0 : 1.35;
 static constexpr int N=256;
 double mant[N], inv[N], exp2[201];
 FastPowerPrototype(){
  for(int i=0;i<N;++i){double m=1.+(i+.5)/N;mant[i]=std::pow(m,a);inv[i]=1./m;}
  for(int i=0;i<201;++i)exp2[i]=std::pow(std::ldexp(1.,i-100),a);
 }
 inline double eval(double x) const {
  std::uint64_t bits;std::memcpy(&bits,&x,8);
  unsigned e=(unsigned)((bits>>52)&2047);
  // Keep extreme exponent, subnormal, invalid and negative handling exact.
  if(e<923||e>1123||(bits>>63))return std::exp(a*std::log(x));
  unsigned idx=(unsigned)((bits>>44)&255);
  bits=(bits&UINT64_C(0x000fffffffffffff))|UINT64_C(0x3ff0000000000000);
  double m;std::memcpy(&m,&bits,8);
  double t=(m-(1.+(idx+.5)/N))*inv[idx];
  constexpr double c2=a*(a-1)/2,c3=c2*(a-2)/3,c4=c3*(a-3)/4,c5=c4*(a-4)/5;
  return (mant[idx]*exp2[e-923])*(1.+t*(a+t*(c2+t*(c3+t*(c4+t*c5)))));
 }
};
template<int P> inline double fastPowerPrototype(double x){static const FastPowerPrototype<P> table;return table.eval(x);}
}
