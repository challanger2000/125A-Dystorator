#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
namespace vamp {
// Experimental block-semiseparable Newton solver, NOT production.
template<int M> struct RankedPortPrototype {
 double u[M]{},v[M]{},x[M]{},y[M]{};
 bool valid=false;
 void prepare(const double* K) {
  valid=false;
  int a=0,b=M-2,c=M-2,e=0;
  for(int i=0;i<2;++i)for(int j=M-2;j<M;++j){
   if(std::abs(K[i*M+j])>std::abs(K[a*M+b])){a=i;b=j;}
   if(std::abs(K[j*M+i])>std::abs(K[c*M+e])){c=j;e=i;}
  }
  if(!std::isfinite(K[a*M+b])||!std::isfinite(K[c*M+e])||K[a*M+b]==0||K[c*M+e]==0)return;
  for(int i=0;i<M;++i){u[i]=K[i*M+b];v[i]=K[a*M+i]/K[a*M+b];x[i]=K[i*M+e];y[i]=K[c*M+i]/K[c*M+e];}
  // Unused terminal factors set to zero, avoiding spurious terms in first/last block.
  for(int i=0;i<2;++i){x[i]=v[i]=0;u[M-2+i]=y[M-2+i]=0;}
  for(int i=0;i<M;++i)for(int j=0;j<M;++j)if(i/2!=j/2){
   double actual=K[i*M+j],approx=i<j?u[i]*v[j]:x[i]*y[j];
   if(!std::isfinite(approx)||!std::isfinite(actual)||std::abs(actual-approx)>64*std::numeric_limits<double>::epsilon()*std::abs(actual))return;
  }
  valid=true;
 }
 bool solve(const double* K,const double (&d)[M][2],const double (&r)[M],double (&dp)[M]) const {
  if(!valid)return false;
  double S=0,T=0, w[M], z[M],q[M];
  for(int k=0;k<M;k+=2){
   const double d0=d[k][0],d1=d[k][1],d2=d[k+1][1];
   const double X0=-(d0*x[k]+d1*x[k+1]),X1=-d2*x[k+1];
   const double U0=-(d0*u[k]+d1*u[k+1])-X0*S,U1=-d2*u[k+1]-X1*S;
   const double Y0=y[k]-S*v[k],Y1=y[k+1]-S*v[k+1];
   const double a=1-(d0*K[k*M+k]+d1*K[(k+1)*M+k])-X0*S*v[k];
   const double b=-(d0*K[k*M+k+1]+d1*K[(k+1)*M+k+1])-X0*S*v[k+1];
   const double c=-d2*K[(k+1)*M+k]-X1*S*v[k];
   const double e=1-d2*K[(k+1)*M+k+1]-X1*S*v[k+1];
   const double f=d0*r[k]+d1*r[k+1]-X0*T,g=d2*r[k+1]-X1*T;
   const double ae=a*e,bc=b*c,det=ae-bc;
   if(!std::isfinite(det)||std::abs(det)<=1e-10*std::max(1.,std::abs(ae)+std::abs(bc)))return false;
   const double inv=1/det;
   w[k]=(e*f-b*g)*inv;w[k+1]=(a*g-c*f)*inv;
   z[k]=(e*U0-b*U1)*inv;z[k+1]=(a*U1-c*U0)*inv;
   S+=Y0*z[k]+Y1*z[k+1];T+=Y0*w[k]+Y1*w[k+1];
  }
  double V=0;
  for(int k=M-2;k>=0;k-=2){q[k]=w[k]-z[k]*V;q[k+1]=w[k+1]-z[k+1]*V;V+=v[k]*q[k]+v[k+1]*q[k+1];}
  // Reconstruct voltages using the original full matrix, then validate the
  // original current/voltage equation. Nothing accepted on failed guard.
  for(int k=0;k<M;++k){double s=r[k];for(int j=0;j<M;++j)s+=K[k*M+j]*q[j];dp[k]=s;}
  for(int k=0;k<M;k+=2){double a=d[k][0]*dp[k],b=d[k][1]*dp[k+1],c=d[k+1][1]*dp[k+1];
   if(!std::isfinite(dp[k])||!std::isfinite(dp[k+1])||!std::isfinite(q[k])||!std::isfinite(q[k+1])||
    std::abs(q[k]-a-b)>1e-12*(std::abs(q[k])+std::abs(a)+std::abs(b))+1e-22||
    std::abs(q[k+1]-c)>1e-12*(std::abs(q[k+1])+std::abs(c))+1e-22)return false;
  }
  return true;
 }
};
}
