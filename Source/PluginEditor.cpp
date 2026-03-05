#include <JuceHeader.h>
#include "PluginEditor.h"

//==============================================================================
// Colour helpers
//==============================================================================
static const juce::Colour BG_DARK(0xff0e0e12);
static const juce::Colour BG_MID(0xff14141e);
static const juce::Colour GOLD(0xffc8a84b);
static const juce::Colour TAB_BG(0xff1a1a26);

//==============================================================================
ChromaDelayEditor::ChromaDelayEditor(ChromaDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{

    //----------------------------------------------------------------------
    // Style the tab bar
    tabs.setColour(juce::TabbedComponent::backgroundColourId, BG_DARK);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colour(0xff2a2a3a));
    tabs.setTabBarDepth(32);

    //----------------------------------------------------------------------
    // Add child components to each tab page BEFORE adding tabs
    // (tabs takes ownership of the Component* via addTab)
    //----------------------------------------------------------------------

    // We pass raw pointers of our member Component objects — tabs does NOT
    // take ownership when deleteComponentWhenNotNeeded = false
    auto addToTab = [&](juce::Component& tab, juce::Component& child) {
        tab.addAndMakeVisible(child);
    };

    // --- Core tab children ---
    addToTab(coreTab, delaySection);
    addToTab(coreTab, slDelayTime);
    addToTab(coreTab, slFeedback);
    addToTab(coreTab, slMix);
    addToTab(coreTab, slInputGain);
    addToTab(coreTab, slOutput);
    addToTab(coreTab, slGridOffset);
    addToTab(coreTab, tgPhaseInv);
    addToTab(coreTab, tgLockHold);

    addToTab(coreTab, syncSection);
    addToTab(coreTab, tgSync);
    addToTab(coreTab, cbDivision);
    addToTab(coreTab, cbModifier);

    btnTap.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e2a));
    btnTap.setColour(juce::TextButton::buttonOnColourId, GOLD.withAlpha(0.3f));
    btnTap.setColour(juce::TextButton::textColourOffId, GOLD);
    btnTap.setColour(juce::TextButton::textColourOnId, GOLD);
    btnTap.onClick = [this] { audioProcessor.tapTempo(); };
    addToTab(coreTab, btnTap);

    lblBpm.setFont(juce::Font(juce::FontOptions{}.withHeight(11.f).withStyle("Bold")));
    lblBpm.setColour(juce::Label::textColourId, juce::Colour(0xff7a7a9a));
    lblBpm.setJustificationType(juce::Justification::centred);
    lblBpm.setText("-- BPM", juce::dontSendNotification);
    addToTab(coreTab, lblBpm);

    // --- Color tab children ---
    addToTab(colorTab, bcSection);
    addToTab(colorTab, tgBc);
    addToTab(colorTab, slBcBits);
    addToTab(colorTab, slBcSrDiv);
    addToTab(colorTab, satSection);
    addToTab(colorTab, tgSat);
    addToTab(colorTab, slSatDrive);
    addToTab(colorTab, filtSection);
    addToTab(colorTab, tgFilt);
    addToTab(colorTab, cbFiltType);
    addToTab(colorTab, slCutoff);
    addToTab(colorTab, slResonance);

    // --- Movement tab children ---
    addToTab(movementTab, chorusSection);
    addToTab(movementTab, tgChorus);
    addToTab(movementTab, slChorusRate);
    addToTab(movementTab, slChorusDpth);
    addToTab(movementTab, slChorusMix);
    addToTab(movementTab, phaserSection);
    addToTab(movementTab, tgPhaser);
    addToTab(movementTab, slPhaserRate);
    addToTab(movementTab, slPhaserDpth);
    addToTab(movementTab, slPhaserFb);
    addToTab(movementTab, tremSection);
    addToTab(movementTab, tgTrem);
    addToTab(movementTab, slTremRate);
    addToTab(movementTab, slTremDepth);
    addToTab(movementTab, tgTremSync);

    // --- Utility tab children ---
    addToTab(utilityTab, harmSection);
    addToTab(utilityTab, tgHarm);
    addToTab(utilityTab, cbHarmMode);
    addToTab(utilityTab, slHarmInt);
    addToTab(utilityTab, slHarmMix);
    addToTab(utilityTab, ghostSection);
    addToTab(utilityTab, tgGhost);
    addToTab(utilityTab, slGhostTime);
    addToTab(utilityTab, slGhostFb);
    addToTab(utilityTab, reverbSection);
    addToTab(utilityTab, tgReverb);
    addToTab(utilityTab, slRevSize);
    addToTab(utilityTab, slRevDamp);
    addToTab(utilityTab, slRevMix);
    addToTab(utilityTab, reverseSection);
    addToTab(utilityTab, tgReverse);

    //----------------------------------------------------------------------
    // Register tabs — false = tabs does NOT delete our member components
    tabs.addTab("CORE", TAB_BG, &coreTab, false);
    tabs.addTab("COLOR", TAB_BG, &colorTab, false);
    tabs.addTab("MOVEMENT", TAB_BG, &movementTab, false);
    tabs.addTab("UTILITY", TAB_BG, &utilityTab, false);

    // Style tab buttons
    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        tabs.setTabBackgroundColour(i, juce::Colour(0xff18181e));
    }

    addAndMakeVisible(tabs);

    startTimerHz(10);

    setSize(860, 560);
    setResizable(true, true);
    setResizeLimits(700, 460, 1400, 900);

}

