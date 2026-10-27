#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VocalEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalEditor (VocalProcessor&);
    ~VocalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& title, const juce::String& suffix);
    void placeKnob (Knob&, juce::Rectangle<int> area);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> r, float grDb, const juce::String& name);
    void timerCallback() override;

    VocalProcessor& proc;

    Knob lowCut, mud, warmth, comp, presence, air, dsFreq, dsThresh, dsRange, output, mix;
    juce::ComboBox presetBox, modeBox;
    juce::ToggleButton listenButton { "Listen" };
    std::unique_ptr<ComboAtt> modeAtt;
    std::unique_ptr<ButtonAtt> listenAtt;

    juce::Rectangle<int> toneArea, compArea, outArea, dsArea, meterArea;
    float shownDsGr = 0.0f, shownCompGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditor)
};
