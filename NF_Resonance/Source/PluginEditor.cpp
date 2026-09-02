#include "PluginEditor.h"
// 0.1f: resizable editor. Base design canvas is the original 1180x650 layout
// -- resized() scales every control proportionally from these same
// reference coordinates, so at 100% window size the layout is pixel-
// identical to 0.1e's fixed layout (with the RELEASE/HIGH overlap already
// fixed and the curve panel folded into the analyzer).
static constexpr int kBaseW = 1180, kBaseH = 650;
NFResonanceAudioProcessorEditor::NFResonanceAudioProcessorEditor(NFResonanceAudioProcessor& pr):AudioProcessorEditor(pr),p(pr),spectrum(pr.engine()),curve(pr.state()){
 setLookAndFeel(&lf);
 setup(depth,"depth","DEPTH");setup(sharp,"sharpness","SHARPNESS");setup(detail,"detail","DETAIL");setup(select,"selectivity","SELECTIVITY");setup(attack,"attack","ATTACK");setup(release,"release","RELEASE");setup(output,"output","OUTPUT");setup(mix,"mix","MIX");setup(low,"lowHz","LOW");setup(high,"highHz","HIGH");setup(transient,"transient","TRANSIENT");setup(maxRed,"maxReductionDb","MAX RED");
 addAndMakeVisible(maxRedEnabled);mrea=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state(),"maxReductionEnabled",maxRedEnabled);
 presetManager=std::make_unique<PresetManager>(p.state());
 presetButton.setButtonText("Default v");presetButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff141a24));presetButton.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff00afff));presetButton.onClick=[this]{showPresetMenu();};addAndMakeVisible(presetButton);
 { juce::Path ham; float w=18.0f,h=13.0f,lh=2.2f; ham.addRoundedRectangle(0.0f,0.0f,w,lh,lh*0.5f); ham.addRoundedRectangle(0.0f,(h-lh)*0.5f,w,lh,lh*0.5f); ham.addRoundedRectangle(0.0f,h-lh,w,lh,lh*0.5f); menuButton.setShape(ham,false,true,false); }
 menuButton.onClick=[this]{showMainMenu();};addAndMakeVisible(menuButton);
 addAndMakeVisible(spectrum);addAndMakeVisible(curve); // curve added AFTER spectrum: paints on top, as an overlay
 addAndMakeVisible(rangeLight);
 mode.addItemList({"Stereo","L/R","Mid/Side"},1);detect.addItemList({"Internal","Sidechain"},1);addAndMakeVisible(mode);addAndMakeVisible(detect);addAndMakeVisible(delta);addAndMakeVisible(bypass);addAndMakeVisible(fft);
 ma=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state(),"mode",mode);deta=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state(),"detectorSource",detect);da=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state(),"delta",delta);ba=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state(),"bypass",bypass);fa=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state(),"showOriginalFft",fft);
 spectrum.setShowOriginalFftParam(p.state().getRawParameterValue("showOriginalFft")); // 0.1r
 setResizable(true,true);
 setResizeLimits((int)(kBaseW*0.66),(int)(kBaseH*0.66),(int)(kBaseW*1.55),(int)(kBaseH*1.55));
 if (auto* c=getConstrainer()) c->setFixedAspectRatio((double)kBaseW/(double)kBaseH);
 setSize(kBaseW,kBaseH);
 // lowEnabled/highEnabled are changed ONLY by double-clicking the analyzer
 // handle (see ControlCurveComponent::mouseDoubleClick) -- turning the knob
 // here only ever edits lowHz/highHz, on/off state is untouched either way.
 startTimerHz(15); // dims the LOW/HIGH knobs when their side is OFF -- cheap polling avoids a Listener subclass just for two flags
}
void NFResonanceAudioProcessorEditor::timerCallback(){
 bool lowOn=p.state().getRawParameterValue("lowEnabled")->load()>0.5f, highOn=p.state().getRawParameterValue("highEnabled")->load()>0.5f;
 low.setAlpha(lowOn?1.0f:0.45f); high.setAlpha(highOn?1.0f:0.45f);
 bool maxRedOn=p.state().getRawParameterValue("maxReductionEnabled")->load()>0.5f;
 maxRed.setAlpha(maxRedOn?1.0f:0.45f);
 rangeLight.repaint(); // reflects lowEnabled/highEnabled -- same 15Hz poll, no separate Timer
}
NFResonanceAudioProcessorEditor::~NFResonanceAudioProcessorEditor(){setLookAndFeel(nullptr);}void NFResonanceAudioProcessorEditor::setup(juce::Slider&s,const char*id,const char*){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,68,18);addAndMakeVisible(s);sa.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state(),id,s));}
void NFResonanceAudioProcessorEditor::paint(juce::Graphics&g){g.fillAll(juce::Colour(0xff090d13));g.setColour(juce::Colour(0xffeaf4ff));g.setFont(30);g.drawText("NF",20,12,48,36,juce::Justification::centredLeft);g.setColour(juce::Colour(0xff00afff));g.drawText("Resonance",68,12,190,36,juce::Justification::centredLeft);
 // Subtitle: same font/weight/colour/vertical position as before, just
 // ~13.6% larger (11->12.5, inside the requested 10-15% range) and
 // horizontally re-centred under the FULL "NF Resonance" title's own
 // rendered width (not just left-edge aligned as it was) -- computed from
 // each word's actual glyph width at the title's own font size, since
 // "Resonance"'s drawText box (190px) is wider than its real rendered
 // text. "NF"/"Resonance" themselves (size, colour, position, gap) are
 // completely untouched above.
 {
     const juce::Font titleFont{juce::FontOptions(30.0f)};
     const float nfX = 20.0f, resonanceX = 68.0f;
     const float titleLeft = nfX;
     const float titleRight = resonanceX + juce::GlyphArrangement::getStringWidth(titleFont, "Resonance");
     const float titleCenterX = (titleLeft + titleRight) * 0.5f;
     const float subtitleFontSize = 12.5f;
     const juce::String subtitleText("DYNAMIC RESONANCE SUPPRESSOR");
     const juce::Font subtitleFont{juce::FontOptions(subtitleFontSize)};
     const float subtitleW = juce::GlyphArrangement::getStringWidth(subtitleFont, subtitleText);
     g.setColour(juce::Colour(0xff708090));
     g.setFont(subtitleFontSize);
     g.drawText(subtitleText, (int) std::round(titleCenterX - subtitleW * 0.5f), 45, (int) std::round(subtitleW) + 1, 18, juce::Justification::centred);
 }
 // 0.1g typography/spacing polish: captions now sit in a real label ROW
 // ABOVE each knob (own dedicated vertical space carved out in resized()),
 // not overlaid on the knob face -- with a 3-tier hierarchy: DEPTH/MIX/OUTPUT
 // (principal) largest+brightest, SHARPNESS/SELECTIVITY/ATTACK/RELEASE/LOW/
 // HIGH/MODE/QUALITY (secondary) smaller but clearly legible, DELTA/BYPASS
 // just wider/better-spaced (their own button text is already the label).
 auto knobCaptionAbove=[&](juce::Component&c,const char*name,float sz,juce::Colour col,int gapAbove,int rowH){
     auto b=c.getBounds();
     g.setColour(col);g.setFont(sz);
     g.drawText(name,b.getX()-6,b.getY()-gapAbove,b.getWidth()+12,rowH,juce::Justification::centred);
 };
 const juce::Colour principalCol(0xffeaf4ff), secondaryCol(0xffc3d3e2);
 knobCaptionAbove(depth,"DEPTH",12.5f,principalCol,20,17);
 knobCaptionAbove(output,"OUTPUT",12.0f,principalCol,20,16);
 knobCaptionAbove(mix,"MIX",12.5f,principalCol,15,14);
 knobCaptionAbove(sharp,"SHARPNESS",9.8f,secondaryCol,15,13);
 knobCaptionAbove(detail,"DETAIL",9.8f,secondaryCol,15,13);
 knobCaptionAbove(select,"SELECTIVITY",9.8f,secondaryCol,15,13);
 knobCaptionAbove(attack,"ATTACK",9.8f,secondaryCol,15,13);
 knobCaptionAbove(release,"RELEASE",9.8f,secondaryCol,15,13);
 knobCaptionAbove(low,"LOW",9.5f,secondaryCol,9,9);
 knobCaptionAbove(high,"HIGH",9.5f,secondaryCol,9,9);
 // TRANSIENT never had a caption of its own (pre-existing gap, unrelated
 // to Max Reduction/QUALITY) -- now that MAX RED sits directly above it,
 // the missing label read as ambiguous. Same secondary-knob caption style.
 knobCaptionAbove(transient,"TRANSIENT",9.5f,secondaryCol,14,12);
 g.setColour(secondaryCol);g.setFont(10.8f);
 g.drawText("MODE",mode.getX(),mode.getY()-16,mode.getWidth(),14,juce::Justification::centredLeft);
 // DETECT (Etapa 1, External Sidechain): same caption style as MODE
 // (label row above the box). Internal (default) = today's behaviour,
 // Sidechain = the optional second input bus feeds the detector instead.
 g.drawText("DETECT",detect.getX(),detect.getY()-16,detect.getWidth(),14,juce::Justification::centredLeft);
 g.drawText("ORIGINAL    REDUCTION",spectrum.getX(),spectrum.getY()-16,spectrum.getWidth(),14,juce::Justification::centredRight); // 0.1e: RESONANCES removed -- no matching overlay exists yet

 // 0.1i: manufacturer footer -- anchored to the LIVE bottom edge (not the
 // base-canvas R() mapping) so it always hugs the real window edge at any
 // resize. Sits in the existing free gap below the (shorter) LOW/HIGH/right-
 // column content; "V1.0" stops well clear of the built-in resize-corner
 // handle in the bottom-right.
 float footerSigSz=juce::jlimit(16.0f,22.0f,(float)getHeight()*0.028f), footerVerSz=juce::jlimit(9.5f,11.5f,(float)getHeight()*0.015f); // NF Audio Tools bumped up and re-centred as its own row
 g.setColour(juce::Colour(0xffeaf4ff).withAlpha(0.9f));g.setFont(footerSigSz);
 g.drawText("NF Audio Tools",0,getHeight()-26,getWidth(),22,juce::Justification::centred);
 g.setColour(juce::Colour(0xff708090));g.setFont(footerVerSz);
 g.drawText("V1.0",getWidth()-70,getHeight()-18,46,14,juce::Justification::centredRight);
}
void NFResonanceAudioProcessorEditor::resized(){
 // 0.1f: proportional layout, scaled from a fixed 1180x650 base canvas
 // (aspect ratio locked via the constrainer set in the constructor), so the
 // same relative spacing holds at every permitted window size.
 // 0.1g: column-B (SHARPNESS/SELECTIVITY/ATTACK/RELEASE) knobs now sit on a
 // wider pitch with real label rows above them (drawn in paint()) instead of
 // captions overlaid on the knob face, and LOW/HIGH follow after RELEASE
 // with a real gap -- this is what actually fixes the RELEASE/HIGH overlap
 // (0.1f's fix only moved LOW/HIGH down by a few px; this widens the whole
 // stack so there's genuine breathing room, not just a non-overlapping seam).
 const float sx = (float) getWidth() / (float) kBaseW, sy = (float) getHeight() / (float) kBaseH;
 auto R = [&](float x, float y, float w, float h) { return juce::Rectangle<int>((int) std::round(x * sx), (int) std::round(y * sy), (int) std::round(w * sx), (int) std::round(h * sy)); };
 depth.setBounds(R(20, 90, 150, 150));
 // DETAIL (0.1s): grouped with DEPTH -- not column-B -- since conceptually
 // DEPTH is "how much" and DETAIL is "how granular", the same pairing the
 // spec calls for. Sits in column-A's own empty space below DEPTH (240)
 // and well above LOW/HIGH (551), same small-knob size/style as SHARPNESS/
 // SELECTIVITY/ATTACK/RELEASE always used. Column-B (SHARPNESS/
 // SELECTIVITY/ATTACK/RELEASE) is untouched, back to its original spacing.
 detail.setBounds(R(48, 290, 94, 94));
 sharp.setBounds(R(178, 99, 94, 94));
 select.setBounds(R(178, 213, 94, 94));
 attack.setBounds(R(178, 327, 94, 94));
 release.setBounds(R(178, 441, 94, 94));
 // LOW/HIGH re-centred (0.1s) to share the exact same centerX as the
 // column above them -- column 1 (DEPTH/DETAIL, centerX=95) for LOW,
 // column 2 (SHARPNESS/SELECTIVITY/ATTACK/RELEASE, centerX=225) for HIGH.
 // Size/width unchanged (100x99), only the X origin moves. Y nudged up
 // from 551 to 545 -- as tight as RELEASE's own bottom edge (535) allows
 // while still leaving the HIGH/LOW caption its own clear row (see the
 // matching gapAbove/rowH shrink in paint()), trimming the empty gap
 // below DETAIL without touching knob sizes or column-B's own positions.
 low.setBounds(R(45, 545, 100, 99));
 high.setBounds(R(175, 545, 100, 99));
 // RANGE light: the CLICKABLE component is 24x16px (a comfortable hit-
 // area, especially on Retina) even though the drawn mark inside it stays
 // exactly 16x5px (see RangeLightButton::paint()) -- both share the same
 // centre, x=160 (the middle of the 30px gap between LOW, ending at
 // x=145, and HIGH, starting at x=175) and y=594 (vertically centred on
 // the knobs' own row, y=545..644). 24px wide still fits entirely inside
 // that 30px gap without overlapping either knob. Scales with the rest of
 // the layout via the same R() proportional mapping every other control
 // uses, so its position and hit-area stay correctly centred at any
 // window size. Purely additive -- LOW/HIGH's own bounds above are
 // untouched.
 rangeLight.setBounds(R(148, 586, 24, 16));
 spectrum.setBounds(R(290, 82, 690, 473));
 // RIGHT COLUMN: every group below shares ONE central axis, Xc=1076 (the
 // centre of DETECT/MODE's own 152px-wide span, x=1000..1152 -- the widest
 // fixed element in the column, so it sets the axis). OUTPUT/MIX/TRANSIENT
 // (130 wide) and the MAX RED block (84 wide) are each re-centred on Xc
 // without changing their own size. DELTA+BYPASS's combined span (1000..
 // 1152, widths unchanged) already shares Xc exactly. Vertical rhythm
 // standardized to an 18px gap between every group's own box-bottom and
 // the next group's box-top (a caption drawn in that gap, e.g. DETECT's/
 // MODE's/TRANSIENT's own gapAbove of 14-16px, always has 2px+ of clear
 // space before the previous box -- verified, not just assumed).
 const float colXc = 1076.0f;
 output.setBounds(R(colXc - 65.0f, 88, 130, 130));
 mix.setBounds(R(colXc - 65.0f, 236, 130, 130));
 delta.setBounds(R(1000, 384, 68, 30));
 bypass.setBounds(R(1080, 384, 72, 30));
 // DETECT (Etapa 1, External Sidechain) inserted above MODE, own caption
 // row included (its gapAbove=16 fits inside the 18px inter-group gap).
 detect.setBounds(R(1000, 432, 152, 20));
 mode.setBounds(R(1000, 470, 152, 30));
 // MAX REDUCTION -- occupies the space freed by QUALITY's removal.
 // Compact toggle + small knob, not another large primary knob. Toggle
 // text IS the on/off label (same convention as DELTA/BYPASS); the knob
 // always shows the dB value, dimmed when disabled (same dimming
 // convention timerCallback() already uses for LOW/HIGH). Re-centred on
 // Xc; the 4px gap between toggle and knob is intentionally tighter --
 // they're one group, not two.
 maxRedEnabled.setBounds(R(colXc - 42.0f, 518, 84, 18));
 maxRed.setBounds(R(colXc - 42.0f, 540, 84, 34));
 // TRANSIENT: own caption (gapAbove=14) fits inside the 18px gap above it.
 transient.setBounds(R(colXc - 65.0f, 592, 130, 52));
 fft.setBounds(R(290, 65, 46, 15)); // 0.1r: discreet FFT/ORIGINAL toggle, same header row as the legend
 // Preset selector + hamburger anchored to the TOP-RIGHT corner (not a
 // fixed left-based coordinate), so they stay pinned there through resize
 // exactly like the OUTPUT/MIX column already does with its own R() calls.
 presetButton.setBounds(R(1008, 20, 118, 24));
 menuButton.setBounds(R(1134, 22, 22, 18));

 // Curve overlay: exactly the analyzer's own inner plot area (same log-Hz
 // axis), in editor coordinates.
 auto plot = SpectrumComponent::plotAreaFor(spectrum.getLocalBounds().toFloat());
 curve.setBounds(spectrum.getX() + (int) plot.getX(), spectrum.getY() + (int) plot.getY(), (int) plot.getWidth(), (int) plot.getHeight());
}

