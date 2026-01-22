/*
  ==============================================================================
    BeccaToneAmp - Audio Processor
    Becca's signature guitar tone in a box

    Warm, buttery amp simulation with built-in effects pedals.
    Perfect for soulful fusion, funk, R&B, indie pop, and blues.
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class BeccaToneAmpProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    BeccaToneAmpProcessor();
    ~BeccaToneAmpProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter Access
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    //==============================================================================
    // BeatConnect Integration
    const juce::String& getPluginId() const { return pluginId_; }
    const juce::String& getApiBaseUrl() const { return apiBaseUrl_; }
    bool hasActivationEnabled() const;

    //==============================================================================
    // Level metering for UI
    float getInputLevel() const { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }

    //==============================================================================
    // Spectrum analyzer for visualizer
    static constexpr int FFT_ORDER = 10;  // 1024 points
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int NUM_SPECTRUM_BINS = FFT_SIZE / 2;

    void pushSamplesToFFT(const float* samples, int numSamples);
    void getSpectrum(float* magnitudes, int numBins) const;
    bool isSpectrumReady() const { return fftDataReady.load(); }

    //==============================================================================
    // Compressor gain reduction for UI
    float getGainReduction() const { return gainReduction.load(); }

private:
    //==============================================================================
    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // BeatConnect project data
    void loadProjectData();
    juce::String pluginId_;
    juce::String apiBaseUrl_;
    juce::String supabasePublishableKey_;
    juce::var buildFlags_;

    //==============================================================================
    // Level metering
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    //==============================================================================
    // FFT Spectrum Analyzer
    juce::dsp::FFT fft { FFT_ORDER };
    juce::dsp::WindowingFunction<float> fftWindow { FFT_SIZE, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, FFT_SIZE> fftFifo;
    std::array<float, FFT_SIZE * 2> fftData;
    std::array<float, NUM_SPECTRUM_BINS> spectrumMagnitudes;
    int fftFifoIndex = 0;
    std::atomic<bool> fftDataReady { false };

    //==============================================================================
    // Compressor gain reduction tracking
    std::atomic<float> gainReduction { 0.0f };

    //==============================================================================
    // DSP Processing Chain
    juce::dsp::ProcessSpec currentSpec;

    // Amp EQ (3-band)
    juce::dsp::IIR::Filter<float> bassFilterL, bassFilterR;
    juce::dsp::IIR::Filter<float> midFilterL, midFilterR;
    juce::dsp::IIR::Filter<float> trebleFilterL, trebleFilterR;
    juce::dsp::IIR::Filter<float> presenceFilterL, presenceFilterR;

    // Gain stage (tube-like saturation)
    juce::dsp::WaveShaper<float> waveshaper;
    juce::dsp::Gain<float> inputGain;
    juce::dsp::Gain<float> outputGain;

    // Compressor (subtle built-in)
    juce::dsp::Compressor<float> compressor;

    // Chorus
    juce::dsp::Chorus<float> chorus;

    // Tremolo (implemented with gain modulation)
    float tremoloPhase = 0.0f;

    // Delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    float delaySmoothedTime = 0.0f;

    // Reverb
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // Distortion/Fuzz pedal
    juce::dsp::WaveShaper<float> distortionShaper;
    juce::dsp::IIR::Filter<float> distortionToneFilterL, distortionToneFilterR;

    // Advanced EQ (8-band parametric)
    std::array<juce::dsp::IIR::Filter<float>, 8> eqFiltersL;
    std::array<juce::dsp::IIR::Filter<float>, 8> eqFiltersR;
    static constexpr std::array<float, 8> eqFrequencies = { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

    //==============================================================================
    // Smoothed parameter values
    juce::SmoothedValue<float> smoothedGain;
    juce::SmoothedValue<float> smoothedBass;
    juce::SmoothedValue<float> smoothedMid;
    juce::SmoothedValue<float> smoothedTreble;
    juce::SmoothedValue<float> smoothedPresence;
    juce::SmoothedValue<float> smoothedLevel;
    juce::SmoothedValue<float> smoothedVolume;
    juce::SmoothedValue<float> smoothedCompression;

    //==============================================================================
    void updateFilters();
    void processDistortion(juce::AudioBuffer<float>& buffer);
    void processChorus(juce::AudioBuffer<float>& buffer);
    void processTremolo(juce::AudioBuffer<float>& buffer);
    void processDelay(juce::AudioBuffer<float>& buffer);
    void processReverb(juce::AudioBuffer<float>& buffer);
    void processAmpStage(juce::AudioBuffer<float>& buffer);
    void processAdvancedEQ(juce::AudioBuffer<float>& buffer);
    float softClip(float x);

    //==============================================================================
    // Preset handling
    void applyPreset(int presetIndex);
    int lastAppliedPreset = -1;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeccaToneAmpProcessor)
};
