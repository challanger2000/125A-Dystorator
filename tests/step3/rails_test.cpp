#include "dsp/Preamp5150.h"
#include "baseline/Preamp5150.h"
#include <iostream>
int main()
{
    vamp::Preamp5150 after;
    vamp_step3_before::Preamp5150 before;
    before.vb1=after.vb1=311.;before.vb2=after.vb2=326.;
    before.prepare(96000);after.prepare(96000);
    double maximum=0,energy=0;
    bool finite=true;
    for(int i=0;i<24000;++i)
    {
        if(i%3000==0)
        {
            const int channel=(i/3000)%3;
            before.setMode(channel);after.setMode(channel);
            const bool crunch=(i/3000)%2==0;
            before.setCrunch(crunch);after.setCrunch(crunch);
        }
        const double v1=300.+8.*std::sin(i*.0017),v2=320.+7.*std::cos(i*.0021);
        before.setRails(v1,v2);after.setRails(v1,v2);
        const double input=.15*std::sin(i*.017)+.03*std::sin(i*.173);
        const double a=before.process(input),b=after.process(input);
        double a1,a2,b1,b2;before.railCurrents(a1,a2);after.railCurrents(b1,b2);
        for (double value : {a,b,a1,a2,b1,b2}) finite=finite&&std::isfinite(value);
        maximum=std::max({maximum,std::abs(a-b),std::abs(a1-b1),std::abs(a2-b2)});
        energy+=a*a;
    }
    const bool ok=finite&&maximum==0.&&energy>1e-8;
    std::cout<<(ok?"PASS":"FAIL")<<" per-sample rail source scheduling matches frozen 398204e, channel/crunch fades/custom initial rails max="<<maximum<<" energy="<<energy<<'\n';
    return ok?0:1;
}
