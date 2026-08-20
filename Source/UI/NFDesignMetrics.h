#pragma once

// Master coordinate system the GUI is designed against. NFEqualizerPanel is
// laid out at this exact resolution and the editor scales the whole panel
// as one block to fit whatever size the host actually gives it, so these
// numbers stay the single source of truth for every position on screen.
struct DesignMetrics
{
    static constexpr float width  = 1318.0f;
    static constexpr float height = 487.0f;

    // Outer chassis / green face
    static constexpr float chassisX = 8.0f,  chassisY = 22.0f, chassisW = 1308.0f, chassisH = 452.0f;
    static constexpr float faceX    = 12.0f, faceY    = 26.0f, faceW    = 1300.0f, faceH    = 444.0f;

    // Logo
    static constexpr float logoNfX = 584.0f, logoNfY = 27.0f, logoNfW = 140.0f, logoNfH = 45.0f;
    static constexpr float logoSubX = 586.0f, logoSubY = 74.0f, logoSubW = 137.0f, logoSubH = 21.0f;

    // Skin picker — tucked in the gap between the logo and the NF CHARACTER
    // section, the only bit of open headroom that doesn't collide with
    // anything else.
    static constexpr float skinCaptionX = 828.0f, skinCaptionY = 27.0f, skinCaptionW = 78.0f, skinCaptionH = 12.0f;
    static constexpr float skinButtonY = 40.0f, skinButtonW = 36.0f, skinButtonH = 22.0f, skinButtonGap = 4.0f;
    static constexpr float skinButton1X = 828.0f;
    static constexpr float skinButton2X = skinButton1X + skinButtonW + skinButtonGap;

    // INPUT panel
    static constexpr float inputX = 30.0f, inputY = 49.0f, inputW = 143.0f, inputH = 372.0f;
    static constexpr float inputHeaderX = 61.0f, inputHeaderY = 61.0f, inputHeaderW = 83.0f, inputHeaderH = 23.0f;
    static constexpr float inputKnobCX = 102.0f, inputKnobCY = 147.0f, inputKnobD = 76.0f;
    static constexpr float inputMeterX = 78.0f, inputMeterY = 246.0f, inputMeterW = 49.0f, inputMeterH = 133.0f;

    // LOW section
    static constexpr float lowX = 223.0f, lowY = 94.0f, lowW = 190.0f, lowH = 316.0f;
    static constexpr float lowHeaderX = 243.0f, lowHeaderY = 95.0f, lowHeaderW = 114.0f, lowHeaderH = 20.0f;
    static constexpr float lowFreqCX = 300.0f, lowFreqCY = 182.0f, lowFreqD = 78.0f;
    static constexpr float lowGainCX = 300.0f, lowGainCY = 315.0f, lowGainD = 76.0f;
    static constexpr float lowShelfX = 261.0f, lowShelfY = 384.0f, lowShelfW = 80.0f, lowShelfH = 27.0f;

    // MID section
    static constexpr float midX = 413.0f, midEndX = 712.0f, midHeaderCX = 558.0f;
    static constexpr float midHeaderY = 95.0f, midHeaderW = 114.0f, midHeaderH = 20.0f;
    static constexpr float midFreqCX = 558.0f, midFreqCY = 182.0f, midFreqD = 78.0f;
    static constexpr float midGainCX = 487.0f, midGainCY = 315.0f, midGainD = 76.0f;
    static constexpr float midQCX = 632.0f, midQCY = 315.0f, midQD = 76.0f;

    // HIGH section
    static constexpr float highX = 712.0f;
    static constexpr float highHeaderX = 747.0f, highHeaderY = 95.0f, highHeaderW = 122.0f, highHeaderH = 20.0f;
    static constexpr float highFreqCX = 808.0f, highFreqCY = 182.0f, highFreqD = 78.0f;
    static constexpr float highGainCX = 808.0f, highGainCY = 315.0f, highGainD = 76.0f;
    static constexpr float highShelfX = 770.0f, highShelfY = 384.0f, highShelfW = 80.0f, highShelfH = 27.0f;

    // NF CHARACTER section
    static constexpr float characterX = 911.0f, characterEndX = 1125.0f, characterCX = 1011.0f;
    static constexpr float characterHeaderY = 95.0f, characterHeaderW = 165.0f, characterHeaderH = 20.0f;
    static constexpr float driveCX = 1011.0f, driveCY = 182.0f, driveD = 78.0f;
    static constexpr float characterKnobCX = 1011.0f, characterKnobCY = 281.0f, characterKnobD = 60.0f;
    static constexpr float mixCX = 1011.0f, mixCY = 368.0f, mixD = 50.0f;

    // OUTPUT panel
    static constexpr float outputX = 1152.0f, outputY = 49.0f, outputW = 143.0f, outputH = 372.0f;
    static constexpr float outputHeaderX = 1182.0f, outputHeaderY = 61.0f, outputHeaderW = 83.0f, outputHeaderH = 23.0f;
    static constexpr float outputKnobCX = 1223.0f, outputKnobCY = 147.0f, outputKnobD = 76.0f;
    static constexpr float outputMeterX = 1199.0f, outputMeterY = 246.0f, outputMeterW = 49.0f, outputMeterH = 133.0f;

    // Small purple section indicators (LOW, MID, HIGH, NF CHARACTER)
    static constexpr float led1X = 204.0f, led2X = 450.0f, led3X = 737.0f, led4X = 941.0f;
    static constexpr float ledY = 132.0f, ledD = 20.0f;

    // Vertical dividers
    static constexpr float dividerTopY = 82.0f, dividerBottomY = 410.0f;
    static constexpr float divider1X = 413.0f, divider2X = 712.0f, divider3X = 911.0f, divider4X = 1125.0f;

    // Bottom control bar
    static constexpr float bypassX = 89.0f, bypassY = 436.0f, bypassW = 100.0f, bypassH = 29.0f;
    static constexpr float prevX = 418.0f, prevY = 436.0f, prevW = 38.0f, prevH = 28.0f;
    static constexpr float presetX = 461.0f, presetY = 436.0f, presetW = 235.0f, presetH = 28.0f;
    static constexpr float nextX = 697.0f, nextY = 436.0f, nextW = 38.0f, nextH = 28.0f;
    static constexpr float saveX = 747.0f, saveY = 436.0f, saveW = 71.0f, saveH = 28.0f;
    static constexpr float loadX = 830.0f, loadY = 436.0f, loadW = 76.0f, loadH = 28.0f;
    static constexpr float oversamplingX = 1126.0f, oversamplingY = 437.0f, oversamplingW = 137.0f, oversamplingH = 25.0f;

    // Decorative screws
    static constexpr float screwTopY = 34.0f, screwBottomY = height - 25.0f;
    static constexpr float screwLeftX = 20.0f, screwRightX = 1298.0f;
    static constexpr float screwNearInputX = 176.0f, screwNearOutputX = 1150.0f;
};
