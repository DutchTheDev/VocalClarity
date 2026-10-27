#include "PluginEditor.h"

namespace
{
const juce::Colour kBg      { 0xff14111c };
const juce::Colour kPanel   { 0xff1e1a2b };
const juce::Colour kAccent  { 0xffe0a458 };   // warm gold
const juce::Colour kAccent2 { 0xffb86fd6 };   // purple
const juce::Colour kText    { 0xffe9e4f2 };
const juce::Colour kDim     { 0xff8d85a1 };
}

VocalEditor::VocalEditor (VocalProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (780, 430);

    setupKnob (lowCut,   "lowcut",    "Low Cut",   " Hz");
    setupKnob (mud,      "mud",       "Mud Cut",   " dB");
    setupKnob (warmth,   "warmth",    "Warmth",    " dB");
    setupKnob (presence, "presence",  "Presence",  " dB");
    setupKnob (air,      "air",       "Air",       " dB");
    setupKnob (comp,     "comp",      "Smooth Comp", " %");
    setupKnob (dsFreq,   "ds_freq",   "Frequency", " Hz");
    setupKnob (dsThresh, "ds_thresh", "Threshold", " dB");
    setupKnob (dsRange,  "ds_range",  "Range",     " dB");
    setupKnob (output,   "output",    "Output",    " dB");
    setupKnob (mix,      "mix",       "Mix",       " %");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    modeBox.addItemList ({ "Split-band", "Wide-band" }, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<ComboAtt> (proc.apvts, "ds_mode", modeBox);

    listenButton.setColour (juce::ToggleButton::textColourId, kText);
    listenButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (listenButton);
    listenAtt = std::make_unique<ButtonAtt> (proc.apvts, "ds_listen", listenButton);

    startTimerHz (30);
}

VocalEditor::~VocalEditor() { stopTimer(); }

void VocalEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& title, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3450));
    k.slider.setColour (juce::Slider::thumbColourId, kText);
    k.slider.setColour (juce::Slider::textBoxTextColourId, kText);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, kDim);
    k.label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void VocalEditor::placeKnob (Knob& k, juce::Rectangle<int> a)
{
    k.label.setBounds (a.removeFromTop (18));
    k.slider.setBounds (a);
}

void VocalEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (44);
    presetBox.setBounds (header.removeFromRight (190).reduced (0, 8));

    r.removeFromTop (6);
    auto row1 = r.removeFromTop (170);
    r.removeFromTop (12);
    auto row2 = r;

    const int kw = 96;
    toneArea = row1.removeFromLeft (kw * 5 + 16);
    row1.removeFromLeft (10);
    compArea = row1.removeFromLeft (kw + 16);
    row1.removeFromLeft (10);
    outArea = row1;

    auto t = toneArea.reduced (8, 26);
    for (auto* k : { &lowCut, &mud, &warmth, &presence, &air })
        placeKnob (*k, t.removeFromLeft (kw));
    placeKnob (comp, compArea.reduced (8, 26));
    auto o = outArea.reduced (8, 26);
    placeKnob (output, o.removeFromLeft (o.getWidth() / 2));
    placeKnob (mix, o);

    dsArea = row2.removeFromLeft (kw * 3 + 16 + 170);
    row2.removeFromLeft (10);
    meterArea = row2;

    auto d = dsArea.reduced (8, 26);
    placeKnob (dsFreq,   d.removeFromLeft (kw));
    placeKnob (dsThresh, d.removeFromLeft (kw));
    placeKnob (dsRange,  d.removeFromLeft (kw));
    d.removeFromLeft (10);
    modeBox.setBounds (d.removeFromTop (28).reduced (0, 2));
    d.removeFromTop (14);
    listenButton.setBounds (d.removeFromTop (28));
}

void VocalEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float grDb, const juce::String& name)
{
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (name, r.removeFromTop (16.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff0f0c16));
    g.fillRoundedRectangle (r, 4.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, grDb / 15.0f);
    g.setColour (kAccent2);
    g.fillRoundedRectangle (r.withWidth (r.getWidth() * frac), 4.0f);
    g.setColour (kText);
    g.drawText (juce::String (grDb, 1) + " dB", r.reduced (6, 0), juce::Justification::centredRight);
}

void VocalEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kAccent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOCAL CLARITY", 20, 12, 300, 32, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("De-esser + vocal mixer for smooth R&B", 232, 14, 360, 32, juce::Justification::centredLeft);

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& title)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (kAccent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, a.getX() + 12, a.getY() + 6, 200, 16, juce::Justification::centredLeft);
    };
    panel (toneArea, "TONE");
    panel (compArea, "LEVEL");
    panel (outArea, "OUTPUT");
    panel (dsArea, "DE-ESSER");
    panel (meterArea, "GAIN REDUCTION");

    auto m = meterArea.reduced (14, 30).toFloat();
    drawMeter (g, m.removeFromTop (46.0f), shownDsGr, "De-esser");
    m.removeFromTop (12.0f);
    drawMeter (g, m.removeFromTop (46.0f), shownCompGr, "Compressor");
}

void VocalEditor::timerCallback()
{
    auto follow = [] (float& shown, float target)
    {
        shown = target > shown ? target : shown * 0.85f + target * 0.15f;
    };
    follow (shownDsGr, proc.deEssGr.load());
    follow (shownCompGr, proc.compGr.load());
    repaint (meterArea);
}
