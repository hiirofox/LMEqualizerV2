#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <utility>

#define MaxBiquadStages 12 //最多12+1个Biquad串联
struct BiquadCoeffs
{
	float b0, b1, b2, a1, a2;
	int numStages;
	float b0s[MaxBiquadStages];
	float b1s[MaxBiquadStages];
	float b2s[MaxBiquadStages];
	float a1s[MaxBiquadStages];
	float a2s[MaxBiquadStages];

	BiquadCoeffs() : b0(0.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f), numStages(0)
	{
		memset(b0s, 0, sizeof(b0s));
		memset(b1s, 0, sizeof(b1s));
		memset(b2s, 0, sizeof(b2s));
		memset(a1s, 0, sizeof(a1s));
		memset(a2s, 0, sizeof(a2s));
		for (int i = 0; i < MaxBiquadStages; ++i) b0s[i] = 1.0;
	}
	BiquadCoeffs(float b0, float b1, float b2, float a1, float a2) : b0(b0), b1(b1), b2(b2), a1(a1), a2(a2), numStages(0)
	{
		memset(b0s, 0, sizeof(b0s));
		memset(b1s, 0, sizeof(b1s));
		memset(b2s, 0, sizeof(b2s));
		memset(a1s, 0, sizeof(a1s));
		memset(a2s, 0, sizeof(a2s));
		for (int i = 0; i < MaxBiquadStages; ++i) b0s[i] = 1.0;
	}
	BiquadCoeffs(const float* b0s,
		const float* b1s,
		const float* b2s,
		const float* a1s,
		const float* a2s,
		int numStages)
	{
		this->numStages = numStages;
		this->b0 = b0s[0];
		this->b1 = b1s[0];
		this->b2 = b2s[0];
		this->a1 = a1s[0];
		this->a2 = a2s[0];
		for (int i = 1; i < numStages; ++i)
		{
			this->b0s[i - 1] = b0s[i];
			this->b1s[i - 1] = b1s[i];
			this->b2s[i - 1] = b2s[i];
			this->a1s[i - 1] = a1s[i];
			this->a2s[i - 1] = a2s[i];
		}
		for (int i = numStages - 1; i < MaxBiquadStages; ++i)
		{
			this->b0s[i] = 0;
			this->b1s[i] = 0;
			this->b2s[i] = 0;
			this->a1s[i] = 0;
			this->a2s[i] = 0;
		}
	}
};

class Biquad
{
private:
	BiquadCoeffs coeffs;
	float z1 = 0.0f, z2 = 0.0f;
public:
	Biquad() {}
	void SetCoeffs(const BiquadCoeffs& c) { coeffs = c; }
	float ProcessSample(float in)
	{
		float out = coeffs.b0 * in + coeffs.b1 * z1 + coeffs.b2 * z2 - coeffs.a1 * z1 - coeffs.a2 * z2;
		z2 = z1;
		z1 = out;
		return out;
	}
};

class BiquadDesigner
{
private:
	constexpr static float pihalf = M_PI / 2.0;

	float sampleRate = 48000.0f;

	// ---- f1 / f2 and matching gains ----
	static std::pair<float, float> calc_h_phi(float fc, float fc4, float g, float invg, float f) {
		float f4 = powf(f, 4.0f);
		float h = (fc4 + f4 * g) / (fc4 + f4 * invg);
		float phi = powf(sinf(pihalf * f), 2.0f);
		return std::pair<float, float>(h, phi);
	};

	static void computePoles(float f0, float Q, float& a1, float& a2)
	{
		// Impulse-invariant poles  (BiquadFits Eq. 12)
		float w0 = 2.0f * static_cast<float>(M_PI) * f0;
		float wd = w0 * sqrtf(fmaxf(0.0f, 1.0f - 1.0f / (4.0f * Q * Q)));
		float e = expf(-w0 / (2.0f * Q));
		a1 = -2.0f * e * cosf(wd);
		a2 = e * e;
	}

public:
	BiquadDesigner(float sr = 48000.0f) : sampleRate(sr) {}
	void SetSampleRate(float sr) { sampleRate = sr; }
	float GetSampleRate() const { return sampleRate; }