ChromaDelayEditor::~ChromaDelayEditor()
{
    stopTimer();
}

//==============================================================================
void ChromaDelayEditor::paint(juce::Graphics& g)
{
    g.fillAll(BG_DARK);

    // Header strip
    g.setColour(BG_MID);
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(GOLD.withAlpha(0.3f));
    g.drawLine(0.f, 40.f, (float)getWidth(), 40.f, 1.f);

    // Plugin name — "The DRENCHED Delay"
    const int headerY = 0;
    const int headerH = 40;
    int x = 16;

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.f)));
    g.setColour(juce::Colour(0xff7a7a9a));
    g.drawText("The", x, headerY, 36, headerH, juce::Justification::centredLeft);
    x += 38;

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(20.f).withStyle("Bold")));
    g.setColour(GOLD);
    g.drawText("DRENCHED", x, headerY, 120, headerH, juce::Justification::centredLeft);
    x += 122;

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.f)));
    g.setColour(juce::Colour(0xff7a7a9a));
    g.drawText("Delay", x, headerY, 60, headerH, juce::Justification::centredLeft);

    // Version (keep as-is)
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f)));
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawText("v1.0.0", getWidth() - 56, 0, 48, 40, juce::Justification::centredRight);

    // Version
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f)));
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawText("v1.0.0", getWidth() - 56, 0, 48, 40, juce::Justification::centredRight);
}

void ChromaDelayEditor::resized()
{
    auto area = getLocalBounds();

    // Leave room for header
    area.removeFromTop(40);

    tabs.setBounds(area);

    layoutCoreTab();
    layoutColorTab();
    layoutMovementTab();
    layoutUtilityTab();
}

//==============================================================================
void ChromaDelayEditor::layoutCoreTab()
{
    auto area = coreTab.getLocalBounds().reduced(10);

    // --- Delay section (top, full width) ---
    auto delayRow = area.removeFromTop(160);
    delaySection.setBounds(delayRow);
    auto dContent = delaySection.getContentBounds();

    const int kw = 76; // knob width
    // 6 knobs + 2 toggles
    slDelayTime.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(4);
    slFeedback.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(4);
    slMix.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(4);
    slInputGain.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(4);
    slOutput.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(4);
    slGridOffset.setBounds(dContent.removeFromLeft(kw));
    dContent.removeFromLeft(8);
    // Toggles stacked on the right
    auto toggleCol = dContent.removeFromLeft(90);
    tgPhaseInv.setBounds(toggleCol.removeFromTop(30));
    toggleCol.removeFromTop(6);
    tgLockHold.setBounds(toggleCol.removeFromTop(30));

    area.removeFromTop(10);

    // --- Sync section (bottom) ---
    auto syncRow = area.removeFromTop(160);
    syncSection.setBounds(syncRow);
    auto sContent = syncSection.getContentBounds();

    tgSync.setBounds(sContent.removeFromLeft(90).removeFromTop(28));
    sContent.removeFromLeft(10);
    cbDivision.setBounds(sContent.removeFromLeft(120).removeFromTop(56));
    sContent.removeFromLeft(10);
    cbModifier.setBounds(sContent.removeFromLeft(120).removeFromTop(56));
    sContent.removeFromLeft(20);
    // Tap tempo + BPM display
    auto tapCol = sContent.removeFromLeft(110);
    btnTap.setBounds(tapCol.removeFromTop(36));
    tapCol.removeFromTop(6);
    lblBpm.setBounds(tapCol.removeFromTop(20));
}

//==============================================================================
void ChromaDelayEditor::layoutColorTab()
{
    auto area = colorTab.getLocalBounds().reduced(10);
    const int kw = 76;

    // Bitcrusher
    auto bcRow = area.removeFromTop(150);
    bcSection.setBounds(bcRow);
    auto bc = bcSection.getContentBounds();
    tgBc.setBounds(bc.removeFromLeft(90).removeFromTop(28));
    bc.removeFromLeft(8);
    slBcBits.setBounds(bc.removeFromLeft(kw));
    bc.removeFromLeft(4);
    slBcSrDiv.setBounds(bc.removeFromLeft(kw));

    area.removeFromTop(8);

    // Saturation
    auto satRow = area.removeFromTop(150);
    satSection.setBounds(satRow);
    auto sat = satSection.getContentBounds();
    tgSat.setBounds(sat.removeFromLeft(90).removeFromTop(28));
    sat.removeFromLeft(8);
    slSatDrive.setBounds(sat.removeFromLeft(kw));

    area.removeFromTop(8);

    // Filter
    auto filtRow = area;
    filtSection.setBounds(filtRow);
    auto filt = filtSection.getContentBounds();
    tgFilt.setBounds(filt.removeFromLeft(90).removeFromTop(28));
    filt.removeFromLeft(8);
    cbFiltType.setBounds(filt.removeFromLeft(110).removeFromTop(56));
    filt.removeFromLeft(8);
    slCutoff.setBounds(filt.removeFromLeft(kw));
    filt.removeFromLeft(4);
    slResonance.setBounds(filt.removeFromLeft(kw));
}