// Same per-user "Manual" folder both installers (macOS .pkg postinstall,
// Windows Inno Setup) place the manuals into -- this is the SAME
// juce::File::userApplicationDataDirectory-based path on every platform
// (~/Library/Application Support/... on macOS, %APPDATA%\... on Windows),
// so this one implementation covers both without any #if. Checked for
// existence before ever being offered as clickable (see showMainMenu()) --
// a menu item never appears for a manual that isn't actually installed.
juce::File NFResonanceAudioProcessorEditor::manualFileEN()
{
    return PresetManager::presetsRootFolder().getParentDirectory().getChildFile("Manual").getChildFile("NF_Resonance_Manual_EN_V1.0.pdf");
}
juce::File NFResonanceAudioProcessorEditor::manualFilePT()
{
    return PresetManager::presetsRootFolder().getParentDirectory().getChildFile("Manual").getChildFile("NF_Resonance_Manual_PT_V1.0.pdf");
}

void NFResonanceAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    int id = 1;
    std::vector<std::pair<int,int>> factoryIds; // menu id -> preset index
    for (int i = 0; i < (int) presetManager->factoryPresets().size(); ++i)
    {
        auto& fp = presetManager->factoryPresets()[(size_t) i];
        menu.addItem(id, fp.name, true, currentPresetName == fp.name && ! currentIsUserPreset);
        factoryIds.push_back({ id, i });
        ++id;
    }
    auto userNames = presetManager->listUserPresets();
    std::vector<std::pair<int,juce::String>> userIds;
    if (! userNames.isEmpty())
    {
        menu.addSeparator();
        for (auto& name : userNames)
        {
            menu.addItem(id, name, true, currentPresetName == name && currentIsUserPreset);
            userIds.push_back({ id, name });
            ++id;
        }
    }
    menu.addSeparator();
    const int idSave = id++, idSaveAs = id++, idDelete = id++;
    menu.addItem(idSave, "Save Preset...", currentIsUserPreset);
    menu.addItem(idSaveAs, "Save Preset As...");
    menu.addItem(idDelete, "Delete User Preset", currentIsUserPreset);

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, factoryIds, userIds, idSave, idSaveAs, idDelete](int result)
    {
        if (result == 0) return;
        if (result == idSaveAs) { promptSaveAs(); return; }
        if (result == idSave)
        {
            if (currentIsUserPreset && presetManager->saveUserPreset(currentPresetName))
                presetButton.setButtonText(currentPresetName + " v");
            return;
        }
        if (result == idDelete)
        {
            if (currentIsUserPreset && presetManager->deleteUserPreset(currentPresetName))
            {
                currentPresetName = "Default"; currentIsUserPreset = false;
                presetManager->applyFactoryPreset(0);
                presetButton.setButtonText(currentPresetName + " v");
            }
            return;
        }
        for (auto& fi : factoryIds) if (fi.first == result)
        {
            presetManager->applyFactoryPreset(fi.second);
            currentPresetName = presetManager->factoryPresets()[(size_t) fi.second].name;
            currentIsUserPreset = false;
            presetButton.setButtonText(currentPresetName + " v");
            return;
        }
        for (auto& ui : userIds) if (ui.first == result)
        {
            if (presetManager->loadUserPreset(ui.second))
            {
                currentPresetName = ui.second; currentIsUserPreset = true;
                presetButton.setButtonText(currentPresetName + " v");
            }
            return;
        }
    });
}

