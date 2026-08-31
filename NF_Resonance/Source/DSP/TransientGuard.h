#pragma once
#include <JuceHeader.h>
struct TransientGuard { float env=0, slow=0; void reset(){env=slow=0;} float process(float x,float amount){ float a=std::abs(x); env=juce::jmax(a,env*0.93f); slow=0.995f*slow+0.005f*a; float t=juce::jlimit(0.0f,1.0f,(env-slow)*18.0f); return 1.0f-t*juce::jlimit(0.0f,1.0f,amount/10.0f)*0.85f; } };