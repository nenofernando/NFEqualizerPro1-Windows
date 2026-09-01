#include "NFLookAndFeel.h"
// Neon Blue palette (0.1c): BACKGROUND #090D13, PANEL #101722, SECONDARY PANEL
// #151E2A, NEON BLUE #00AFFF, BRIGHT CYAN #27D8FF, DEEP BLUE #0066FF,
// TEXT #EAF4FF, DIM TEXT #708090. Same knob/button geometry as 0.1b -- only
// the colour constants changed.
NFLookAndFeel::NFLookAndFeel(){setColour(juce::Slider::textBoxTextColourId,juce::Colours::white);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff151e2a));setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);}
void NFLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float a0,float a1,juce::Slider&){
 // 0.1f: force a SQUARE drawing bounds (diameter = min(w,h), centred in the
 // component) before anything else, so every rotary stays geometrically
 // circular at any window size/aspect the editor is resized to -- the
 // caller's (possibly non-square) w/h is never used directly for the ellipse.
 float diameter=juce::jmin((float)w,(float)h);
 auto square=juce::Rectangle<float>((float)x+((float)w-diameter)*0.5f,(float)y+((float)h-diameter)*0.5f,diameter,diameter);
 auto r=square.reduced(juce::jmax(4.0f,diameter*0.07f));float rad=juce::jmin(r.getWidth(),r.getHeight())*.5f,ang=a0+pos*(a1-a0);auto c=r.getCentre();g.setColour(juce::Colour(0xff090d13));g.fillEllipse(r);g.setColour(juce::Colour(0xff283542));g.drawEllipse(r,2);juce::Path arc;arc.addCentredArc(c.x,c.y,rad-4,rad-4,0,a0,ang,true);g.setColour(juce::Colour(0xff00afff));g.strokePath(arc,juce::PathStrokeType(5,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));juce::Path p;p.addRoundedRectangle(-1.5f,-rad*.68f,3,rad*.42f,1.5f);p.applyTransform(juce::AffineTransform::rotation(ang).translated(c.x,c.y));g.setColour(juce::Colours::white);g.fillPath(p);}
void NFLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down){auto r=b.getLocalBounds().toFloat().reduced(1);g.setColour(down?juce::Colour(0xff0077cc):(over?juce::Colour(0xff17202b):juce::Colour(0xff101722)));g.fillRoundedRectangle(r,5);g.setColour(juce::Colour(0xff2a3542));g.drawRoundedRectangle(r,5,1);}
// 0.1g: MODE/QUALITY dropdown text was reported too small -- bump it a bit
// beyond LookAndFeel_V4's default (~0.85 * box height) without letting it
// grow unbounded during resize.
juce::Font NFLookAndFeel::getComboBoxFont(juce::ComboBox& box){ return juce::Font(juce::jlimit(12.5f,16.0f,(float)box.getHeight()*0.48f)); }
