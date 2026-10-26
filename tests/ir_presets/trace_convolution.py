"""Instrument a BUILD-LOCAL copy of JUCE 8.0.4, never the dependency checkout.
Records ownership order at the actual JUCE API; does not replace DSP processing.
"""
import pathlib,sys,re
src,out=map(pathlib.Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
def write_changed(path, content):
    if not path.exists() or path.read_text() != content:
        path.write_text(content)
conv=(src/'frequency/juce_Convolution.cpp').read_text()
conv=conv.replace('namespace juce::dsp\n{','extern void irTestConvolutionCall(const void*, int);\nnamespace juce::dsp\n{',1)
conv,n=re.subn(r'(void Convolution::loadImpulseResponse \(.*?\n\{)',r'\1\n    ::irTestConvolutionCall(this, 1);',conv,flags=re.S)
assert n==3,n
conv=conv.replace('Convolution::~Convolution() noexcept = default;', 'Convolution::~Convolution() noexcept { ::irTestConvolutionCall(this, 2); }')
conv,n=re.subn(r'(void Convolution::processSamples \(.*?\n\{)',r'\1\n    ::irTestConvolutionCall(this, 0);',conv,flags=re.S)
assert n==1,n
write_changed(out/'traced_Convolution.cpp', conv)
dsp=(src/'juce_dsp.cpp').read_text().replace('#include "frequency/juce_Convolution.cpp"','#include "traced_Convolution.cpp"')
assert 'traced_Convolution.cpp' in dsp
write_changed(out/'traced_dsp.cpp', dsp)
