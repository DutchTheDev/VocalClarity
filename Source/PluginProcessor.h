#pragma once
#include <JuceHeader.h>
#include "DSP.h"

class VocalProcessor : public juce::AudioProcessor
{
public:
    VocalProcessor();
    ~VocalProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vocal Clarity"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // meters (dB of gain reduction), read by the editor
    std::atomic<float> deEssGr { 0.0f }, compGr { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pLowCut; std::atomic<float>* pMud; std::atomic<float>* pWarmth;
    std::atomic<float>* pComp;   std::atomic<float>* pPresence; std::atomic<float>* pAir;
    std::atomic<float>* pDsFreq; std::atomic<float>* pDsThresh; std::atomic<float>* pDsRange;
    std::atomic<float>* pDsMode; std::atomic<float>* pDsListen;
    std::atomic<float>* pOutput; std::atomic<float>* pMix;

    vc::Biquad lowCut[2], mud[2], warmth[2], presence[2], air[2];
    vc::Compressor compressor;
    vc::DeEsser deEsser;
    juce::AudioBuffer<float> dryBuffer;
    double sr = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalProcessor)
};
