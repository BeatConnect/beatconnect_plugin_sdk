/*
  ==============================================================================
    BeccaToneAmp - Audio Processor Implementation
    Becca's signature guitar tone in a box
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

#if HAS_PROJECT_DATA
#include "ProjectData.h"
#endif

#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

//==============================================================================
BeccaToneAmpProcessor::BeccaToneAmpProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    loadProjectData();

    // Initialize waveshaper for tube-like saturation
    waveshaper.functionToUse = [](float x) {
        // Soft saturation curve (warm tube-like response)
        return std::tanh(x * 1.5f) * 0.9f;
    };

    // Initialize distortion waveshaper (more aggressive)
    distortionShaper.functionToUse = [](float x) {
        // Asymmetric soft clipping for fuzz character
        if (x > 0.0f)
            return 1.0f - std::exp(-x * 2.0f);
        else
            return -1.0f + std::exp(x * 1.8f);
    };
}

BeccaToneAmpProcessor::~BeccaToneAmpProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BeccaToneAmpProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ===========================================================================
    // Amp Parameters
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::gain, 1 },
        "Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f  // Slight edge-of-breakup warmth by default
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::bass, 1 },
        "Bass",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.6f  // Warm, thick lows
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::mid, 1 },
        "Mid",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.55f  // Full mids for presence
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::treble, 1 },
        "Treble",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.4f  // Smooth highs, not harsh
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::presence, 1 },
        "Presence",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::level, 1 },
        "Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::volume, 1 },
        "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.75f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::compression, 1 },
        "Compression",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.2f  // Subtle by default
    ));

    // ===========================================================================
    // Chorus Pedal
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::chorusEnabled, 1 },
        "Chorus",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::chorusRate, 1 },
        "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f, 0.5f),
        1.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::chorusDepth, 1 },
        "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::chorusMix, 1 },
        "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    // ===========================================================================
    // Tremolo Pedal
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::tremoloEnabled, 1 },
        "Tremolo",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::tremoloRate, 1 },
        "Tremolo Rate",
        juce::NormalisableRange<float>(0.5f, 10.0f, 0.01f, 0.5f),
        4.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::tremoloDepth, 1 },
        "Tremolo Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    // ===========================================================================
    // Delay Pedal
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::delayEnabled, 1 },
        "Delay",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayTime, 1 },
        "Delay Time",
        juce::NormalisableRange<float>(50.0f, 1000.0f, 1.0f, 0.5f),
        300.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayFeedback, 1 },
        "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.9f, 0.01f),
        0.35f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayMix, 1 },
        "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f
    ));

    // ===========================================================================
    // Reverb Pedal
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::reverbEnabled, 1 },
        "Reverb",
        true  // Slight reverb on by default (Becca signature)
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::reverbSize, 1 },
        "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.4f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::reverbDamping, 1 },
        "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::reverbMix, 1 },
        "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.25f
    ));

    // ===========================================================================
    // Distortion/Fuzz Pedal
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::distortionEnabled, 1 },
        "Distortion",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::distortionDrive, 1 },
        "Distortion Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::distortionTone, 1 },
        "Distortion Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::distortionLevel, 1 },
        "Distortion Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f
    ));

    // ===========================================================================
    // Global
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::preset, 1 },
        "Preset",
        PresetNames::presets,
        0  // Becca Signature Tone
    ));

    // ===========================================================================
    // Advanced Controls (Expert Mode)
    // ===========================================================================
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::expertMode, 1 },
        "Expert Mode",
        false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::micDistance, 1 },
        "Mic Distance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f  // Closer = brighter
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::cabStyle, 1 },
        "Cabinet Style",
        CabStyles::styles,
        0  // Vintage
    ));

    // 8-band EQ
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq60Hz, 1 }, "EQ 60Hz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq120Hz, 1 }, "EQ 120Hz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq250Hz, 1 }, "EQ 250Hz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq500Hz, 1 }, "EQ 500Hz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq1kHz, 1 }, "EQ 1kHz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq2kHz, 1 }, "EQ 2kHz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq4kHz, 1 }, "EQ 4kHz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::eq8kHz, 1 }, "EQ 8kHz",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String BeccaToneAmpProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BeccaToneAmpProcessor::acceptsMidi() const { return false; }
bool BeccaToneAmpProcessor::producesMidi() const { return false; }
bool BeccaToneAmpProcessor::isMidiEffect() const { return false; }
double BeccaToneAmpProcessor::getTailLengthSeconds() const { return 2.0; } // Reverb/delay tail

int BeccaToneAmpProcessor::getNumPrograms() { return static_cast<int>(PresetNames::presets.size()); }
int BeccaToneAmpProcessor::getCurrentProgram()
{
    return static_cast<int>(apvts.getRawParameterValue(ParamIDs::preset)->load());
}
void BeccaToneAmpProcessor::setCurrentProgram(int index)
{
    apvts.getParameter(ParamIDs::preset)->setValueNotifyingHost(
        static_cast<float>(index) / static_cast<float>(PresetNames::presets.size() - 1));
    applyPreset(index);
}
const juce::String BeccaToneAmpProcessor::getProgramName(int index)
{
    if (index >= 0 && index < PresetNames::presets.size())
        return PresetNames::presets[index];
    return {};
}
void BeccaToneAmpProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void BeccaToneAmpProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSpec.sampleRate = sampleRate;
    currentSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    currentSpec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize smoothed values
    smoothedGain.reset(sampleRate, 0.02);
    smoothedBass.reset(sampleRate, 0.02);
    smoothedMid.reset(sampleRate, 0.02);
    smoothedTreble.reset(sampleRate, 0.02);
    smoothedPresence.reset(sampleRate, 0.02);
    smoothedLevel.reset(sampleRate, 0.02);
    smoothedVolume.reset(sampleRate, 0.02);
    smoothedCompression.reset(sampleRate, 0.02);

    // Prepare DSP components
    inputGain.prepare(currentSpec);
    outputGain.prepare(currentSpec);

    // Compressor
    compressor.prepare(currentSpec);
    compressor.setAttack(10.0f);
    compressor.setRelease(100.0f);
    compressor.setRatio(4.0f);
    compressor.setThreshold(-20.0f);

    // Chorus
    chorus.prepare(currentSpec);
    chorus.setCentreDelay(7.0f);

    // Delay
    delayLine.prepare(currentSpec);
    delayLine.setMaximumDelayInSamples(static_cast<int>(sampleRate * 2.0));

    // Reverb
    reverb.prepare(currentSpec);

    // Initialize EQ filters
    updateFilters();

    // Prepare advanced EQ
    for (auto& filter : eqFiltersL)
        filter.prepare(currentSpec);
    for (auto& filter : eqFiltersR)
        filter.prepare(currentSpec);
}

void BeccaToneAmpProcessor::releaseResources()
{
    delayLine.reset();
    reverb.reset();
    chorus.reset();
}

bool BeccaToneAmpProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void BeccaToneAmpProcessor::updateFilters()
{
    auto sampleRate = currentSpec.sampleRate;
    if (sampleRate <= 0.0) return;

    // Get current parameter values
    float bassVal = apvts.getRawParameterValue(ParamIDs::bass)->load();
    float midVal = apvts.getRawParameterValue(ParamIDs::mid)->load();
    float trebleVal = apvts.getRawParameterValue(ParamIDs::treble)->load();
    float presenceVal = apvts.getRawParameterValue(ParamIDs::presence)->load();

    // Convert 0-1 to dB gain (-12 to +12)
    float bassGain = (bassVal - 0.5f) * 24.0f;
    float midGain = (midVal - 0.5f) * 24.0f;
    float trebleGain = (trebleVal - 0.5f) * 24.0f;
    float presenceGain = (presenceVal - 0.5f) * 24.0f;

    // Bass: Low shelf at 200 Hz
    auto bassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, 200.0f, 0.707f, juce::Decibels::decibelsToGain(bassGain));
    *bassFilterL.coefficients = *bassCoeffs;
    *bassFilterR.coefficients = *bassCoeffs;

    // Mid: Peak at 800 Hz
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 800.0f, 1.0f, juce::Decibels::decibelsToGain(midGain));
    *midFilterL.coefficients = *midCoeffs;
    *midFilterR.coefficients = *midCoeffs;

    // Treble: High shelf at 3 kHz
    auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain(trebleGain));
    *trebleFilterL.coefficients = *trebleCoeffs;
    *trebleFilterR.coefficients = *trebleCoeffs;

    // Presence: Peak at 4.5 kHz
    auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 4500.0f, 1.5f, juce::Decibels::decibelsToGain(presenceGain));
    *presenceFilterL.coefficients = *presenceCoeffs;
    *presenceFilterR.coefficients = *presenceCoeffs;
}

float BeccaToneAmpProcessor::softClip(float x)
{
    // Warm soft clipping
    if (x > 1.0f)
        return 1.0f - std::exp(1.0f - x);
    else if (x < -1.0f)
        return -1.0f + std::exp(-1.0f - x);
    return x;
}

void BeccaToneAmpProcessor::processAmpStage(juce::AudioBuffer<float>& buffer)
{
    auto gainVal = apvts.getRawParameterValue(ParamIDs::gain)->load();
    auto compressionVal = apvts.getRawParameterValue(ParamIDs::compression)->load();
    auto levelVal = apvts.getRawParameterValue(ParamIDs::level)->load();
    auto volumeVal = apvts.getRawParameterValue(ParamIDs::volume)->load();

    smoothedGain.setTargetValue(gainVal);
    smoothedCompression.setTargetValue(compressionVal);
    smoothedLevel.setTargetValue(levelVal);
    smoothedVolume.setTargetValue(volumeVal);

    // Update compressor based on compression amount
    float compThreshold = -30.0f + (1.0f - compressionVal) * 20.0f; // -30 to -10 dB
    compressor.setThreshold(compThreshold);
    compressor.setRatio(1.0f + compressionVal * 7.0f); // 1:1 to 8:1

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // Update EQ filters
    updateFilters();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentGain = smoothedGain.getNextValue();
        float currentLevel = smoothedLevel.getNextValue();
        float currentVolume = smoothedVolume.getNextValue();

        // Gain drives the saturation amount (0 = clean, 1 = saturated)
        float driveAmount = 1.0f + currentGain * 4.0f; // 1x to 5x gain into saturation

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            float input = channelData[sample];

            // Apply input gain (drive)
            float driven = input * driveAmount;

            // Soft saturation (tube-like warmth)
            driven = softClip(driven);

            // Apply EQ (3-band + presence)
            if (channel == 0)
            {
                driven = bassFilterL.processSample(driven);
                driven = midFilterL.processSample(driven);
                driven = trebleFilterL.processSample(driven);
                driven = presenceFilterL.processSample(driven);
            }
            else
            {
                driven = bassFilterR.processSample(driven);
                driven = midFilterR.processSample(driven);
                driven = trebleFilterR.processSample(driven);
                driven = presenceFilterR.processSample(driven);
            }

            // Apply level and volume
            channelData[sample] = driven * currentLevel * currentVolume;
        }
    }

    // Apply compression and track gain reduction
    if (compressionVal > 0.01f)
    {
        // Measure level before compression
        float preCompLevel = buffer.getMagnitude(0, 0, buffer.getNumSamples());

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        compressor.process(context);

        // Measure level after compression
        float postCompLevel = buffer.getMagnitude(0, 0, buffer.getNumSamples());

        // Calculate gain reduction in dB
        if (preCompLevel > 0.0001f && postCompLevel > 0.0001f)
        {
            float gr = juce::Decibels::gainToDecibels(postCompLevel / preCompLevel);
            // Clamp to reasonable range (negative values = reduction)
            gr = juce::jlimit(-24.0f, 0.0f, gr);
            gainReduction.store(-gr); // Store as positive value
        }
    }
    else
    {
        gainReduction.store(0.0f);
    }
}

void BeccaToneAmpProcessor::processDistortion(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::distortionEnabled)->load() < 0.5f)
        return;

    auto drive = apvts.getRawParameterValue(ParamIDs::distortionDrive)->load();
    auto tone = apvts.getRawParameterValue(ParamIDs::distortionTone)->load();
    auto level = apvts.getRawParameterValue(ParamIDs::distortionLevel)->load();

    auto sampleRate = currentSpec.sampleRate;

    // Tone control: low-pass filter
    float toneFreq = 500.0f + tone * 4500.0f; // 500 Hz to 5 kHz
    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneFreq);
    *distortionToneFilterL.coefficients = *toneCoeffs;
    *distortionToneFilterR.coefficients = *toneCoeffs;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // Apply drive
            float driven = input * (1.0f + drive * 10.0f);

            // Fuzz-style distortion
            if (driven > 0.0f)
                driven = 1.0f - std::exp(-driven * 2.0f);
            else
                driven = -1.0f + std::exp(driven * 1.8f);

            // Tone filter
            if (channel == 0)
                driven = distortionToneFilterL.processSample(driven);
            else
                driven = distortionToneFilterR.processSample(driven);

            channelData[sample] = driven * level;
        }
    }
}

void BeccaToneAmpProcessor::processChorus(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::chorusEnabled)->load() < 0.5f)
        return;

    auto rate = apvts.getRawParameterValue(ParamIDs::chorusRate)->load();
    auto depth = apvts.getRawParameterValue(ParamIDs::chorusDepth)->load();
    auto mix = apvts.getRawParameterValue(ParamIDs::chorusMix)->load();

    chorus.setRate(rate);
    chorus.setDepth(depth);
    chorus.setMix(mix);
    chorus.setFeedback(-0.2f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    chorus.process(context);
}

void BeccaToneAmpProcessor::processTremolo(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::tremoloEnabled)->load() < 0.5f)
        return;

    auto rate = apvts.getRawParameterValue(ParamIDs::tremoloRate)->load();
    auto depth = apvts.getRawParameterValue(ParamIDs::tremoloDepth)->load();

    float phaseIncrement = rate / static_cast<float>(currentSpec.sampleRate);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Sine wave LFO
        float lfo = 0.5f + 0.5f * std::sin(tremoloPhase * juce::MathConstants<float>::twoPi);
        float gain = 1.0f - depth * (1.0f - lfo);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.getWritePointer(channel)[sample] *= gain;
        }

        tremoloPhase += phaseIncrement;
        if (tremoloPhase >= 1.0f)
            tremoloPhase -= 1.0f;
    }
}

void BeccaToneAmpProcessor::processDelay(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::delayEnabled)->load() < 0.5f)
        return;

    auto time = apvts.getRawParameterValue(ParamIDs::delayTime)->load();
    auto feedback = apvts.getRawParameterValue(ParamIDs::delayFeedback)->load();
    auto mix = apvts.getRawParameterValue(ParamIDs::delayMix)->load();

    float delaySamples = time * 0.001f * static_cast<float>(currentSpec.sampleRate);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Smooth delay time changes
        delaySmoothedTime += (delaySamples - delaySmoothedTime) * 0.001f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            float input = channelData[sample];

            float delayed = delayLine.popSample(channel, delaySmoothedTime);
            delayLine.pushSample(channel, input + delayed * feedback);

            channelData[sample] = input * (1.0f - mix) + delayed * mix;
        }
    }
}

void BeccaToneAmpProcessor::processReverb(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::reverbEnabled)->load() < 0.5f)
        return;

    auto size = apvts.getRawParameterValue(ParamIDs::reverbSize)->load();
    auto damping = apvts.getRawParameterValue(ParamIDs::reverbDamping)->load();
    auto mix = apvts.getRawParameterValue(ParamIDs::reverbMix)->load();

    reverbParams.roomSize = size;
    reverbParams.damping = damping;
    reverbParams.wetLevel = mix;
    reverbParams.dryLevel = 1.0f - mix * 0.5f;
    reverbParams.width = 1.0f;
    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);
}

void BeccaToneAmpProcessor::processAdvancedEQ(juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue(ParamIDs::expertMode)->load() < 0.5f)
        return;

    auto sampleRate = currentSpec.sampleRate;

    // Get EQ values and update filters
    const char* eqParams[] = {
        ParamIDs::eq60Hz, ParamIDs::eq120Hz, ParamIDs::eq250Hz, ParamIDs::eq500Hz,
        ParamIDs::eq1kHz, ParamIDs::eq2kHz, ParamIDs::eq4kHz, ParamIDs::eq8kHz
    };

    for (int i = 0; i < 8; ++i)
    {
        float gain = apvts.getRawParameterValue(eqParams[i])->load();
        if (std::abs(gain) > 0.1f)
        {
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, eqFrequencies[i], 1.0f, juce::Decibels::decibelsToGain(gain));
            *eqFiltersL[i].coefficients = *coeffs;
            *eqFiltersR[i].coefficients = *coeffs;
        }
    }

    // Process through EQ filters
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int i = 0; i < 8; ++i)
        {
            float gain = apvts.getRawParameterValue(eqParams[i])->load();
            if (std::abs(gain) > 0.1f)
            {
                if (buffer.getNumChannels() > 0)
                {
                    float* left = buffer.getWritePointer(0);
                    left[sample] = eqFiltersL[i].processSample(left[sample]);
                }
                if (buffer.getNumChannels() > 1)
                {
                    float* right = buffer.getWritePointer(1);
                    right[sample] = eqFiltersR[i].processSample(right[sample]);
                }
            }
        }
    }
}

void BeccaToneAmpProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Check if preset changed (from WebUI or automation)
    int currentPreset = static_cast<int>(apvts.getRawParameterValue(ParamIDs::preset)->load());
    if (currentPreset != lastAppliedPreset)
    {
        applyPreset(currentPreset);
    }

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Calculate input level
    float inLevel = 0.0f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        inLevel = std::max(inLevel, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    inputLevel.store(inLevel);

    // Check bypass
    if (apvts.getRawParameterValue(ParamIDs::bypass)->load() > 0.5f)
    {
        outputLevel.store(inLevel);
        return;
    }

    // Signal chain:
    // 1. Distortion pedal (before amp)
    processDistortion(buffer);

    // 2. Amp stage (gain, EQ, compression)
    processAmpStage(buffer);

    // 3. Advanced EQ (if expert mode)
    processAdvancedEQ(buffer);

    // 4. Modulation effects
    processChorus(buffer);
    processTremolo(buffer);

    // 5. Time-based effects
    processDelay(buffer);
    processReverb(buffer);

    // Calculate output level
    float outLevel = 0.0f;
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        outLevel = std::max(outLevel, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    outputLevel.store(outLevel);

    // Push samples to FFT for spectrum analysis (use left channel or mono mix)
    if (totalNumOutputChannels > 0)
    {
        pushSamplesToFFT(buffer.getReadPointer(0), buffer.getNumSamples());
    }
}

//==============================================================================
void BeccaToneAmpProcessor::applyPreset(int presetIndex)
{
    // Don't re-apply if same preset
    if (presetIndex == lastAppliedPreset)
        return;

    lastAppliedPreset = presetIndex;

    // Helper to set parameter value
    auto setParam = [this](const char* paramId, float value) {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(value);
    };

    // Helper to set boolean parameter
    auto setBoolParam = [this](const char* paramId, bool value) {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(value ? 1.0f : 0.0f);
    };

    switch (presetIndex)
    {
        case 0: // Becca Signature Tone
            setParam(ParamIDs::gain, 0.35f);
            setParam(ParamIDs::bass, 0.6f);
            setParam(ParamIDs::mid, 0.55f);
            setParam(ParamIDs::treble, 0.4f);
            setParam(ParamIDs::presence, 0.5f);
            setParam(ParamIDs::compression, 0.2f);
            setParam(ParamIDs::level, 0.7f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, false);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, false);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.4f);
            setParam(ParamIDs::reverbMix, 0.25f);
            break;

        case 1: // Pristine Clean
            setParam(ParamIDs::gain, 0.15f);
            setParam(ParamIDs::bass, 0.5f);
            setParam(ParamIDs::mid, 0.5f);
            setParam(ParamIDs::treble, 0.5f);
            setParam(ParamIDs::presence, 0.45f);
            setParam(ParamIDs::compression, 0.0f);
            setParam(ParamIDs::level, 0.8f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, false);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, false);
            setBoolParam(ParamIDs::reverbEnabled, false);
            break;

        case 2: // Super Clean
            setParam(ParamIDs::gain, 0.2f);
            setParam(ParamIDs::bass, 0.55f);
            setParam(ParamIDs::mid, 0.5f);
            setParam(ParamIDs::treble, 0.55f);
            setParam(ParamIDs::presence, 0.6f);
            setParam(ParamIDs::compression, 0.3f);
            setParam(ParamIDs::level, 0.75f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, false);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, false);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.3f);
            setParam(ParamIDs::reverbMix, 0.15f);
            break;

        case 3: // Overdriven / Fuzzy
            setParam(ParamIDs::gain, 0.6f);
            setParam(ParamIDs::bass, 0.65f);
            setParam(ParamIDs::mid, 0.6f);
            setParam(ParamIDs::treble, 0.45f);
            setParam(ParamIDs::presence, 0.55f);
            setParam(ParamIDs::compression, 0.15f);
            setParam(ParamIDs::level, 0.65f);
            setParam(ParamIDs::volume, 0.7f);
            setBoolParam(ParamIDs::distortionEnabled, true);
            setParam(ParamIDs::distortionDrive, 0.6f);
            setParam(ParamIDs::distortionTone, 0.55f);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, false);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.35f);
            setParam(ParamIDs::reverbMix, 0.2f);
            break;

        case 4: // 80s Dream
            setParam(ParamIDs::gain, 0.25f);
            setParam(ParamIDs::bass, 0.5f);
            setParam(ParamIDs::mid, 0.45f);
            setParam(ParamIDs::treble, 0.55f);
            setParam(ParamIDs::presence, 0.5f);
            setParam(ParamIDs::compression, 0.25f);
            setParam(ParamIDs::level, 0.7f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, false);
            setBoolParam(ParamIDs::chorusEnabled, true);
            setParam(ParamIDs::chorusRate, 0.3f);
            setParam(ParamIDs::chorusDepth, 0.5f);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, true);
            setParam(ParamIDs::delayTime, 0.35f);
            setParam(ParamIDs::delayMix, 0.25f);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.6f);
            setParam(ParamIDs::reverbMix, 0.4f);
            break;

        case 5: // Funk Groove
            setParam(ParamIDs::gain, 0.3f);
            setParam(ParamIDs::bass, 0.55f);
            setParam(ParamIDs::mid, 0.65f);
            setParam(ParamIDs::treble, 0.6f);
            setParam(ParamIDs::presence, 0.55f);
            setParam(ParamIDs::compression, 0.5f);
            setParam(ParamIDs::level, 0.75f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, false);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, false);
            setBoolParam(ParamIDs::delayEnabled, false);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.25f);
            setParam(ParamIDs::reverbMix, 0.12f);
            break;

        case 6: // Indie Grit
            setParam(ParamIDs::gain, 0.45f);
            setParam(ParamIDs::bass, 0.55f);
            setParam(ParamIDs::mid, 0.5f);
            setParam(ParamIDs::treble, 0.4f);
            setParam(ParamIDs::presence, 0.45f);
            setParam(ParamIDs::compression, 0.2f);
            setParam(ParamIDs::level, 0.7f);
            setParam(ParamIDs::volume, 0.75f);
            setBoolParam(ParamIDs::distortionEnabled, true);
            setParam(ParamIDs::distortionDrive, 0.35f);
            setParam(ParamIDs::distortionTone, 0.4f);
            setBoolParam(ParamIDs::chorusEnabled, false);
            setBoolParam(ParamIDs::tremoloEnabled, true);
            setParam(ParamIDs::tremoloRate, 0.2f);
            setParam(ParamIDs::tremoloDepth, 0.3f);
            setBoolParam(ParamIDs::delayEnabled, true);
            setParam(ParamIDs::delayTime, 0.4f);
            setParam(ParamIDs::delayMix, 0.2f);
            setBoolParam(ParamIDs::reverbEnabled, true);
            setParam(ParamIDs::reverbSize, 0.5f);
            setParam(ParamIDs::reverbMix, 0.3f);
            break;

        default:
            break;
    }
}

//==============================================================================
bool BeccaToneAmpProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* BeccaToneAmpProcessor::createEditor()
{
    return new BeccaToneAmpEditor(*this);
}

//==============================================================================
void BeccaToneAmpProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BeccaToneAmpProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
bool BeccaToneAmpProcessor::hasActivationEnabled() const
{
#if HAS_PROJECT_DATA && BEATCONNECT_ACTIVATION_ENABLED
    return static_cast<bool>(buildFlags_.getProperty("enableActivationKeys", false));
#else
    return false;
#endif
}

void BeccaToneAmpProcessor::loadProjectData()
{
#if HAS_PROJECT_DATA
    int dataSize = 0;
    const char* data = ProjectData::getNamedResource("project_data_json", dataSize);

    if (data == nullptr || dataSize == 0)
        return;

    auto jsonString = juce::String::fromUTF8(data, dataSize);
    auto parsed = juce::JSON::parse(jsonString);

    if (parsed.isVoid())
        return;

    pluginId_ = parsed.getProperty("pluginId", "").toString();
    apiBaseUrl_ = parsed.getProperty("apiBaseUrl", "").toString();
    supabasePublishableKey_ = parsed.getProperty("supabasePublishableKey", "").toString();
    buildFlags_ = parsed.getProperty("flags", juce::var());

#if BEATCONNECT_ACTIVATION_ENABLED
    bool enableActivation = static_cast<bool>(buildFlags_.getProperty("enableActivationKeys", false));
    if (enableActivation && pluginId_.isNotEmpty())
    {
        beatconnect::ActivationConfig config;
        config.apiBaseUrl = apiBaseUrl_.toStdString();
        config.pluginId = pluginId_.toStdString();
        config.supabaseKey = supabasePublishableKey_.toStdString();
        beatconnect::Activation::getInstance().configure(config);
    }
#endif
#endif
}

//==============================================================================
// FFT Spectrum Analyzer
//==============================================================================
void BeccaToneAmpProcessor::pushSamplesToFFT(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        fftFifo[fftFifoIndex++] = samples[i];

        if (fftFifoIndex == FFT_SIZE)
        {
            // Copy to FFT data and apply window
            std::copy(fftFifo.begin(), fftFifo.end(), fftData.begin());
            fftWindow.multiplyWithWindowingTable(fftData.data(), FFT_SIZE);

            // Perform FFT
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            // Convert to magnitudes (in dB)
            for (int bin = 0; bin < NUM_SPECTRUM_BINS; ++bin)
            {
                float magnitude = fftData[bin];
                // Convert to dB with floor at -100dB
                float db = juce::jlimit(-100.0f, 0.0f,
                    juce::Decibels::gainToDecibels(magnitude / static_cast<float>(FFT_SIZE)));
                // Normalize to 0-1 range
                spectrumMagnitudes[bin] = (db + 100.0f) / 100.0f;
            }

            fftDataReady.store(true);
            fftFifoIndex = 0;
        }
    }
}

void BeccaToneAmpProcessor::getSpectrum(float* magnitudes, int numBins) const
{
    int binsToUse = std::min(numBins, NUM_SPECTRUM_BINS);
    for (int i = 0; i < binsToUse; ++i)
    {
        magnitudes[i] = spectrumMagnitudes[i];
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeccaToneAmpProcessor();
}
