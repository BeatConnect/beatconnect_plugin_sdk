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
    // Pedal Slots (signal chain order)
    // Not automatable to avoid glitches during playback
    // ===========================================================================
    auto slotAttributes = juce::AudioParameterChoiceAttributes().withAutomatable(false);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::pedalSlot0, 1 },
        "Pedal Slot 1", PedalSlots::pedalIds, 0, slotAttributes));  // distortion

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::pedalSlot1, 1 },
        "Pedal Slot 2", PedalSlots::pedalIds, 1, slotAttributes));  // chorus

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::pedalSlot2, 1 },
        "Pedal Slot 3", PedalSlots::pedalIds, 2, slotAttributes));  // tremolo

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::pedalSlot3, 1 },
        "Pedal Slot 4", PedalSlots::pedalIds, 3, slotAttributes));  // delay

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::pedalSlot4, 1 },
        "Pedal Slot 5", PedalSlots::pedalIds, 4, slotAttributes));  // reverb

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

    // FDN Reverb initialization
    float srScale = static_cast<float>(sampleRate) / 44100.0f;

    // Initialize FDN delay lines
    int maxFDNSize = static_cast<int>(100.0f * sampleRate / 1000.0f) + 512; // 100ms max
    for (int i = 0; i < kFDNSize; ++i)
    {
        fdnDelayLines[i].setSize(1, maxFDNSize);
        fdnDelayLines[i].clear();
        fdnWritePos[i] = 0;
        fdnDelaySamples[i] = static_cast<int>(kFDNDelayMs[i] * sampleRate / 1000.0f);
        fdnFilterState[i] = 0.0f;
    }

    // Early reflections buffer
    static const float earlyTapTimesL[] = { 1.9f, 4.3f, 6.1f, 8.7f, 11.3f, 14.7f, 18.1f, 22.9f, 27.3f, 33.1f, 39.7f, 47.3f };
    static const float earlyTapTimesR[] = { 2.7f, 5.1f, 7.3f, 9.9f, 12.7f, 16.1f, 20.3f, 25.1f, 30.7f, 36.3f, 43.1f, 51.7f };
    static const float earlyTapGainsLInit[] = { 0.82f, 0.71f, 0.67f, 0.59f, 0.53f, 0.47f, 0.41f, 0.35f, 0.29f, 0.23f, 0.18f, 0.13f };
    static const float earlyTapGainsRInit[] = { 0.79f, 0.73f, 0.64f, 0.57f, 0.51f, 0.44f, 0.38f, 0.32f, 0.26f, 0.21f, 0.16f, 0.11f };

    int earlySize = static_cast<int>(60.0 * sampleRate / 1000.0) + 512;
    earlyReflectionBuffer.setSize(2, earlySize);
    earlyReflectionBuffer.clear();
    earlyWritePos = 0;

    for (int i = 0; i < kNumEarlyTaps; ++i)
    {
        earlyTapDelaysL[i] = static_cast<int>(earlyTapTimesL[i] * sampleRate / 1000.0);
        earlyTapDelaysR[i] = static_cast<int>(earlyTapTimesR[i] * sampleRate / 1000.0);
        earlyTapGainsL[i] = earlyTapGainsLInit[i];
        earlyTapGainsR[i] = earlyTapGainsRInit[i];
    }

    // Pre-delay buffer (up to 100ms)
    int preDelaySize = static_cast<int>(100.0 * sampleRate / 1000.0) + 512;
    reverbPreDelayBuffer.setSize(2, preDelaySize);
    reverbPreDelayBuffer.clear();
    reverbPreDelayWritePos = 0;

    // Input diffusion allpasses
    static const int inputDiffDelaysInit[] = { 142, 107, 379 };
    int maxInputDiffSize = static_cast<int>(400 * srScale) + 128;
    for (int i = 0; i < kNumInputDiffusers; ++i)
    {
        inputDiffBuffersL[i].setSize(1, maxInputDiffSize);
        inputDiffBuffersR[i].setSize(1, maxInputDiffSize);
        inputDiffBuffersL[i].clear();
        inputDiffBuffersR[i].clear();
        inputDiffWritePosL[i] = 0;
        inputDiffWritePosR[i] = 0;
        inputDiffDelays[i] = static_cast<int>(inputDiffDelaysInit[i] * srScale);
    }

    // Smoothed reverb parameters
    smoothedReverbMix.reset(sampleRate, 0.05);
    smoothedReverbSize.reset(sampleRate, 0.05);
    smoothedReverbDamping.reset(sampleRate, 0.05);
    reverbLfoPhase = 0.0f;

    // Initialize distortion filters for 70s warmth
    distortionInputFilterL.prepare(currentSpec);
    distortionInputFilterR.prepare(currentSpec);
    distortionOutputFilterL.prepare(currentSpec);
    distortionOutputFilterR.prepare(currentSpec);

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
    chorus.reset();

    // Clear FDN reverb buffers
    for (int i = 0; i < kFDNSize; ++i)
        fdnDelayLines[i].clear();
    earlyReflectionBuffer.clear();
    reverbPreDelayBuffer.clear();
    for (int i = 0; i < kNumInputDiffusers; ++i)
    {
        inputDiffBuffersL[i].clear();
        inputDiffBuffersR[i].clear();
    }
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