	// ---------------------------------------------------------------------
	// Matched biquad low-pass  (Vicanek 2016 Eq. 46-47)
	BiquadCoeffs DesignLPF(float cutoff, float stages, float ctofGainDB)
	{
		int numStages = stages / 40.0 * 12.0;
		if (numStages >= MaxBiquadStages)numStages = MaxBiquadStages - 1;
		ctofGainDB = ctofGainDB / (numStages + 1);//每级的增益

		if (ctofGainDB < -6.0)
		{
			cutoff *= powf(10.0, (ctofGainDB + 6.0) / 32.0);//经验公式
			if (cutoff > sampleRate / 2.0) cutoff = sampleRate / 2.0;
			ctofGainDB = -6.0;
		}
		float Q = powf(10.0, ctofGainDB / 20.0);//约-6.0dB以下则不能匹配

		float f0 = cutoff / sampleRate;
		float a1, a2;
		computePoles(f0, Q, a1, a2);

		float f02 = f0 * f0;
		float term = sqrtf((1.0f - f02) * (1.0f - f02) + f02 / (Q * Q));

		float r0 = 1.0f + a1 + a2;
		float r1 = (1.0f - a1 + a2) * f02 / term;

		float b0 = 0.5f * (r0 + r1);
		float b1 = r0 - b0;
		float b2 = 0.0f;

		BiquadCoeffs coeffs(b0, b1, b2, a1, a2);
		for (int i = 0; i < numStages; ++i)
		{
			coeffs.b0s[i] = b0;
			coeffs.b1s[i] = b1;
			coeffs.b2s[i] = b2;
			coeffs.a1s[i] = a1;
			coeffs.a2s[i] = a2;
		}
		coeffs.numStages = numStages;
		return coeffs;
	}

	// Matched biquad high-pass  (Vicanek 2016 Eq. 48-49)
	BiquadCoeffs DesignHPF(float cutoff, float stages, float ctofGainDB)
	{
		int numStages = stages / 40.0 * 12.0;
		if (numStages >= MaxBiquadStages)numStages = MaxBiquadStages - 1;
		ctofGainDB = ctofGainDB / (numStages + 1);//每级的增益

		if (ctofGainDB < -6.0)
		{
			cutoff /= powf(10.0, (ctofGainDB + 6.0) / 32.0);//经验公式
			if (cutoff > sampleRate / 2.0) cutoff = sampleRate / 2.0;
			ctofGainDB = -6.0;
		}
		float Q = powf(10.0, ctofGainDB / 20.0);

		float f0 = cutoff / sampleRate;
		float a1, a2;
		computePoles(f0, Q, a1, a2);

		float f02 = f0 * f0;
		float term = sqrtf((1.0f - f02) * (1.0f - f02) + f02 / (Q * Q));

		float r1 = (1.0f - a1 + a2) / term;

		float b0 = 0.25f * r1;
		float b1 = -2.0f * b0;
		float b2 = b0;

		BiquadCoeffs coeffs(b0, b1, b2, a1, a2);
		for (int i = 0; i < numStages; ++i)
		{
			coeffs.b0s[i] = b0;
			coeffs.b1s[i] = b1;
			coeffs.b2s[i] = b2;
			coeffs.a1s[i] = a1;
			coeffs.a2s[i] = a2;
		}
		coeffs.numStages = numStages;
		return coeffs;
	}

	// 带通滤波器
	BiquadCoeffs DesignBPF(float cutoff, float stages, float ctofGainDB)
	{
		int numStages = stages / 40.0 * 12.0;
		if (numStages >= MaxBiquadStages)numStages = MaxBiquadStages - 1;
		ctofGainDB = ctofGainDB / (numStages + 1);//每级的增益

		float Q = powf(10.0, ctofGainDB / 20.0);

		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);

		float a0 = 1.0f + alpha;
		float b0 = alpha / a0 * Q;
		float b1 = 0.0f;
		float b2 = -alpha / a0 * Q;
		float a1 = -2.0f * cosw0 / a0;
		float a2 = (1.0f - alpha) / a0;

