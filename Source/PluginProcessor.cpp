/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
LModelAudioProcessor::LModelAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	)
#endif
{

}


juce::AudioProcessorValueTreeState::ParameterLayout LModelAudioProcessor::createParameterLayout()
{
	juce::AudioProcessorValueTreeState::ParameterLayout layout;
	return layout;
}

LModelAudioProcessor::~LModelAudioProcessor()
{
}

//==============================================================================
const juce::String LModelAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool LModelAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool LModelAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool LModelAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double LModelAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int LModelAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int LModelAudioProcessor::getCurrentProgram()
{
	return 0;
}

void LModelAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String LModelAudioProcessor::getProgramName(int index)
{
	return "LMEqualizer2";
}

void LModelAudioProcessor::changeProgramName(int index, const juce::String& newName)
{

}

//==============================================================================
void LModelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	eq.SetSampleRate(sampleRate);
}

void LModelAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LModelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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


void LModelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	int isMidiUpdata = 0;
	juce::MidiMessage MidiMsg;//先处理midi事件
	int MidiTime;
	juce::MidiBuffer::Iterator MidiBuf(midiMessages);
	while (MidiBuf.getNextEvent(MidiMsg, MidiTime))
	{
		if (MidiMsg.isNoteOn())
		{
			int note = MidiMsg.getNoteNumber() - 24;
		}
		if (MidiMsg.isNoteOff())
		{
			int note = MidiMsg.getNoteNumber() - 24;
		}
	}
	midiMessages.clear();

	const int numSamples = buffer.getNumSamples();
	float* wavbufl = buffer.getWritePointer(0);
	float* wavbufr = buffer.getWritePointer(1);
	const float* recbufl = buffer.getReadPointer(0);
	const float* recbufr = buffer.getReadPointer(1);

	float SampleRate = getSampleRate();

	eq.ProcessBlock(recbufl, recbufr, wavbufl, wavbufr, numSamples);

}

//==============================================================================
bool LModelAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* LModelAudioProcessor::createEditor()
{
	return new LModelAudioProcessorEditor(*this);

	//return new juce::GenericAudioProcessorEditor(*this);//自动绘制(调试)
}

//==============================================================================
void LModelAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// 创建ValueTree来存储所有参数状态
	juce::ValueTree state("EqualizerPlugin");

	// 方法2：使用ValueTree结构化存储（推荐）
	juce::ValueTree eqState("Equalizer");
	eqState.setProperty("sampleRate", eq.GetSampleRate(), nullptr);

	auto activeIds = eq.GetActiveNodeIds();
	eqState.setProperty("nodeCount", static_cast<int>(activeIds.size()), nullptr);

	for (int i = 0; i < activeIds.size(); ++i) {
		int nodeId = activeIds[i];
		const auto& node = eq.GetNode(nodeId);

		juce::ValueTree nodeTree("Node" + juce::String(i));
		nodeTree.setProperty("mode", node.mode, nullptr);
		nodeTree.setProperty("cutoff", node.cutoff, nullptr);
		nodeTree.setProperty("q", node.q, nullptr);
		nodeTree.setProperty("gainDB", node.gainDB, nullptr);
		nodeTree.setProperty("active", node.active, nullptr);

		eqState.appendChild(nodeTree, nullptr);
	}

	state.appendChild(eqState, nullptr);

	// 如果您有其他参数，也可以在这里添加
	// 例如：
	// if (auto* params = getParameters())
	// {
	//     for (auto* param : *params)
	//     {
	//         if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(param))
	//         {
	//             state.setProperty(p->getParameterID(), p->get(), nullptr);
	//         }
	//     }
	// }

	// 将ValueTree写入MemoryBlock
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void LModelAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{

	// 从MemoryBlock读取XML
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState != nullptr)
	{
		if (xmlState->hasTagName("EqualizerPlugin"))
		{
			juce::ValueTree state = juce::ValueTree::fromXml(*xmlState);

			// 方法2：从ValueTree结构化读取（推荐）
			juce::ValueTree eqState = state.getChildWithName("Equalizer");
			if (eqState.isValid())
			{
				float sampleRate = eqState.getProperty("sampleRate", 48000.0f);
				int nodeCount = eqState.getProperty("nodeCount", 0);

				// 清除现有EQ设置
				eq.Clear();
				eq.SetSampleRate(sampleRate);

				// 重建所有节点
				for (int i = 0; i < nodeCount; ++i)
				{
					juce::ValueTree nodeTree = eqState.getChildWithName("Node" + juce::String(i));
					if (nodeTree.isValid())
					{
						int mode = nodeTree.getProperty("mode", MODE_PEAKING);
						float cutoff = nodeTree.getProperty("cutoff", 1000.0f);
						float q = nodeTree.getProperty("q", 0.707f);
						float gainDB = nodeTree.getProperty("gainDB", 0.0f);
						bool active = nodeTree.getProperty("active", false);

						if (active)
						{
							eq.AddNode(mode, cutoff, q, gainDB);
						}
					}
				}
			}

			// 如果您有其他参数，也可以在这里恢复
			// 例如：
			// if (auto* params = getParameters())
			// {
			//     for (auto* param : *params)
			//     {
			//         if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(param))
			//         {
			//             if (state.hasProperty(p->getParameterID()))
			//             {
			//                 *p = state.getProperty(p->getParameterID());
			//             }
			//         }
			//     }
			// }
		}
	}

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new LModelAudioProcessor();
}
