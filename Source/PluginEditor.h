/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LM_slider.h"
#include "ui/EnveUI.h"
#include "ui/specWaterFallUI.h"
#include "ui/menuTable.h"

//==============================================================================
/**
*/
class LModelAudioProcessorEditor : public juce::AudioProcessorEditor, juce::Timer
{
public:
	LModelAudioProcessorEditor(LModelAudioProcessor&);
	~LModelAudioProcessorEditor() override;

	//==============================================================================
	void paint(juce::Graphics&) override;
	void resized() override;
	void timerCallback() override;//注意，这里面不定期会给fft填东西。

private:
	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	LModelAudioProcessor& audioProcessor;
	LMKnob K_LT, K_RT, K_FB, K_POW;
	LMKnob K_LPM, K_DRY;
	EnveUI enveUI1{ &audioProcessor.enveFunc1 };
	EnveUI enveUI2{ &audioProcessor.enveFunc2 };

	SpecWaterFallUI swfUI{ audioProcessor.swf };
	
	MenuTable menutable;

	juce::ComponentBoundsConstrainer constrainer;  // 用于设置宽高比例
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LModelAudioProcessorEditor)
};

