#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <iostream>

int main (int, char**)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    NFWhiteDelayAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "No editor\n";
        return 1;
    }

    // Alta resolução: abre no canvas nativo (1280x860, scale 1.0 -- sem
    // downscale do "open size" menor) e renderiza em 2x supersampling.
    editor->setSize (1280, 860);
    const int w = editor->getWidth();
    const int h = editor->getHeight();
    constexpr int superSample = 2;
    juce::Image img (juce::Image::ARGB, w * superSample, h * superSample, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) superSample));
        editor->paintEntireComponent (g, true);
    }

    const auto outDir = juce::File ("/Users/nenofernando/Desktop/NF_Equalizer_JUCE_V2/NF_White_Delay/dist");
    outDir.createDirectory();
    const auto out = outDir.getChildFile ("prototype_chassis_time_sync_display.png");
    out.deleteFile();

    {
        juce::FileOutputStream stream (out);
        if (! stream.openedOk())
        {
            std::cerr << "Cannot write " << out.getFullPathName() << "\n";
            return 2;
        }

        juce::PNGImageFormat png;
        if (! png.writeImageToStream (img, stream))
        {
            std::cerr << "PNG write failed\n";
            return 3;
        }
        stream.flush();
    }

    editor->removeFromDesktop();
    std::cout << out.getFullPathName() << "\n";
    return 0;
}
