#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Labelled rotary (or linear) slider with APVTS attachment
//==============================================================================
class LabelledSlider : public juce::Component
{
public:
    LabelledSlider(const juce::String& labelText,
        juce::AudioProcessorValueTreeState& apvts,
        const juce::String& paramID,
        juce::Slider::SliderStyle style = juce::Slider::RotaryVerticalDrag)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.f)));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff9090b0));
        addAndMakeVisible(label);

        slider.setSliderStyle(style);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc8a84b));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a3a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffc8a84b));
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xffc8a84b));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffc0b090));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        addAndMakeVisible(slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (apvts, paramID, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds(b.removeFromBottom(16));
        slider.setBounds(b);
    }

    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Toggle button with APVTS attachment
//==============================================================================
class LabelledToggle : public juce::Component
{
public:
    LabelledToggle(const juce::String& labelText,
        juce::AudioProcessorValueTreeState& apvts,
        const juce::String& paramID)
    {
        button.setButtonText(labelText);
        button.setClickingTogglesState(true);
        button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff9090b0));
        button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffc8a84b));
        button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff3a3a4a));
        addAndMakeVisible(button);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (apvts, paramID, button);
    }

    void resized() override { button.setBounds(getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
// Combo box with APVTS attachment (for Choice parameters)
//==============================================================================
class LabelledCombo : public juce::Component
{
public:
    LabelledCombo(const juce::String& labelText,
        juce::AudioProcessorValueTreeState& apvts,
        const juce::String& paramID)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.f)));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff9090b0));
        addAndMakeVisible(label);

        combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a24));
        combo.setColour(juce::ComboBox::textColourId, juce::Colour(0xffc0b090));
        combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3a4a));
        combo.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffc8a84b));
        addAndMakeVisible(combo);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            (apvts, paramID, combo);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds(b.removeFromTop(16));
        combo.setBounds(b.removeFromTop(24));
    }

    juce::ComboBox combo;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
// Titled section panel (painted group box)
//==============================================================================
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel(const juce::String& t,
        juce::Colour accent = juce::Colour(0xffc8a84b))
        : title(t), accentColour(accent) {}

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff14141e));
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(accentColour.withAlpha(0.35f));
        g.drawRoundedRectangle(b.reduced(0.5f), 6.f, 1.f);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f).withStyle("Bold")));
        g.setColour(accentColour);
        g.drawText(title.toUpperCase(),
            getLocalBounds().reduced(8, 0).removeFromTop(18),
            juce::Justification::centredLeft);
    }

    /// Returns the usable content rectangle in the PARENT component's coordinate space.
    /// (Children of the parent tab are positioned using these coordinates.)
    juce::Rectangle<int> getContentBounds() const
    {
        // getBounds() is already in parent space; trim border + title bar
        return getBounds().reduced(6).withTrimmedTop(16);
    }

private:
    juce::String title;
    juce::Colour accentColour;
};