//==============================================================================
void ChromaDelayEditor::layoutMovementTab()
{
    auto area = movementTab.getLocalBounds().reduced(10);
    const int kw = 76;

    // Chorus
    auto choRow = area.removeFromTop(150);
    chorusSection.setBounds(choRow);
    auto cho = chorusSection.getContentBounds();
    tgChorus.setBounds(cho.removeFromLeft(80).removeFromTop(28));
    cho.removeFromLeft(6);
    slChorusRate.setBounds(cho.removeFromLeft(kw));
    cho.removeFromLeft(4);
    slChorusDpth.setBounds(cho.removeFromLeft(kw));
    cho.removeFromLeft(4);
    slChorusMix.setBounds(cho.removeFromLeft(kw));

    area.removeFromTop(8);

    // Phaser
    auto phRow = area.removeFromTop(150);
    phaserSection.setBounds(phRow);
    auto ph = phaserSection.getContentBounds();
    tgPhaser.setBounds(ph.removeFromLeft(80).removeFromTop(28));
    ph.removeFromLeft(6);
    slPhaserRate.setBounds(ph.removeFromLeft(kw));
    ph.removeFromLeft(4);
    slPhaserDpth.setBounds(ph.removeFromLeft(kw));
    ph.removeFromLeft(4);
    slPhaserFb.setBounds(ph.removeFromLeft(kw));

    area.removeFromTop(8);

    // Tremolo
    auto trRow = area;
    tremSection.setBounds(trRow);
    auto tr = tremSection.getContentBounds();
    tgTrem.setBounds(tr.removeFromLeft(80).removeFromTop(28));
    tr.removeFromLeft(6);
    slTremRate.setBounds(tr.removeFromLeft(kw));
    tr.removeFromLeft(4);
    slTremDepth.setBounds(tr.removeFromLeft(kw));
    tr.removeFromLeft(10);
    tgTremSync.setBounds(tr.removeFromLeft(80).removeFromTop(28));
}

//==============================================================================
void ChromaDelayEditor::layoutUtilityTab()
{
    auto area = utilityTab.getLocalBounds().reduced(10);
    const int kw = 76;

    // Top row: Harmonizer | Ghost Delay
    auto topRow = area.removeFromTop(170);

    auto harmArea = topRow.removeFromLeft(topRow.getWidth() / 2 - 4);
    harmSection.setBounds(harmArea);
    auto harm = harmSection.getContentBounds();
    tgHarm.setBounds(harm.removeFromLeft(80).removeFromTop(28));
    harm.removeFromLeft(6);
    cbHarmMode.setBounds(harm.removeFromLeft(110).removeFromTop(56));
    harm.removeFromLeft(6);
    slHarmMix.setBounds(harm.removeFromLeft(kw));
    // Semitone slider below
    auto harmLower = harmSection.getContentBounds();
    harmLower.removeFromTop(70);
    slHarmInt.setBounds(harmLower.reduced(4, 0).removeFromTop(40));

    topRow.removeFromLeft(8);
    ghostSection.setBounds(topRow);
    auto ghost = ghostSection.getContentBounds();
    tgGhost.setBounds(ghost.removeFromLeft(80).removeFromTop(28));
    ghost.removeFromLeft(6);
    slGhostTime.setBounds(ghost.removeFromLeft(kw));
    ghost.removeFromLeft(4);
    slGhostFb.setBounds(ghost.removeFromLeft(kw));

    area.removeFromTop(8);

    // Bottom row: Reverb | Reverse
    auto botRow = area;

    auto revArea = botRow.removeFromLeft(botRow.getWidth() * 3 / 4 - 4);
    reverbSection.setBounds(revArea);
    auto rev = reverbSection.getContentBounds();
    tgReverb.setBounds(rev.removeFromLeft(80).removeFromTop(28));
    rev.removeFromLeft(6);
    slRevSize.setBounds(rev.removeFromLeft(kw));
    rev.removeFromLeft(4);
    slRevDamp.setBounds(rev.removeFromLeft(kw));
    rev.removeFromLeft(4);
    slRevMix.setBounds(rev.removeFromLeft(kw));

    botRow.removeFromLeft(8);
    reverseSection.setBounds(botRow);
    auto rvsContent = reverseSection.getContentBounds();
    tgReverse.setBounds(rvsContent.removeFromTop(28));
}

//==============================================================================
void ChromaDelayEditor::timerCallback()
{
    double bpm = audioProcessor.getCurrentBpm();
    lblBpm.setText(juce::String(bpm, 1) + " BPM", juce::dontSendNotification);
}

//==============================================================================
juce::AudioProcessorEditor* ChromaDelayAudioProcessor::createEditor()
{
    return new ChromaDelayEditor(*this);
}