float BeccaToneAmpProcessor::processAllpass(float input, float* buffer, int& writePos, int bufSize, int delaySamples, float feedback)
{
    delaySamples = juce::jmin(delaySamples, bufSize - 1);
    int readPos = writePos - delaySamples;
    if (readPos < 0) readPos += bufSize;

    float bufOut = buffer[readPos];
    float output = -input + bufOut;
    buffer[writePos] = input + bufOut * feedback;

    if (++writePos >= bufSize)
        writePos = 0;

    return output;
}

float BeccaToneAmpProcessor::processTubeSaturation(float sample, float drive, float bias)
{
    // Triode tube saturation - warm, asymmetric, even harmonics dominant
    // Models the transfer curve of a 12AX7-style preamp tube

    // Input stage - tubes have input capacitance that warms the signal
    float warmth = sample * 0.95f + bias * 0.05f;

    // Drive amount: 0-1 maps to 1-10x saturation
    float driveAmount = 1.0f + drive * 9.0f;
    float input = warmth * driveAmount;

    // Triode transfer function - asymmetric with soft positive clip, harder negative
    float shaped;
    if (input >= 0.0f)
    {
        // Positive half: soft, gradual compression (plate saturation)
        // More headroom, gentle rolloff - the "warm" side
        float x = input * 0.7f;
        shaped = x / (1.0f + 0.28f * x * x);  // Soft rational curve

        // Add 2nd harmonic (even) - signature of tube warmth
        shaped += 0.15f * driveAmount * shaped * std::abs(shaped);
    }
    else
    {
        // Negative half: harder clip (grid conduction)
        // This is where tubes "bite" - more abrupt limiting
        float x = -input * 0.9f;
        float limited = x / (1.0f + x);  // Harder curve
        shaped = -limited;

        // Grid blocking - reduces negative peaks more at high drive
        float gridBlock = 1.0f - 0.3f * driveAmount * limited * limited;
        shaped *= juce::jmax(0.3f, gridBlock);
    }

    // Plate sag simulation - compression increases with level
    float sagAmount = 0.1f * driveAmount * std::abs(shaped);
    shaped *= 1.0f / (1.0f + sagAmount);

    // Final soft limit (output transformer saturation)
    return shaped / (1.0f + 0.2f * std::abs(shaped));
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

    // 70s tube-style fuzz - warm, crunchy, even harmonics
    // Pre-filter: slight bass boost for warmth before saturation
    float inputFreq = 200.0f + (1.0f - drive) * 300.0f;  // Lower cutoff with more drive
    auto inputCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, inputFreq, 0.707f, juce::Decibels::decibelsToGain(3.0f + drive * 3.0f));
    *distortionInputFilterL.coefficients = *inputCoeffs;
    *distortionInputFilterR.coefficients = *inputCoeffs;

    // Tone control: low-pass after saturation (controls brightness)
    // 700 Hz (dark) to 5 kHz (bright), with gentle slope
    float toneFreq = 800.0f + tone * 4200.0f;
    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneFreq, 0.5f);
    *distortionToneFilterL.coefficients = *toneCoeffs;
    *distortionToneFilterR.coefficients = *toneCoeffs;

    // Output filter: gentle high-frequency rolloff to prevent harshness
    float outputFreq = 6000.0f + tone * 4000.0f;
    auto outputCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, outputFreq, 0.707f);
    *distortionOutputFilterL.coefficients = *outputCoeffs;
    *distortionOutputFilterR.coefficients = *outputCoeffs;

    // Slight asymmetric bias for even harmonics (tube character)
    float bias = 0.02f + drive * 0.03f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        auto& inputFilter = (channel == 0) ? distortionInputFilterL : distortionInputFilterR;
        auto& toneFilter = (channel == 0) ? distortionToneFilterL : distortionToneFilterR;
        auto& outputFilter = (channel == 0) ? distortionOutputFilterL : distortionOutputFilterR;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // Pre-filter for warmth
            input = inputFilter.processSample(input);

            // Tube saturation (warm, asymmetric clipping)
            float saturated = processTubeSaturation(input, drive, bias);

            // Tone shaping
            saturated = toneFilter.processSample(saturated);

            // Final smoothing to prevent any remaining harshness
            saturated = outputFilter.processSample(saturated);

            // Output level with slight makeup gain
            channelData[sample] = saturated * level * 0.85f;
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

    // Update smoothed values
    smoothedReverbSize.setTargetValue(size);
    smoothedReverbDamping.setTargetValue(damping);
    smoothedReverbMix.setTargetValue(mix);

    auto sampleRate = currentSpec.sampleRate;
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    float* outL = numChannels > 0 ? buffer.getWritePointer(0) : nullptr;
    float* outR = numChannels > 1 ? buffer.getWritePointer(1) : outL;

    int earlyBufSize = earlyReflectionBuffer.getNumSamples();
    float* earlyBufL = earlyReflectionBuffer.getWritePointer(0);
    float* earlyBufR = earlyReflectionBuffer.getWritePointer(1);

    // Modulation rate for lush, smooth reverb
    float modRate = 0.5f;
    float modDepth = 0.3f;

    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = outL ? outL[i] : 0.0f;
        float dryR = outR ? outR[i] : 0.0f;

        float currentMix = smoothedReverbMix.getNextValue();
        float currentSize = smoothedReverbSize.getNextValue();
        float currentDamping = smoothedReverbDamping.getNextValue();

        // === Early Reflections ===
        earlyBufL[earlyWritePos] = dryL;
        earlyBufR[earlyWritePos] = dryR;

        float earlyL = 0.0f, earlyR = 0.0f;
        for (int t = 0; t < kNumEarlyTaps; ++t)
        {
            int readPosL = earlyWritePos - static_cast<int>(earlyTapDelaysL[t] * currentSize);
            int readPosR = earlyWritePos - static_cast<int>(earlyTapDelaysR[t] * currentSize);
            if (readPosL < 0) readPosL += earlyBufSize;
            if (readPosR < 0) readPosR += earlyBufSize;

            earlyL += earlyBufL[readPosL] * earlyTapGainsL[t] + earlyBufR[readPosR] * earlyTapGainsR[t] * 0.15f;
            earlyR += earlyBufR[readPosR] * earlyTapGainsR[t] + earlyBufL[readPosL] * earlyTapGainsL[t] * 0.15f;
        }
        earlyL *= 0.4f;
        earlyR *= 0.4f;

        if (++earlyWritePos >= earlyBufSize)
            earlyWritePos = 0;

        // === Input Diffusion ===
        float diffInL = dryL;
        float diffInR = dryR;
        float inputDiffFeedback = 0.6f;

        for (int d = 0; d < kNumInputDiffusers; ++d)
        {
            int bufSizeL = inputDiffBuffersL[d].getNumSamples();
            int bufSizeR = inputDiffBuffersR[d].getNumSamples();

            diffInL = processAllpass(diffInL, inputDiffBuffersL[d].getWritePointer(0),
                                      inputDiffWritePosL[d], bufSizeL, inputDiffDelays[d], inputDiffFeedback);
            diffInR = processAllpass(diffInR, inputDiffBuffersR[d].getWritePointer(0),
                                      inputDiffWritePosR[d], bufSizeR, inputDiffDelays[d], inputDiffFeedback);
        }

        // === FDN (Feedback Delay Network) ===
        // LFO for gentle modulation
        float lfo = std::sin(reverbLfoPhase * juce::MathConstants<float>::twoPi);
        float lfoInc = modRate / static_cast<float>(sampleRate);
        reverbLfoPhase += lfoInc;
        if (reverbLfoPhase >= 1.0f) reverbLfoPhase -= 1.0f;

        // Calculate feedback from decay time (size controls decay)
        float decayTime = 0.5f + currentSize * 4.5f;  // 0.5s to 5s
        float avgDelayMs = 50.0f * currentSize;
        float feedback = std::pow(10.0f, -3.0f * (avgDelayMs / 1000.0f) / juce::jmax(decayTime, 0.1f));
        feedback = juce::jlimit(0.0f, 0.95f, feedback);  // Conservative limit for stability

        // Read outputs from all FDN delay lines
        std::array<float, kFDNSize> delayOuts;
        for (int j = 0; j < kFDNSize; ++j)
        {
            int bufSize = fdnDelayLines[j].getNumSamples();
            float* buf = fdnDelayLines[j].getWritePointer(0);

            // Modulate delay time slightly for lushness
            float modAmount = lfo * modDepth * 0.02f * ((j % 2) == 0 ? 1.0f : -1.0f);
            int delaySamp = static_cast<int>(fdnDelaySamples[j] * currentSize * (1.0f + modAmount));
            delaySamp = juce::jlimit(1, bufSize - 1, delaySamp);

            int readPos = fdnWritePos[j] - delaySamp;
            if (readPos < 0) readPos += bufSize;

            float out = buf[readPos];

            // Damping filter (one-pole lowpass)
            fdnFilterState[j] = out * (1.0f - currentDamping) + fdnFilterState[j] * currentDamping;
            delayOuts[j] = fdnFilterState[j];
        }

        // Hadamard mixing matrix (8x8) - creates dense, smooth reverb
        std::array<float, kFDNSize> mixed;
        for (int j = 0; j < kFDNSize; ++j)
        {
            mixed[j] = 0.0f;
            for (int k = 0; k < kFDNSize; ++k)
            {
                // Proper Hadamard sign pattern using popcount
                int popcount = 0;
                int val = j & k;
                while (val) { popcount += val & 1; val >>= 1; }
                float sign = (popcount % 2 == 0) ? 1.0f : -1.0f;
                mixed[j] += delayOuts[k] * sign;
            }
            mixed[j] *= kHadamard; // Normalize for energy preservation
        }

        // Write back to FDN delay lines with input injection
        float inputL = diffInL * 0.4f;
        float inputR = diffInR * 0.4f;

        for (int j = 0; j < kFDNSize; ++j)
        {
            int bufSize = fdnDelayLines[j].getNumSamples();
            float* buf = fdnDelayLines[j].getWritePointer(0);

            float input = (j < 4) ? inputL : inputR;
            buf[fdnWritePos[j]] = input + mixed[j] * feedback;

            if (++fdnWritePos[j] >= bufSize)
                fdnWritePos[j] = 0;
        }

        // FDN output: sum specific delay lines for stereo
        float lateL = (delayOuts[0] + delayOuts[2] + delayOuts[4] + delayOuts[6]) * 0.3f;
        float lateR = (delayOuts[1] + delayOuts[3] + delayOuts[5] + delayOuts[7]) * 0.3f;

        // Mix early and late reflections (30% early, 70% late)
        float wetL = earlyL * 0.3f + lateL * 0.7f;
        float wetR = earlyR * 0.3f + lateR * 0.7f;

        // Stereo width enhancement
        float mid = (wetL + wetR) * 0.5f;
        float side = (wetL - wetR) * 0.5f;
        float width = 0.8f + currentSize * 0.4f;
        side *= width;
        wetL = mid + side;
        wetR = mid - side;

        // Final wet/dry mix
        if (outL) outL[i] = dryL * (1.0f - currentMix) + wetL * currentMix;
        if (outR) outR[i] = dryR * (1.0f - currentMix) + wetR * currentMix;
    }
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

    // Get pedal order from parameters
    int pedalOrder[5];
    pedalOrder[0] = static_cast<int>(apvts.getRawParameterValue(ParamIDs::pedalSlot0)->load());
    pedalOrder[1] = static_cast<int>(apvts.getRawParameterValue(ParamIDs::pedalSlot1)->load());
    pedalOrder[2] = static_cast<int>(apvts.getRawParameterValue(ParamIDs::pedalSlot2)->load());
    pedalOrder[3] = static_cast<int>(apvts.getRawParameterValue(ParamIDs::pedalSlot3)->load());
    pedalOrder[4] = static_cast<int>(apvts.getRawParameterValue(ParamIDs::pedalSlot4)->load());

    // Process pedals in dynamic order
    // Pedal IDs: 0=distortion, 1=chorus, 2=tremolo, 3=delay, 4=reverb
    for (int slot = 0; slot < 5; ++slot)
    {
        switch (pedalOrder[slot])
        {
            case 0: processDistortion(buffer); break;
            case 1: processChorus(buffer); break;
            case 2: processTremolo(buffer); break;
            case 3: processDelay(buffer); break;
            case 4: processReverb(buffer); break;
        }
    }

    // Amp stage (gain, EQ, compression) - always after pedals
    processAmpStage(buffer);

    // Advanced EQ (if expert mode)
    processAdvancedEQ(buffer);

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

    // Helper to set pedal order (array of 5 pedal indices: 0=distortion, 1=chorus, 2=tremolo, 3=delay, 4=reverb)
    auto setPedalOrder = [this, &setParam](int slot0, int slot1, int slot2, int slot3, int slot4) {
        // Pedal slots are choice parameters with 5 options, so normalize: index / 4.0
        setParam(ParamIDs::pedalSlot0, slot0 / 4.0f);
        setParam(ParamIDs::pedalSlot1, slot1 / 4.0f);
        setParam(ParamIDs::pedalSlot2, slot2 / 4.0f);
        setParam(ParamIDs::pedalSlot3, slot3 / 4.0f);
        setParam(ParamIDs::pedalSlot4, slot4 / 4.0f);
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
            // Default pedal order: distortion -> chorus -> tremolo -> delay -> reverb
            setPedalOrder(0, 1, 2, 3, 4);
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
            // Default pedal order
            setPedalOrder(0, 1, 2, 3, 4);
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
            // Default pedal order
            setPedalOrder(0, 1, 2, 3, 4);
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
            // Fuzz first, then reverb at end
            setPedalOrder(0, 1, 2, 3, 4);
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
            // 80s: chorus -> delay -> reverb chain for that lush sound
            setPedalOrder(0, 1, 2, 3, 4);
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
            // Default pedal order
            setPedalOrder(0, 1, 2, 3, 4);
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
            // Indie: distortion -> tremolo -> chorus -> delay -> reverb
            setPedalOrder(0, 2, 1, 3, 4);
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

// State version - increment when making breaking parameter changes
static constexpr int kStateVersion = 1;

//==============================================================================
void BeccaToneAmpProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Add state version for forwards compatibility
    xml->setAttribute("stateVersion", kStateVersion);

    copyXmlToBinary(*xml, destData);
}

void BeccaToneAmpProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        // Check state version
        int loadedVersion = xmlState->getIntAttribute("stateVersion", 0);

        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        // Reset to defaults if state version mismatch (breaking parameter changes)
        if (loadedVersion != kStateVersion)
        {
            DBG("State version mismatch (loaded: " + juce::String(loadedVersion) +
                ", current: " + juce::String(kStateVersion) + ") - using defaults");
            // Add any parameter resets needed here
        }
    }
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
        config.pluginName = "BeccaToneAmp";
        activation_ = beatconnect::Activation::create(config);
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