void NFResonanceAudioProcessorEditor::promptSaveAs()
{
    nameDialog = std::make_unique<juce::AlertWindow>("Save Preset As", "Preset name:", juce::AlertWindow::NoIcon);
    nameDialog->addTextEditor("name", currentIsUserPreset ? currentPresetName : juce::String());
    nameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    nameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    nameDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1)
        {
            auto name = nameDialog->getTextEditorContents("name").trim();
            if (name.isNotEmpty() && presetManager->saveUserPreset(name))
            {
                currentPresetName = name; currentIsUserPreset = true;
                presetButton.setButtonText(currentPresetName + " v");
            }
        }
        nameDialog.reset();
    }), true);
}

void NFResonanceAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu menu;
    bool enAvailable = manualFileEN().existsAsFile();
    bool ptAvailable = manualFilePT().existsAsFile();
    menu.addItem(5, "Manual (English)", enAvailable);
    menu.addItem(6, "Manual (Portugues)", ptAvailable);
    menu.addItem(2, "About NF Resonance");
    menu.addItem(3, "Reset to Default");
    menu.addItem(4, "Open Preset Folder");
    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        if (result == 5) { manualFileEN().startAsProcess(); }
        else if (result == 6) { manualFilePT().startAsProcess(); }
        else if (result == 2)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::NoIcon, "About NF Resonance",
                "NF Resonance V1.0\nNF Audio Tools\n\nDynamic Resonance Suppressor.");
        }
        else if (result == 3)
        {
            presetManager->resetToDefault();
            currentPresetName = "Default"; currentIsUserPreset = false;
            presetButton.setButtonText(currentPresetName + " v");
        }
        else if (result == 4)
        {
            PresetManager::presetsRootFolder().revealToUser();
        }
    });
}
