#pragma once
#include <JuceHeader.h>
#include "../dsp/spectrum1d.h"
#include <memory>
#include <algorithm>
#include <cmath>
class SpectrumUI : public juce::Component, public juce::Timer
{
public:
	// ���Ƴ�������
	static constexpr float MIN_FREQ = 10.01f;
	static constexpr float MAX_FREQ = 24000.0f;
	static constexpr float MIN_DB = -40.0f;
	static constexpr float MAX_DB = 40.0f;
	const juce::Colour BORDER_COLOR = juce::Colour(0xff00ff00);
	static constexpr float BORDER_WIDTH = 2.0f;
	const juce::Colour SPECTRUM_LINE_COLOR = juce::Colour(0xffffffff);
	const juce::Colour SPECTRUM_FILL_COLOR = juce::Colour(0xff555555);
	static constexpr int REFRESH_RATE_HZ = 30;
	explicit SpectrumUI(std::shared_ptr<Spectrum1d> processor = nullptr)
		: processor_(processor)
		, useLogSpectrum_(true) // Ĭ��ʹ�ö���Ƶ��
	{
		startTimerHz(REFRESH_RATE_HZ);
		setOpaque(false);
	}

	void setProcessor(std::shared_ptr<Spectrum1d> processor)
	{
		processor_ = processor;
	}

	// �����Ƿ�ʹ�ö���Ƶ�׻��ƣ�ƽ�����ƣ�
	void setUseLogSpectrum(bool useLog)
	{
		useLogSpectrum_ = useLog;
	}

	bool isUsingLogSpectrum() const
	{
		return useLogSpectrum_;
	}

	// Component overrides
	void paint(juce::Graphics& g) override
	{
		auto bounds = getLocalBounds().toFloat();

		// ���Ʊ���
		g.fillAll(juce::Colours::black);

		// ���Ʊ߿�
		g.setColour(BORDER_COLOR);
		g.drawRect(bounds, BORDER_WIDTH);

		// ����Ƶ�׻�������ȥ���߿�
		spectrumBounds_ = bounds.reduced(BORDER_WIDTH);

		if (!spectrumData_.empty())
		{
			if (useLogSpectrum_)
				updateLogSpectrumPath();
			else
				updateLinearSpectrumPath();

			// �����������
			g.setColour(SPECTRUM_FILL_COLOR);
			juce::Path fillPath = spectrumPath_;
			fillPath.lineTo(spectrumBounds_.getRight(), spectrumBounds_.getBottom());
			fillPath.lineTo(spectrumBounds_.getX(), spectrumBounds_.getBottom());
			fillPath.closeSubPath();
			g.fillPath(fillPath);

			// ����Ƶ����
			g.setColour(SPECTRUM_LINE_COLOR);
			g.strokePath(spectrumPath_, juce::PathStrokeType(1.0f));
		}

		// ���ƿ̶ȱ�ǩ
		drawGridAndLabels(g);
	}

	void resized() override
	{
		// �����С�ı�ʱ����Ƶ��·��
		repaint();
	}

	// Timer override
	void timerCallback() override
	{
		if (processor_)
		{
			if (useLogSpectrum_)
			{
				spectrumData_ = processor_->getLogSpectrumData();
				logFrequencies_ = processor_->getLogFrequencies();
			}
			else
			{
				spectrumData_ = processor_->getLinearSpectrumData();
			}
			repaint();
		}
	}

private:
	void drawGridAndLabels(juce::Graphics& g)
	{
		g.setColour(juce::Colours::grey.withAlpha(0.3f));
		g.setFont(10.0f);

		// ����Ƶ��������
		std::vector<float> frequencies = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
		for (float freq : frequencies)
		{
			if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			{
				float x = frequencyToPosition(freq, spectrumBounds_);
				g.drawVerticalLine((int)x, spectrumBounds_.getY(), spectrumBounds_.getBottom());

				// ����Ƶ�ʱ�ǩ
				juce::String label;
				if (freq >= 1000)
					label = juce::String(freq / 1000, 1) + "k";
				else
					label = juce::String((int)freq);

				g.drawText(label, (int)x - 15, (int)spectrumBounds_.getBottom() + 2, 30, 12,
					juce::Justification::centred);
			}
		}

		// ����dB������
		for (float db = MIN_DB; db <= MAX_DB; db += 10.0f)
		{
			float y = dbToPosition(db, spectrumBounds_);
			g.drawHorizontalLine((int)y, spectrumBounds_.getX(), spectrumBounds_.getRight());

			// ����dB��ǩ
			g.drawText(juce::String((int)db) + "dB",
				(int)spectrumBounds_.getX() - 35, (int)y - 6, 30, 12,
				juce::Justification::centredRight);
		}
	}

