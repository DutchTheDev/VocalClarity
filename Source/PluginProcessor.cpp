#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* kIds[] = { "lowcut", "mud", "warmth", "comp", "presence", "air",
                       "ds_freq", "ds_thresh", "ds_range", "ds_mode", "output", "mix" };

struct Preset { const char* name; float v[12]; };
//                                lowcut mud  warm comp pres air  dsFrq dsThr dsRng mode out mix
const Preset kPresets[] = {
    { "Smooth R&B",          {  90, -2.0f,  1.0f, 40, 1.5f, 2.5f, 6500, -32,  8, 0, 0, 100 } },
    { "Airy Falsetto",       { 120, -1.5f, -1.0f, 35, 1.0f, 5.0f, 7500, -34, 10, 0, 0, 100 } },
    { "Warm Intimate",       {  80, -3.0f,  3.0f, 50, 0.5f, 1.5f, 6000, -30,  9, 0, 0, 100 } },
    { "Bright Pop R&B",      { 100, -2.0f,  0.0f, 45, 3.0f, 4.0f, 7000, -34, 10, 0, 0, 100 } },
    { "Harsh Vocal Rescue",  { 110, -3.0f,  0.0f, 45, 0.0f, 1.0f, 5800, -38, 12, 0, 0, 100 } },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalProcessor::createLayout()
{
    using APF = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<APF> (id ("lowcut"),   "Low Cut",  Range (20.f, 250.f, 1.f, 0.5f), 90.f,  Attr().withLabel ("Hz")));
    l.add (std::make_unique<APF> (id ("mud"),      "Mud Cut",  Range (-8.f, 0.f, 0.1f),        -2.f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("warmth"),   "Warmth",   Range (-6.f, 6.f, 0.1f),         1.f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("comp"),     "Smooth Comp", Range (0.f, 100.f, 1.f),     40.f,  Attr().withLabel ("%")));
    l.add (std::make_unique<APF> (id ("presence"), "Presence", Range (-6.f, 6.f, 0.1f),        1.5f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("air"),      "Air",      Range (0.f, 8.f, 0.1f),         2.5f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("ds_freq"),  "De-Ess Freq", Range (3000.f, 12000.f, 10.f, 0.6f), 6500.f, Attr().withLabel ("Hz")));
    l.add (std::make_unique<APF> (id ("ds_thresh"), "De-Ess Threshold", Range (-60.f, -10.f, 0.5f), -32.f, Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("ds_range"), "De-Ess Range", Range (0.f, 15.f, 0.5f),    8.f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterChoice> (id ("ds_mode"), "De-Ess Mode",
                                                         juce::StringArray { "Split-band", "Wide-band" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (id ("ds_listen"), "De-Ess Listen", false));
    l.add (std::make_unique<APF> (id ("output"),   "Output",   Range (-12.f, 12.f, 0.1f),       0.f,  Attr().withLabel ("dB")));
    l.add (std::make_unique<APF> (id ("mix"),      "Mix",      Range (0.f, 100.f, 1.f),       100.f,  Attr().withLabel ("%")));
    return l;
}

VocalProcessor::VocalProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto p = [this] (const char* s) { return apvts.getRawParameterValue (s); };
    pLowCut = p ("lowcut"); pMud = p ("mud"); pWarmth = p ("warmth"); pComp = p ("comp");
    pPresence = p ("presence"); pAir = p ("air");
    pDsFreq = p ("ds_freq"); pDsThresh = p ("ds_thresh"); pDsRange = p ("ds_range");
    pDsMode = p ("ds_mode"); pDsListen = p ("ds_listen");
    pOutput = p ("output"); pMix = p ("mix");
}

bool VocalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void VocalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        lowCut[c].reset(); mud[c].reset(); warmth[c].reset(); presence[c].reset(); air[c].reset();
    }
    compressor.prepare (sr);
    deEsser.prepare (sr);
    dryBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
}

void VocalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0)
        return;

    dryBuffer.setSize (numCh, n, false, false, true);
    for (int c = 0; c < numCh; ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, n);

    // ---- update coefficients once per block
    const double nyq = 0.45 * sr;
    const auto hpC   = vc::coeffs::highPass (sr, pLowCut->load(), 0.707);
    const auto mudC  = vc::coeffs::peak (sr, 300.0, 1.0, pMud->load());
    const auto warmC = vc::coeffs::lowShelf (sr, 180.0, pWarmth->load());
    const auto presC = vc::coeffs::peak (sr, 3200.0, 0.7, pPresence->load());
    const auto airC  = vc::coeffs::highShelf (sr, std::min (11000.0, nyq), pAir->load());
    for (int c = 0; c < numCh; ++c)
    {
        lowCut[c].set (hpC); mud[c].set (mudC); warmth[c].set (warmC);
        presence[c].set (presC); air[c].set (airC);
    }
    compressor.setAmount (pComp->load() / 100.0);
    deEsser.setFrequency (pDsFreq->load());
    const bool listen = pDsListen->load() > 0.5f;
    deEsser.setParams (pDsThresh->load(), pDsRange->load(), pDsMode->load() > 0.5f, listen);

    float* const* data = buffer.getArrayOfWritePointers();

    // ---- stage 1: low cut, mud cut, warmth
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
            data[c][i] = (float) warmth[c].process (mud[c].process (lowCut[c].process ((double) data[c][i])));

    // ---- stage 2: smooth compression
    compGr.store ((float) compressor.process (data, numCh, n));

    // ---- stage 3: presence + air
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
            data[c][i] = (float) air[c].process (presence[c].process ((double) data[c][i]));

    // ---- stage 4: de-esser (last, so it catches sibilance added by presence/air)
    deEsser.setParams (pDsThresh->load(), pDsRange->load(), pDsMode->load() > 0.5f, listen);
    deEssGr.store ((float) deEsser.process (data, numCh, n));

    // ---- output gain and dry/wet mix (listen mode always plays the sidechain band)
    const float outGain = juce::Decibels::decibelsToGain (pOutput->load());
    const float wet = listen ? 1.0f : pMix->load() / 100.0f;
    for (int c = 0; c < numCh; ++c)
    {
        auto* w = buffer.getWritePointer (c);
        const auto* d = dryBuffer.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            w[i] = (w[i] * outGain) * wet + d[i] * (1.0f - wet);
    }
}

int VocalProcessor::getNumPrograms() { return kNumPresets; }
const juce::String VocalProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? kPresets[index].name : juce::String();
}

void VocalProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumPresets))
        return;
    currentProgram = index;
    for (int i = 0; i < 12; ++i)
        if (auto* p = apvts.getParameter (kIds[i]))
            p->setValueNotifyingHost (p->convertTo0to1 (kPresets[index].v[i]));
}

void VocalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocalProcessor::createEditor() { return new VocalEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalProcessor(); }
