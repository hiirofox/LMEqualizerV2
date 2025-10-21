#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

extern void fft_f32(std::vector<float>& are, std::vector<float>& aim, int n, int inv);
class Spectrum1d
{
public:
	static constexpr int FFT_SIZE = 1024;
	static constexpr int HOP_SIZE = 512;
	static constexpr int LOG_SPECTRUM_BINS = 1024; // 对数频谱的bin数量
	static constexpr float MIN_FREQ = 10.01f;
	static constexpr float MAX_FREQ = 24000.0f;

	Spectrum1d(double sampleRate = 44100.0)
		: sampleRate_(sampleRate)
		, writePosition_(0)
	{
		inputBuffer_.resize(FFT_SIZE, 0.0f);
		windowBuffer_.resize(FFT_SIZE, 0.0f);
		fftReal_.resize(FFT_SIZE, 0.0f);
		fftImag_.resize(FFT_SIZE, 0.0f);
		linearMagnitudeBuffer_.resize(FFT_SIZE / 2, 0.0f);
		logSpectrumBuffer_.resize(LOG_SPECTRUM_BINS, 0.0f);

		createHannWindow();
		setupLogFrequencies();
	}

	~Spectrum1d() = default;

	void processBlock(const float* inL, const float* inR, int numSamples)
	{
		for (int i = 0; i < numSamples; ++i)
		{
			// 混合左右声道
			float sample = (inL[i] + inR[i]) * 0.5f;

			inputBuffer_[writePosition_] = sample;
			writePosition_++;

			// 当缓冲区满时执行FFT
			if (writePosition_ >= FFT_SIZE)
			{
				performFFT();
				writePosition_ = 0;
			}
		}
	}

	void setSampleRate(double sampleRate)
	{
		sampleRate_ = sampleRate;
		setupLogFrequencies(); // 重新设置对数频率
	}

	// 获取原始线性频谱数据（用于兼容性）
	std::vector<float> getLinearSpectrumData() const
	{
		juce::ScopedLock lock(spectrumLock_);
		return linearMagnitudeBuffer_;
	}

	// 获取对数频谱数据（平滑绘制使用）
	std::vector<float> getLogSpectrumData() const
	{
		juce::ScopedLock lock(spectrumLock_);
		return logSpectrumBuffer_;
	}

	// 获取对数频率数组
	std::vector<float> getLogFrequencies() const
	{
		return logFrequencies_;
	}

	double getSampleRate() const { return sampleRate_; }
	int getLogSpectrumSize() const { return LOG_SPECTRUM_BINS; }

private:
	void performFFT()
	{
		// 应用窗函数并复制到FFT缓冲区
		for (int i = 0; i < FFT_SIZE; ++i)
		{
			fftReal_[i] = inputBuffer_[i] * windowBuffer_[i];
			fftImag_[i] = 0.0f;
		}

		// 执行FFT
		fft_f32(fftReal_, fftImag_, FFT_SIZE, 0);

		// 计算线性频谱幅度并转换为dB
		calculateLinearSpectrum();

		// 转换为对数频谱
		convertToLogSpectrum();
	}

	void calculateLinearSpectrum()
	{
		juce::ScopedLock lock(spectrumLock_);

		for (int i = 0; i < FFT_SIZE / 2; ++i)
		{
			float magnitude = std::sqrt(fftReal_[i] * fftReal_[i] + fftImag_[i] * fftImag_[i]);

			// 归一化并转换为dB
			magnitude = magnitude / (FFT_SIZE * 0.5f);
			float dB = 20.0f * std::log10(std::max(magnitude, 1e-5f));

			linearMagnitudeBuffer_[i] = dB;
		}
	}

	void convertToLogSpectrum()
	{
		juce::ScopedLock lock(spectrumLock_);

		double freqPerBin = sampleRate_ / (FFT_SIZE * 2.0);

		// 创建线性频率数组
		std::vector<float> linearFreqs;
		std::vector<float> linearMags;

		for (int i = 1; i < FFT_SIZE / 2; ++i) // 跳过DC分量
		{
			float freq = (float)(i * freqPerBin);
			if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			{
				linearFreqs.push_back(freq);
				linearMags.push_back(linearMagnitudeBuffer_[i]);
			}
		}

		// 对每个对数频率点进行拉格朗日插值
		for (int i = 0; i < LOG_SPECTRUM_BINS; ++i)
		{
			float targetFreq = logFrequencies_[i];
			logSpectrumBuffer_[i] = lagrangeInterpolation(linearFreqs, linearMags, targetFreq);
		}
	}

	void setupLogFrequencies()
	{
		logFrequencies_.clear();
		logFrequencies_.reserve(LOG_SPECTRUM_BINS);

		float logMin = std::log10(MIN_FREQ);
		float logMax = std::log10(MAX_FREQ);
		float logStep = (logMax - logMin) / (LOG_SPECTRUM_BINS - 1);

		for (int i = 0; i < LOG_SPECTRUM_BINS; ++i)
		{
			float logFreq = logMin + i * logStep;
			logFrequencies_.push_back(std::pow(10.0f, logFreq));
		}
	}

	float lagrangeInterpolation(const std::vector<float>& xData,
		const std::vector<float>& yData,
		float x) const
	{
		if (xData.empty() || yData.empty() || xData.size() != yData.size())
			return -60.0f; // 返回默认最小dB值

		// 找到插值点周围的数据点
		int n = (int)xData.size();

		// 如果目标频率超出范围，返回边界值
		if (x <= xData[0]) return yData[0];
		if (x >= xData[n - 1]) return yData[n - 1];

		// 找到插值区间
		int startIdx = -1;
		for (int i = 0; i < n - 1; ++i)
		{
			if (x >= xData[i] && x <= xData[i + 1])
			{
				startIdx = i;
				break;
			}
		}

		if (startIdx == -1) return -60.0f;

		// 使用4点拉格朗日插值（如果可能）
		int numPoints = 4;
		int start = std::max(0, startIdx - 1);
		int end = std::min(n - 1, start + numPoints - 1);
		start = std::max(0, end - numPoints + 1);

		// 实际使用的插值点数
		int actualPoints = end - start + 1;

		// 拉格朗日插值计算
		float result = 0.0f;

		for (int i = start; i <= end; ++i)
		{
			float term = yData[i];

			// 计算拉格朗日基函数
			for (int j = start; j <= end; ++j)
			{
				if (i != j)
				{
					term *= (x - xData[j]) / (xData[i] - xData[j]);
				}
			}

			result += term;
		}

		return result;
	}

	void createHannWindow()
	{
		for (int i = 0; i < FFT_SIZE; ++i)
		{
			windowBuffer_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (FFT_SIZE - 1)));
		}
	}

private:
	double sampleRate_;
	std::vector<float> inputBuffer_;
	std::vector<float> windowBuffer_;
	std::vector<float> fftReal_;
	std::vector<float> fftImag_;
	std::vector<float> linearMagnitudeBuffer_;  // 线性频谱数据
	std::vector<float> logSpectrumBuffer_;      // 对数频谱数据
	std::vector<float> logFrequencies_;         // 对数频率数组

	int writePosition_;
	mutable juce::CriticalSection spectrumLock_;
};
