/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TestjuceAudioProcessor::TestjuceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ), mAPVTS(*this, nullptr, "PARAMETERS", createParameters())
#endif
{
    mFormatManager.registerBasicFormats();
    mAPVTS.state.addListener(this);

    for (int i = 0; i < mNumVoice; i++) {
        mSampler.addVoice(new juce::SamplerVoice());
    }
}

TestjuceAudioProcessor::~TestjuceAudioProcessor()
{
    mFormatReader = nullptr;
}

//==============================================================================
const juce::String TestjuceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TestjuceAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool TestjuceAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool TestjuceAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double TestjuceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TestjuceAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TestjuceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TestjuceAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String TestjuceAudioProcessor::getProgramName(int index)
{
    return {};
}

void TestjuceAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void TestjuceAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampler.setCurrentPlaybackSampleRate(sampleRate);

    updateADSRValue();
}

void TestjuceAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TestjuceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void TestjuceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (mShouldUpdate) {
        updateADSRValue();
    }

    mSampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool TestjuceAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TestjuceAudioProcessor::createEditor()
{
    return new TestjuceAudioProcessorEditor(*this);
}

//==============================================================================
void TestjuceAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TestjuceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void TestjuceAudioProcessor::loadFile() {
    mSampler.clearSounds();
    FileChooser chooser{ "Please load a file" };
    if (chooser.browseForFileToOpen()) {
        auto file = chooser.getResult();
        mFormatReader = mFormatManager.createReaderFor(file);
    }
    BigInteger range;
    range.setRange(0, 128, true);
    mSampler.addSound(new SamplerSound("Sample", *mFormatReader, range, 60, 0.1, 0.1, 10.0));

    updateADSRValue();
}

void TestjuceAudioProcessor::loadFile(const String& path) {
    mSampler.clearSounds();

    auto file = File(path);
    mFormatReader = mFormatManager.createReaderFor(file);

    auto sampleLength = static_cast<int>(mFormatReader->lengthInSamples);

    mWaveForm.setSize(1, sampleLength);
    mFormatReader->read(&mWaveForm, 0, sampleLength, 0, true, false);

    //auto buffer = mWaveForm.getReadPointer(0);

    //for (int sample = 0; sample < mWaveForm.getNumSamples(); ++sample) {
    //    DBG(buffer[sample]);
    //}

    BigInteger range;
    range.setRange(0, 128, true);

    mSampler.addSound(new SamplerSound("Sample", *mFormatReader, range, 60, 0.1, 0.1, 10.0));

}

void TestjuceAudioProcessor::updateADSRValue() {
    mADSRParams.attack = mAPVTS.getRawParameterValue("ATTACK")->load();
    mADSRParams.decay = mAPVTS.getRawParameterValue("DECAY")->load();
    mADSRParams.sustain = mAPVTS.getRawParameterValue("SUSTAIN")->load();
    mADSRParams.release = mAPVTS.getRawParameterValue("RELEASE")->load();

    for (int i = 0; i < mSampler.getNumSounds(); ++i) {
        if (auto sound = dynamic_cast<SamplerSound*>(mSampler.getSound(i).get())) {
            sound->setEnvelopeParameters(mADSRParams);
        }
    }
}

AudioProcessorValueTreeState::ParameterLayout TestjuceAudioProcessor::createParameters() {
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<AudioParameterFloat>("ATTACK", "Attack", 0.0f, 5.0f, 0.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("DECAY", "Decay", 0.0f, 3.0f, 2.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("SUSTAIN", "Sustain", 0.0f, 1.0f, 1.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("RELEASE", "Release", 0.0f, 5.0f, 2.0f));

    return { parameters.begin(), parameters.end() };
}

void TestjuceAudioProcessor::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) {
    mShouldUpdate = true;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestjuceAudioProcessor();
}