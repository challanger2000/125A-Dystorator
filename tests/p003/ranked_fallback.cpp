#include <cstdio>
#include <cmath>
#include <limits>
#include "dsp/PortSolve.h"
#include "dsp/RankedPortPrototype.h"
int main(){
 constexpr int M=12;
 double K[M*M]{}, d[M][2]{}, r[M]{}, a[M]{}, b[M]{};
 for(int i=0;i<M;++i){r[i]=.1*(i+1);d[i][0]=i%2?0.:.1;for(int j=0;j<M;++j)K[i*M+j]=i/2!=j/2?.125:0.;}
 K[0]=1.;d[0][0]=1.; // singular first diagonal block, full system is nonsingular
 vamp::RankedPortPrototype<M> solver;solver.prepare(K);
 if(!solver.valid||solver.solve(K,d,r,b)||!vamp::solvePortStep<M>(K,d,r,a))return 1;
 double maxResidual=0.;for(int i=0;i<M;++i){double predicted=r[i];for(int j=0;j<M;++j)predicted+=K[i*M+j]*d[j][0]*a[j];if(!std::isfinite(a[i]))return 2;maxResidual=std::max(maxResidual,std::abs(a[i]-predicted));}
 if(maxResidual>1e-12)return 3;
 K[0]=0.;solver.prepare(K);if(!solver.valid||!solver.solve(K,d,r,b))return 4;
 for(double invalid:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  double saved=r[0];r[0]=invalid;if(solver.solve(K,d,r,b))return 5;r[0]=saved;
  saved=d[0][0];d[0][0]=invalid;if(solver.solve(K,d,r,b))return 6;d[0][0]=saved;
  saved=K[0];K[0]=invalid;solver.prepare(K);if(solver.solve(K,d,r,b))return 7;K[0]=saved;solver.prepare(K);
 }
 std::printf("PASS local singular pivot -> dense fallback, full residual %.17g; 6 invalid-input rejection cases\n",maxResidual);return 0;
}
