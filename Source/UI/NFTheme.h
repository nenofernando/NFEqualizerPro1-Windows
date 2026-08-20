#pragma once
#include <JuceHeader.h>

// A "skin" is just a bundle of colours. The knob body stays purple in every
// skin (that's the fixed brand element); what changes is the main face
// colour and which colour ("purple" or "green") plays the accent role for
// SHELF fills, section LEDs, and outlined button borders.
struct NFTheme
{
    juce::String id;
    juce::String displayName;

    juce::Colour faceBright, faceBase, faceDark, faceOutline;
    juce::Colour accentFill;         // resting fill for SHELF / stepper arrows
    juce::Colour accentBorder;       // outlined-button border, lit LED colour
    juce::Colour accentBorderBright; // hover/highlight variant
    juce::Colour onFaceText;         // text drawn directly on the main face

    // A thin contour drawn behind onFaceText before the fill. White text on
    // the medium-bright purple face reads as "blurry" simply from lower
    // luminance contrast than black-on-lime-green; a dark halo fixes that
    // without darkening the face itself. Zero width = no halo (green skin
    // doesn't need one — black-on-bright-green already has full contrast).
    juce::Colour haloColour = juce::Colours::transparentBlack;
    float haloWidth = 0.0f;

    static NFTheme classicGreen()
    {
        NFTheme t;
        t.id = "green";
        t.displayName = "Classic Green";
        t.faceBright = juce::Colour(0xffD5FF38);
        t.faceBase = juce::Colour(0xffAAFF00);
        t.faceDark = juce::Colour(0xff5F9100);
        t.faceOutline = juce::Colour(0xffD5FF38);
        t.accentFill = juce::Colour(0xff4A1268);
        t.accentBorder = juce::Colour(0xff8217B8);
        t.accentBorderBright = juce::Colour(0xffB52DE8);
        t.onFaceText = juce::Colours::black;
        t.haloWidth = 0.0f;
        return t;
    }

    static NFTheme purpleNight()
    {
        NFTheme t;
        t.id = "purple";
        t.displayName = "Purple Night";
        t.faceBright = juce::Colour(0xffC77DFF);
        t.faceBase = juce::Colour(0xffA855F7);
        t.faceDark = juce::Colour(0xff6B21A8);
        t.faceOutline = juce::Colour(0xffE0AAFF);
        t.accentFill = juce::Colour(0xff2F5A0A);
        t.accentBorder = juce::Colour(0xff7ED321);
        t.accentBorderBright = juce::Colour(0xffAAFF00);
        t.onFaceText = juce::Colours::white;
        t.haloColour = juce::Colours::black;
        t.haloWidth = 1.2f;
        return t;
    }

    static NFTheme byIndex(int index)
    {
        return index == 1 ? purpleNight() : classicGreen();
    }
};