	float frequencyToPosition(float freq, const juce::Rectangle<float>& bounds) const
	{
		float logMinFreq = std::log10(MIN_FREQ);
		float logMaxFreq = std::log10(MAX_FREQ);
		float logFreq = std::log10(juce::jlimit(MIN_FREQ, MAX_FREQ, freq));

		float ratio = (logFreq - logMinFreq) / (logMaxFreq - logMinFreq);
		return bounds.getX() + ratio * bounds.getWidth();
	}

	float positionToFrequency(float x, const juce::Rectangle<float>& bounds) const
	{
		float ratio = (x - bounds.getX()) / bounds.getWidth();
		ratio = juce::jlimit(0.0f, 1.0f, ratio);

		float logMinFreq = std::log10(MIN_FREQ);
		float logMaxFreq = std::log10(MAX_FREQ);
		float logFreq = logMinFreq + ratio * (logMaxFreq - logMinFreq);

		return std::pow(10.0f, logFreq);
	}

	float dbToPosition(float db, const juce::Rectangle<float>& bounds) const
	{
		float ratio = (db - MIN_DB) / (MAX_DB - MIN_DB);
		ratio = juce::jlimit(0.0f, 1.0f, ratio);

		// Y�ᷭת����dBֵ���Ϸ�
		return bounds.getBottom() - ratio * bounds.getHeight();
	}

	// ���ƶ���Ƶ�ף�ƽ����
	void updateLogSpectrumPath()
	{
		spectrumPath_.clear();

		if (spectrumData_.empty() || logFrequencies_.empty() ||
			spectrumBounds_.isEmpty() || !processor_)
			return;

		bool firstPoint = true;

		for (size_t i = 0; i < spectrumData_.size() && i < logFrequencies_.size(); ++i)
		{
			float freq = logFrequencies_[i];
			float magnitude = spectrumData_[i];

			if (freq < MIN_FREQ || freq > MAX_FREQ)
				continue;

			float x = frequencyToPosition(freq, spectrumBounds_);
			float y = dbToPosition(magnitude, spectrumBounds_);

			if (firstPoint)
			{
				spectrumPath_.startNewSubPath(x, y);
				firstPoint = false;
			}
			else
			{
				spectrumPath_.lineTo(x, y);
			}
		}
	}

	// ��������Ƶ�ף�ԭʼ������
	void updateLinearSpectrumPath()
	{
		spectrumPath_.clear();

		if (spectrumData_.empty() || spectrumBounds_.isEmpty() || !processor_)
			return;

		double sampleRate = processor_->getSampleRate();
		double freqPerBin = sampleRate / (Spectrum1d::FFT_SIZE * 2.0);

		bool firstPoint = true;

		for (size_t bin = 1; bin < spectrumData_.size(); ++bin) // ��bin 1��ʼ������DC����
		{
			float freq = (float)(bin * freqPerBin);

			if (freq < MIN_FREQ || freq > MAX_FREQ)
				continue;

			float x = frequencyToPosition(freq, spectrumBounds_);
			float y = dbToPosition(spectrumData_[bin], spectrumBounds_);

			if (firstPoint)
			{
				spectrumPath_.startNewSubPath(x, y);
				firstPoint = false;
			}
			else
			{
				spectrumPath_.lineTo(x, y);
			}
		}
	}

private:
	std::shared_ptr<Spectrum1d> processor_;
	std::vector<float> spectrumData_;
	std::vector<float> logFrequencies_;
	juce::Path spectrumPath_;
	juce::Rectangle<float> spectrumBounds_;
	bool useLogSpectrum_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumUI)
};