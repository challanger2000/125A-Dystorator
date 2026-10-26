#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <mutex>
#include <set>
static std::mutex traceMutex;
static std::set<const void*> renderedConvolvers;
static int ownershipViolations=0;
void irTestConvolutionCall(const void* object,int event){
 std::lock_guard<std::mutex> lock(traceMutex);
 if(event==0)renderedConvolvers.insert(object);
 if(event==1 && renderedConvolvers.count(object))++ownershipViolations;
 if(event==2)renderedConvolvers.erase(object);
}


static int checks=0, failures=0;
static void check(bool ok,const char* text){++checks; if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;}
static void param(VibeAmpProcessor& p,const char* id,float v){auto* x=p.apvts.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(v));}
static juce::File wav(const juce::File& folder,const char* name,bool inverted=false){
 auto f=folder.getChildFile(name);juce::WavAudioFormat format;
 auto out=f.createOutputStream(); auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(out.release(),48000,1,32,{},0));
 if(!writer)throw std::runtime_error("fixture writer");
 juce::AudioBuffer<float> data(1,256);data.clear();data.setSample(0,0,inverted?-1.f:1.f);data.setSample(0,24,inverted?-.5f:.5f);
 if(!writer->writeFromAudioSampleBuffer(data,0,256))throw std::runtime_error("fixture write");return f;
}
static void block(VibeAmpProcessor& p){juce::AudioBuffer<float> b(2,64);b.clear();for(int i=0;i<64;++i)b.setSample(0,i,.05f*std::sin(i*.15f));juce::MidiBuffer m;p.processBlock(b,m);for(int c=0;c<2;++c)for(int i=0;i<64;++i)if(!std::isfinite(b.getSample(c,i)))throw std::runtime_error("nonfinite audio");}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("125a-ir-tests","",false);dir.createDirectory();
 try{
  const auto a=wav(dir,"a.wav"),b=wav(dir,"b.wav",true),bad=dir.getChildFile("bad.wav"),missing=dir.getChildFile("missing.wav");bad.replaceWithText("not an audio file");
  VibeAmpProcessor p(dir);p.prepareToPlay(48000,64);param(p,ids::channel,0);param(p,ids::gateOn,0);
  p.loadIR(a);block(p);p.presets.load(0);check(p.getIRPath().isEmpty(),"A04 factory restores built-in");
  p.loadIR(a);param(p,ids::ampGain,2);const auto previous=p.apvts.copyState().toXmlString();const auto name=p.presets.getCurrentName();
  auto wrong=dir.getChildFile("foreign.xml");wrong.replaceWithText("<unrelated/>");p.presets.refresh();p.presets.load(p.presets.indexOf("foreign"));
  check(p.apvts.copyState().toXmlString()==previous && p.presets.getCurrentName()==name,"B03 foreign XML rejects without parameter/name mutation");
  p.loadIR(a);p.loadIR(bad);check(p.getIRPath()==a.getFullPathName(),"B04 invalid WAV preserves accepted IR path");
  p.loadIR(a);p.loadIR(missing);check(p.getIRPath()==a.getFullPathName(),"missing IR preserves accepted IR path");
  p.loadIR(a);p.presets.save("with-ir");p.loadIR(b);p.presets.load(p.presets.indexOf("with-ir"));check(p.getIRPath()==a.getFullPathName(),"preset save/load restores IR A");
  p.clearIR();p.presets.save("builtin");p.loadIR(a);p.presets.load(p.presets.indexOf("builtin"));check(p.getIRPath().isEmpty(),"A04 user built-in preset clears IR A");
  p.loadIR(a);juce::MemoryBlock state;p.getStateInformation(state);p.loadIR(b);p.setStateInformation(state.getData(),(int)state.getSize());check(p.getIRPath()==a.getFullPathName(),"host state restores IR A");
  std::atomic<bool> done{false};std::thread audio([&]{for(int i=0;i<1000;++i)block(p);done=true;});
  for(int i=0;i<20;++i){p.loadIR(i%2?a:b);if(i%3==0)p.clearIR();}audio.join();check(done,"concurrent load/process completes with finite audio");
#ifdef IR_TRACE_CONVOLUTION
  check(ownershipViolations==0,"B01 no load command touches a published/rendered convolver");
#endif
 }catch(const std::exception& e){check(false,e.what());}
 dir.deleteRecursively();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