//==============================================================================
// Main editor — 100% native JUCE, no WebView, no browser
//==============================================================================
class ChromaDelayEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit ChromaDelayEditor(ChromaDelayAudioProcessor&);
    ~ChromaDelayEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ChromaDelayAudioProcessor& audioProcessor;

    //==========================================================================
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // --- TAB 1: CORE ---
    juce::Component coreTab;
    SectionPanel delaySection{ "Delay",        juce::Colour(0xffc8a84b) };
    LabelledSlider slDelayTime{ "Time",         audioProcessor.apvts, ParamID::delayTimeMs };
    LabelledSlider slFeedback{ "Feedback",     audioProcessor.apvts, ParamID::feedback };
    LabelledSlider slMix{ "Mix",          audioProcessor.apvts, ParamID::mix };
    LabelledSlider slInputGain{ "In Gain",      audioProcessor.apvts, ParamID::inputGain };
    LabelledSlider slOutput{ "Output",       audioProcessor.apvts, ParamID::outputLevel };
    LabelledSlider slGridOffset{ "Slop",         audioProcessor.apvts, ParamID::gridOffset };
    LabelledToggle tgPhaseInv{ "Phase Inv",    audioProcessor.apvts, ParamID::phaseInvert };
    LabelledToggle tgLockHold{ "Lock/Hold",    audioProcessor.apvts, ParamID::lockHold };

    SectionPanel syncSection{ "Sync / Rhythm", juce::Colour(0xff4b7ec8) };
    LabelledToggle tgSync{ "BPM Sync",     audioProcessor.apvts, ParamID::syncEnabled };
    LabelledCombo  cbDivision{ "Division",     audioProcessor.apvts, ParamID::noteDivision };
    LabelledCombo  cbModifier{ "Modifier",     audioProcessor.apvts, ParamID::modifier };
    juce::TextButton btnTap     { "TAP TEMPO" };
    juce::Label      lblBpm;

    // --- TAB 2: COLOR FX ---
    juce::Component colorTab;
    SectionPanel bcSection{ "Bitcrusher",   juce::Colour(0xffc88b4b) };
    LabelledToggle tgBc{ "Enable",       audioProcessor.apvts, ParamID::bcEnabled };
    LabelledSlider slBcBits{ "Bit Depth",    audioProcessor.apvts, ParamID::bcBits };
    LabelledSlider slBcSrDiv{ "SR Divide",    audioProcessor.apvts, ParamID::bcSampleRateDiv };

    SectionPanel satSection{ "Saturation",   juce::Colour(0xffc84b6e) };
    LabelledToggle tgSat{ "Enable",       audioProcessor.apvts, ParamID::satEnabled };
    LabelledSlider slSatDrive{ "Drive",        audioProcessor.apvts, ParamID::satDrive };

    SectionPanel filtSection{ "Resonant Filter", juce::Colour(0xff4bc8a8) };
    LabelledToggle tgFilt{ "Enable",       audioProcessor.apvts, ParamID::filtEnabled };
    LabelledCombo  cbFiltType{ "Type",         audioProcessor.apvts, ParamID::filtType };
    LabelledSlider slCutoff{ "Cutoff",       audioProcessor.apvts, ParamID::filtCutoff };
    LabelledSlider slResonance{ "Resonance",    audioProcessor.apvts, ParamID::filtResonance };

    // --- TAB 3: MOVEMENT FX ---
    juce::Component movementTab;
    SectionPanel chorusSection{ "Chorus / Vibrato", juce::Colour(0xff8b6bc8) };
    LabelledToggle tgChorus{ "Enable",       audioProcessor.apvts, ParamID::chorusEnabled };
    LabelledSlider slChorusRate{ "Rate",         audioProcessor.apvts, ParamID::chorusRate };
    LabelledSlider slChorusDpth{ "Depth",        audioProcessor.apvts, ParamID::chorusDepth };
    LabelledSlider slChorusMix{ "Mix",          audioProcessor.apvts, ParamID::chorusMix };

    SectionPanel phaserSection{ "Phaser",       juce::Colour(0xff4b7ec8) };
    LabelledToggle tgPhaser{ "Enable",       audioProcessor.apvts, ParamID::phaserEnabled };
    LabelledSlider slPhaserRate{ "Rate",         audioProcessor.apvts, ParamID::phaserRate };
    LabelledSlider slPhaserDpth{ "Depth",        audioProcessor.apvts, ParamID::phaserDepth };
    LabelledSlider slPhaserFb{ "Feedback",     audioProcessor.apvts, ParamID::phaserFeedback };

    SectionPanel tremSection{ "Tremolo",      juce::Colour(0xff6bc84b) };
    LabelledToggle tgTrem{ "Enable",       audioProcessor.apvts, ParamID::tremEnabled };
    LabelledSlider slTremRate{ "Rate",         audioProcessor.apvts, ParamID::tremRate };
    LabelledSlider slTremDepth{ "Depth",        audioProcessor.apvts, ParamID::tremDepth };
    LabelledToggle tgTremSync{ "Sync",         audioProcessor.apvts, ParamID::tremSync };

    // --- TAB 4: UTILITY FX ---
    juce::Component utilityTab;
    SectionPanel harmSection{ "Harmonizer",   juce::Colour(0xffc8a84b) };
    LabelledToggle tgHarm{ "Enable",       audioProcessor.apvts, ParamID::harmEnabled };
    LabelledCombo  cbHarmMode{ "Mode",         audioProcessor.apvts, ParamID::harmMode };
    LabelledSlider slHarmInt{ "Semitones",    audioProcessor.apvts, ParamID::harmInterval,
                                  juce::Slider::LinearHorizontal };
    LabelledSlider slHarmMix{ "Mix",          audioProcessor.apvts, ParamID::harmMix };

    SectionPanel ghostSection{ "Ghost Delay",  juce::Colour(0xff8b6bc8) };
    LabelledToggle tgGhost{ "Enable",       audioProcessor.apvts, ParamID::ghostEnabled };
    LabelledSlider slGhostTime{ "Time",         audioProcessor.apvts, ParamID::ghostTime };
    LabelledSlider slGhostFb{ "Feedback",     audioProcessor.apvts, ParamID::ghostFeedback };

    SectionPanel reverbSection{ "Reverb",       juce::Colour(0xff4bc8a8) };
    LabelledToggle tgReverb{ "Enable",       audioProcessor.apvts, ParamID::reverbEnabled };
    LabelledSlider slRevSize{ "Size",         audioProcessor.apvts, ParamID::reverbSize };
    LabelledSlider slRevDamp{ "Damping",      audioProcessor.apvts, ParamID::reverbDamping };
    LabelledSlider slRevMix{ "Mix",          audioProcessor.apvts, ParamID::reverbMix };

    SectionPanel reverseSection{ "Reverse",      juce::Colour(0xffc84b6e) };
    LabelledToggle tgReverse{ "Enable",       audioProcessor.apvts, ParamID::reverseEnabled };

    //==========================================================================
    void timerCallback() override;
    void layoutCoreTab();
    void layoutColorTab();
    void layoutMovementTab();
    void layoutUtilityTab();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChromaDelayEditor)
};