#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>
int main()
{
    NFResonanceAudioProcessor proc;
    auto& apvts = proc.state();
    printf("lowHz default = %.3f\n", apvts.getRawParameterValue("lowHz")->load());
    printf("highHz default = %.3f\n", apvts.getRawParameterValue("highHz")->load());
    for (int i = 0; i < 3; ++i)
        printf("band_active_%d = %.0f\n", i, apvts.getRawParameterValue("band_active_" + juce::String(i))->load());
    return 0;
}
