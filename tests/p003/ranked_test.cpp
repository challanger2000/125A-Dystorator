#include <random>
#include <cstdio>
#include <cstdlib>
#include "dsp/PortSolve.h"
#include "dsp/RankedPortPrototype.h"
int main(){constexpr int M=12;std::mt19937_64 rng(125);std::uniform_real_distribution<double> U(-1,1);int accept=0;double maxerr=0;for(int trial=0;trial<10000;++trial){double K[M*M],d[M][2],r[M],a[M],b[M],u[M],v[M],x[M],y[M];for(int i=0;i<M;++i){u[i]=U(rng);v[i]=U(rng);x[i]=U(rng);y[i]=U(rng);r[i]=U(rng);d[i][0]=i%2?0:(trial%7==0?0:.2*U(rng));d[i][1]=trial%5==0?0:.2*U(rng);}for(int i=0;i<M;++i)for(int j=0;j<M;++j)K[i*M+j]=i/2==j/2?.2*U(rng):i<j?u[i]*v[j]:x[i]*y[j];vamp::RankedPortPrototype<M> solver;solver.prepare(K);if(!solver.valid||!vamp::solvePortStep<M>(K,d,r,a))return 1;if(solver.solve(K,d,r,b)){++accept;for(int i=0;i<M;++i){maxerr=std::max(maxerr,std::abs(a[i]-b[i]));if(!std::isfinite(b[i])||std::abs(a[i]-b[i])>1e-10*(1+std::abs(a[i])))return 2;}}K[2*M+9]+=1.;solver.prepare(K);if(solver.valid||solver.solve(K,d,r,b))return 3;}
printf("accepted=%d/10000 maxerr=%.17g rejected structural mutations=10000/10000\n",accept,maxerr);return accept<9900?4:0;}
