#include "dsp/AmpSim.h"
#include "../cpu_quality/Reference16.h"
#include <iostream>
#include <vector>
#include <numeric>
void irTestCaptureCabinetInput(const float*,int) {}
void irTestConvolutionCall(const void*,int) {}

template<class Amp> static bool measure (double fs,int factor,const char* quality)
{
    Amp amp;
    if constexpr (std::is_same_v<Amp,vamp::AmpSim>) amp.prepare(fs,128,factor);
    else amp.prepare(fs,128);
    typename Amp::Settings settings;
    settings.channel=2;settings.gain=.9f;settings.master=.3f;
    settings.boostOn=true;settings.boostDrive=.8f;settings.boostLevel=.7f;
    constexpr int count=8192;
    juce::dsp::FFT fft(13);
    bool valid=true;
    for(double hz : {1000.,3000.,5000.,9000.}) for(double level : {.03,.12})
    {
        const int bin=((int)std::round(hz*count/fs))|1;
        const int warm=(int)std::ceil(.3*fs/128)*128;
        std::vector<float> data(2*count,0.f);float block[128];int offset=0;
        double dc=0,energy=0,peak=0;
        for(int k=0;k<warm+count;k+=128)
        {
            for(int i=0;i<128;++i) block[i]=(float)(level*std::sin(2*vamp::kPi*bin*(k+i)/count));
            amp.process(block,128,settings);
            if(k>=warm) for(int i=0;i<128;++i)
            {
                const double value=block[i];energy+=value*value;dc+=value;peak=std::max(peak,std::abs(value));
                data[offset]=(float)(value*(.5-.5*std::cos(2*vamp::kPi*offset/(count-1))));++offset;
            }
        }
        fft.performFrequencyOnlyForwardTransform(data.data());
        std::vector<bool> harmonic(count/2),folded(count/2);
        for(int h=1;h*bin<count/2;++h) for(int delta=-4;delta<=4;++delta)
            if(h*bin+delta>0&&h*bin+delta<count/2) harmonic[(size_t)(h*bin+delta)]=true;
        for(int h=count/(2*bin)+1;h<=256;++h)
        {
            int wrapped=(h*bin)%count;if(wrapped>count/2) wrapped=count-wrapped;
            for(int delta=-1;delta<=1;++delta)
                if(wrapped+delta>0&&wrapped+delta<count/2&&!harmonic[(size_t)(wrapped+delta)]) folded[(size_t)(wrapped+delta)]=true;
        }
        double fundamental=0,aliases=0,other=0,total=0;int bins=0;
        for(int k=1;k<count/2&&k*fs/count<=20000.;++k)
        {
            const double power=(double)data[k]*data[k];total+=power;
            if(std::abs(k-bin)<=1) fundamental+=power;
            if(folded[(size_t)k]) {aliases+=power;++bins;}
            if(!harmonic[(size_t)k]) other+=power;
        }
        auto db=[](double v){return 10*std::log10(std::max(v,1e-30));};
        const bool finite=std::isfinite(energy)&&std::isfinite(dc)&&energy>1e-8&&fundamental>1e-10;
        valid=valid&&finite;
        std::cout<<"ALIAS_DATA fs="<<fs<<" quality="<<quality<<" factor="<<factor<<" requested_hz="<<hz<<" actual_hz="<<bin*fs/count
                 <<" input_peak_dbfs="<<20*std::log10(level)<<" folded_bins="<<bins<<" folded_bins_db="<<db(aliases/std::max(fundamental,1e-30))
                 <<" inharmonic_db="<<db(other/std::max(total,1e-30))<<" rms="<<std::sqrt(energy/count)<<" dc="<<dc/count<<" peak="<<peak
                 <<" data_status="<<(finite?"PASS":"FAIL")<<std::endl;
    }
    return valid;
}
int main()
{
    bool valid=true;
    for(double fs : {44100.,48000.})
    {
        for(int q : {2,1,0}) valid=measure<vamp::AmpSim>(fs,1<<(q+1),q==2?"HIGH":q==1?"STANDARD":"ECO")&&valid;
        valid=measure<vamp_before::Reference16>(fs,16,"REF16")&&valid;
    }
    std::cout<<(valid?"PASS":"FAIL")<<" 64 finite non-silent spectral measurements; sonic acceptance NOT RUN"<<std::endl;
    return valid?0:1;
}