		BiquadCoeffs coeffs(b0, b1, b2, a1, a2);
		for (int i = 0; i < numStages; ++i)
		{
			coeffs.b0s[i] = b0;
			coeffs.b1s[i] = b1;
			coeffs.b2s[i] = b2;
			coeffs.a1s[i] = a1;
			coeffs.a2s[i] = a2;
		}
		coeffs.numStages = numStages;
		return coeffs;
	}
	BiquadCoeffs DesignTilt(float cutoff, float /*Q*/, float gainDB)
	{
		return BiquadCoeffs{ 1,0,0,0,0 };
	}

	BiquadCoeffs DesignPeaking(float cutoff, float Q, float gainDB)
	{
		// --- 步骤 0: 初始化计算 ---
		bool isCut = gainDB < 0.0f;
		float g0 = pow(10.0f, gainDB / 20.0f);
		// 算法内部始终按增益滤波器处理，如果是衰减型，最后再进行反转
		if (isCut) {
			g0 = 1.0f / g0;
		}

		float w0T = 2.0f * M_PI * cutoff / sampleRate;

		// --- 步骤 1 & 2: 使用标准MZT方法计算分母'a'系数 (极点) ---
		// 这些系数基于增益滤波器的分母部分（高Q值部分）
		// 参考论文 Section 3.1, Equations (12)-(15)
		float a1_calc, a2_calc;
		if (Q > 0.5f) {
			a1_calc = -2.0f * exp(-w0T / (2.0f * Q)) * cos(sqrt(1.0f - pow(1.0f / (2.0f * Q), 2.0f)) * w0T);
			a2_calc = exp(-w0T / Q);
		}
		else {
			// 当 Q <= 0.5 时，系统为过阻尼，有两个实数极点
			float p1_term = 1.0f / (2.0f * Q) + sqrt(pow(1.0f / (2.0f * Q), 2.0f) - 1.0f);
			float p2_term = 1.0f / (2.0f * Q) - sqrt(pow(1.0f / (2.0f * Q), 2.0f) - 1.0f);
			a1_calc = -(exp(-w0T * p1_term) + exp(-w0T * p2_term));
			a2_calc = exp(-w0T / Q);
		}

		// --- 步骤 3: 计算幅度响应，以获得校正滤波器的参数 ---

		// 3a. 目标模拟滤波器在 Fs/6 和 Fs/3 处的幅度响应
		float w_ratio1 = (sampleRate / 6.0f) / cutoff;
		float w_ratio2 = (sampleRate / 3.0f) / cutoff;

		float num1 = pow(1.0f - w_ratio1 * w_ratio1, 2.0f) + pow(g0 * w_ratio1 / Q, 2.0f);
		float den1 = pow(1.0f - w_ratio1 * w_ratio1, 2.0f) + pow(w_ratio1 / Q, 2.0f);
		float Ha1 = (den1 == 0.0f) ? 1.0f : sqrt(num1 / den1);

		float num2 = pow(1.0f - w_ratio2 * w_ratio2, 2.0f) + pow(g0 * w_ratio2 / Q, 2.0f);
		float den2 = pow(1.0f - w_ratio2 * w_ratio2, 2.0f) + pow(w_ratio2 / Q, 2.0f);
		float Ha2 = (den2 == 0.0f) ? 1.0f : sqrt(num2 / den2);

		// 3b. 全极点数字滤波器在 Fs/6 和 Fs/3 处的幅度响应
		// 参考论文 Section 2, Equations (9), (10) 的分母部分
		float Hd1_den_sq = 1.0f + a1_calc - a2_calc + a1_calc * a1_calc + a1_calc * a2_calc + a2_calc * a2_calc;
		float Hd1 = (Hd1_den_sq <= 0.0f) ? 1.0f : 1.0f / sqrt(Hd1_den_sq);

		float Hd2_den_sq = 1.0f - a1_calc - a2_calc + a1_calc * a1_calc - a1_calc * a2_calc + a2_calc * a2_calc;
		float Hd2 = (Hd2_den_sq <= 0.0f) ? 1.0f : 1.0f / sqrt(Hd2_den_sq);

		// 3c. 校正滤波器的目标响应
		float H0 = 1.0f + a1_calc + a2_calc; // DC 增益
		float H1 = Ha1 / Hd1;
		float H2 = Ha2 / Hd2;

		// --- 步骤 4: 使用 FIR Curve Fit Transform 计算分子 'b' 系数 (零点) ---
		// 参考论文 Section 3.2.2, Step 4 & Equations (27), (28), (29)
		float b1_calc_sqrt_term = H0 * H0 - 2.0f * H1 * H1 + 2.0f * H2 * H2;
		if (b1_calc_sqrt_term < 0.0f) b1_calc_sqrt_term = 0.0f; // 确保参数非负
		float b1_calc = (H0 - sqrt(b1_calc_sqrt_term)) / 2.0f;

		float b2_calc_sqrt_term = -3.0f * H0 * H0 - 6.0f * H0 * b1_calc - 3.0f * b1_calc * b1_calc + 12.0f * H1 * H1;
		if (b2_calc_sqrt_term < 0.0f) b2_calc_sqrt_term = 0.0f; // 确保参数非负
		float b2_calc = (3.0f * (H0 - b1_calc) - sqrt(b2_calc_sqrt_term)) / 6.0f;

		float b0_calc = H0 - b1_calc - b2_calc;

		// --- 步骤 5: 如果是衰减型滤波器，则进行反转 ---
		if (isCut) {
			// 反转滤波器: H_cut = 1 / H_boost
			// H_cut = (1 + a1*z^-1 + a2*z^-2) / (b0 + b1*z^-1 + b2*z^-2)
			// 标准化为 Biquad 形式:
			// H_cut = (1/b0 + (a1/b0)*z^-1 + (a2/b0)*z^-2) / (1 + (b1/b0)*z^-1 + (b2/b0)*z^-2)
			if (b0_calc == 0.0f) {
				// 避免除以零，返回一个平坦响应
				return BiquadCoeffs(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			float inv_b0 = 1.0f / b0_calc;
			return BiquadCoeffs(inv_b0, a1_calc * inv_b0, a2_calc * inv_b0,
				b1_calc * inv_b0, b2_calc * inv_b0);
		}
		else {
			// 如果是增益型，直接使用计算出的系数
			return BiquadCoeffs(b0_calc, b1_calc, b2_calc, a1_calc, a2_calc);
		}
	}

	// 低频搁架滤波器
	BiquadCoeffs DesignLowshelf(float cutoff, float Q, float gainDB)
	{
		float A = powf(10.0f, gainDB / 20.0f);
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);
		float sqrtA = sqrtf(A);

		float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
		float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
		float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
		float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
		float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
		float a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 高频搁架滤波器
	BiquadCoeffs DesignHighshelf(float cutoff, float Q, float gainDB)
	{
		float A = powf(10.0f, gainDB / 20.0f);
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);
		float sqrtA = sqrtf(A);

		float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
		float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
		float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
		float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
		float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
		float a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

};

/*
class BiquadDesigner
{
private:
	float sampleRate = 48000.0f;
public:
	BiquadDesigner(float sr = 48000.0f) : sampleRate(sr) {}
	void SetSampleRate(float sr) { sampleRate = sr; }
	float GetSampleRate() const { return sampleRate; }

	// 低通滤波器
	BiquadCoeffs DesignLPF(float cutoff, float Q)
	{
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);

		float a0 = 1.0f + alpha;
		float b0 = (1.0f - cosw0) / 2.0f;
		float b1 = 1.0f - cosw0;
		float b2 = (1.0f - cosw0) / 2.0f;
		float a1 = -2.0f * cosw0;
		float a2 = 1.0f - alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 高通滤波器
	BiquadCoeffs DesignHPF(float cutoff, float Q)
	{
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);

		float a0 = 1.0f + alpha;
		float b0 = (1.0f + cosw0) / 2.0f;
		float b1 = -(1.0f + cosw0);
		float b2 = (1.0f + cosw0) / 2.0f;
		float a1 = -2.0f * cosw0;
		float a2 = 1.0f - alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 低频搁架滤波器
	BiquadCoeffs DesignLowshelf(float cutoff, float Q, float gainDB)
	{
		float A = powf(10.0f, gainDB / 40.0f);
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);
		float sqrtA = sqrtf(A);

		float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
		float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
		float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
		float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
		float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
		float a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 高频搁架滤波器
	BiquadCoeffs DesignHighshelf(float cutoff, float Q, float gainDB)
	{
		float A = powf(10.0f, gainDB / 40.0f);
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);
		float sqrtA = sqrtf(A);

		float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
		float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
		float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
		float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
		float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
		float a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 峰值滤波器（参数均衡）
	BiquadCoeffs DesignPeaking(float cutoff, float Q, float gainDB)
	{
		float A = powf(10.0f, gainDB / 40.0f);
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);

		float a0 = 1.0f + alpha / A;
		float b0 = 1.0f + alpha * A;
		float b1 = -2.0f * cosw0;
		float b2 = 1.0f - alpha * A;
		float a1 = -2.0f * cosw0;
		float a2 = 1.0f - alpha / A;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}

	// 带通滤波器
	BiquadCoeffs DesignBPF(float cutoff, float Q)
	{
		float w0 = 2.0f * M_PI * cutoff / sampleRate;
		float cosw0 = cosf(w0);
		float sinw0 = sinf(w0);
		float alpha = sinw0 / (2.0f * Q);

		float a0 = 1.0f + alpha;
		float b0 = alpha;
		float b1 = 0.0f;
		float b2 = -alpha;
		float a1 = -2.0f * cosw0;
		float a2 = 1.0f - alpha;

		return BiquadCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
	}
};*/