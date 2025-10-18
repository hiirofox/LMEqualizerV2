#pragma once

#include <vector>
#include <complex>
#include "biquad.h"
#include "svf.h"

enum FilterMode {
	MODE_LOWPASS = 0,
	MODE_HIGHPASS,
	MODE_BANDPASS,
	MODE_PEAKING,
	MODE_LOWSHELF,
	MODE_HIGHSHELF
};

struct FilterNode {
	int mode;
	float cutoff;
	float q;
	float gainDB;
	bool active;
};

class Equalizer
{
private:
	static std::complex<float> TransferFunction(const BiquadCoeffs& coeffs, float w)
	{
		w *= M_PI;
		std::complex<float> z{ cosf(w), sinf(w) };
		std::complex<float> z2 = z * z;
		std::complex<float> numerator = coeffs.b0 + coeffs.b1 * z + coeffs.b2 * z2;
		std::complex<float> denominator = 1.0f + coeffs.a1 * z + coeffs.a2 * z2;
		return numerator / denominator;
	}


	BiquadDesigner designer;
	std::vector<BiquadCoeffs> coeffs;
	std::vector<SVF> svfsLeft;   // �������˲���
	std::vector<SVF> svfsRight;  // �������˲���
	std::vector<FilterNode> nodes;
	std::vector<int> freeIds;
	int numNodes = 0;

	BiquadCoeffs DesignFilter(int mode, float cutoff, float q, float gainDB)
	{
		switch (mode) {
		case MODE_LOWPASS:
			return designer.DesignLPF(cutoff, q);
		case MODE_HIGHPASS:
			return designer.DesignHPF(cutoff, q);
		case MODE_BANDPASS:
			return designer.DesignBPF(cutoff, q);
		case MODE_PEAKING:
			return designer.DesignPeaking(cutoff, q, gainDB);
		case MODE_LOWSHELF:
			return designer.DesignLowshelf(cutoff, q, gainDB);
		case MODE_HIGHSHELF:
			return designer.DesignHighshelf(cutoff, q, gainDB);
		default:
			return { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		}
	}

public:
	Equalizer(float sampleRate = 48000.0f) : designer(sampleRate) {}

	void SetSampleRate(float sr)
	{
		designer.SetSampleRate(sr);
		// ���¼�������ϵ��
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				coeffs[i] = DesignFilter(nodes[i].mode, nodes[i].cutoff,
					nodes[i].q, nodes[i].gainDB);
				svfsLeft[i].SetBiquadCoeffs(coeffs[i]);
				svfsRight[i].SetBiquadCoeffs(coeffs[i]);
			}
		}
	}


	inline float ProcessSample(float in)
	{
		float out = in;
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				out = svfsLeft[i].ProcessSample(out);
			}
		}
		return out;
	}

	inline void ProcessSampleStereo(float inL, float inR, float& outL, float& outR)
	{
		outL = inL;
		outR = inR;
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				outL = svfsLeft[i].ProcessSample(outL);
				outR = svfsRight[i].ProcessSample(outR);
			}
		}
	}

	void ProcessBlock(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
	{
		for (int s = 0; s < numSamples; ++s) {
			float tempL = inL[s];
			float tempR = inR[s];

			for (int i = 0; i < numNodes; ++i) {
				if (nodes[i].active) {
					tempL = svfsLeft[i].ProcessSample(tempL);
					tempR = svfsRight[i].ProcessSample(tempR);
				}
			}

			outL[s] = tempL;
			outR[s] = tempR;
		}
	}


	int AddNode(int mode, float cutoff, float q, float gainDB)
	{
		int id;
		if (!freeIds.empty()) {
			// ���ÿ���ID
			id = freeIds.back();
			freeIds.pop_back();
			nodes[id] = { mode, cutoff, q, gainDB, true };
		}
		else {
			// �����½ڵ�
			id = numNodes++;
			nodes.push_back({ mode, cutoff, q, gainDB, true });
			coeffs.push_back({ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f });
			svfsLeft.push_back(SVF());
			svfsRight.push_back(SVF());
		}

		coeffs[id] = DesignFilter(mode, cutoff, q, gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
		return id;
	}

	void SetNode(int id, int mode, float cutoff, float q, float gainDB)
	{
		if (id < 0 || id >= numNodes) return;

		nodes[id].mode = mode;
		nodes[id].cutoff = cutoff;
		nodes[id].q = q;
		nodes[id].gainDB = gainDB;
		nodes[id].active = true;

		coeffs[id] = DesignFilter(mode, cutoff, q, gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
	}

	void DeleteNode(int id)
	{
		if (id < 0 || id >= numNodes) return;

		nodes[id].active = false;
		coeffs[id] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
		freeIds.push_back(id);
	}

	std::complex<float> GetFrequencyResponse(int id, float freq)
	{
		if (id < 0 || id >= numNodes || !nodes[id].active) {
			return std::complex<float>(1.0f, 0.0f);
		}
		float f0 = freq / designer.GetSampleRate();
		float w = 2.0f * f0;
		return TransferFunction(coeffs[id], w);
	}

	std::complex<float> GetTotalFrequencyResponse(float freq)
	{
		std::complex<float> total(1.0f, 0.0f);
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				total *= GetFrequencyResponse(i, freq);
			}
		}
		return total;
	}

	const FilterNode& GetNode(int id) const { return nodes[id]; }
	int GetNumNodes() const { return numNodes; }
	bool IsNodeActive(int id) const {
		return id >= 0 && id < numNodes && nodes[id].active;
	}
	float GetSampleRate() const { return designer.GetSampleRate(); }

	// ��ȡ���л�ڵ��ID
	std::vector<int> GetActiveNodeIds() const
	{
		std::vector<int> activeIds;
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				activeIds.push_back(i);
			}
		}
		return activeIds;
	}
	// ���ýڵ���������ı�ģʽ��
	void UpdateNodeFreqGain(int id, float cutoff, float gainDB)
	{
		if (id < 0 || id >= numNodes || !nodes[id].active) return;

		nodes[id].cutoff = cutoff;
		nodes[id].gainDB = gainDB;

		coeffs[id] = DesignFilter(nodes[id].mode, nodes[id].cutoff,
			nodes[id].q, nodes[id].gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
	}
	// ���ýڵ�Qֵ
	void UpdateNodeQ(int id, float q)
	{
		if (id < 0 || id >= numNodes || !nodes[id].active) return;

		nodes[id].q = juce::jlimit(0.1f, 20.0f, q);

		coeffs[id] = DesignFilter(nodes[id].mode, nodes[id].cutoff,
			nodes[id].q, nodes[id].gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
	}
	// ���ýڵ�����Ϊ0dB
	void ResetNodeGain(int id)
	{
		if (id < 0 || id >= numNodes || !nodes[id].active) return;

		nodes[id].gainDB = 0.0f;

		coeffs[id] = DesignFilter(nodes[id].mode, nodes[id].cutoff,
			nodes[id].q, nodes[id].gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
	}
	// ���ýڵ�ģʽ
	void SetNodeMode(int id, int mode)
	{
		if (id < 0 || id >= numNodes || !nodes[id].active) return;

		nodes[id].mode = mode;

		coeffs[id] = DesignFilter(nodes[id].mode, nodes[id].cutoff,
			nodes[id].q, nodes[id].gainDB);
		svfsLeft[id].SetBiquadCoeffs(coeffs[id]);
		svfsRight[id].SetBiquadCoeffs(coeffs[id]);
	}

	// ��ȡ�����˲���ģʽ������
	static const char* GetFilterModeName(int mode) {
		static const char* names[] = {
			"Low Pass", "High Pass", "Band Pass",
			"Peaking", "Low Shelf", "High Shelf"
		};
		if (mode >= 0 && mode < 6) return names[mode];
		return "Unknown";
	}

	// ��ȡ���õ��˲���ģʽ����
	static int GetNumFilterModes() { return 6; }




	// ���ݳ־û��ӿ�
	struct EqualizerState {
		float sampleRate;
		int numNodes;
		std::vector<FilterNode> activeNodes;
		std::vector<int> nodeIds;
	};
	// ��ȡ��ǰ״̬���ڳ־û�
	EqualizerState GetState() const {
		EqualizerState state;
		state.sampleRate = designer.GetSampleRate();
		state.numNodes = numNodes;

		// �������л�ڵ�
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				state.activeNodes.push_back(nodes[i]);
				state.nodeIds.push_back(i);
			}
		}

		return state;
	}
	// ��״̬�ָ�
	void SetState(const EqualizerState& state) {
		// �������״̬
		Clear();

		// ���ò�����
		designer.SetSampleRate(state.sampleRate);

		// �ָ��ڵ�
		for (size_t i = 0; i < state.activeNodes.size(); ++i) {
			const FilterNode& node = state.activeNodes[i];
			AddNode(node.mode, node.cutoff, node.q, node.gainDB);
		}
	}
	// ������нڵ�
	void Clear() {
		for (int i = 0; i < numNodes; ++i) {
			if (nodes[i].active) {
				DeleteNode(i);
			}
		}
		freeIds.clear();
		numNodes = 0;
		nodes.clear();
		coeffs.clear();
		svfsLeft.clear();
		svfsRight.clear();
	}
	// ���л�Ϊ�ַ������򵥸�ʽ��
	std::string SerializeToString() const {
		std::ostringstream oss;
		oss << designer.GetSampleRate() << "|";

		std::vector<int> activeIds = GetActiveNodeIds();
		oss << activeIds.size() << "|";

		for (int id : activeIds) {
			const FilterNode& node = nodes[id];
			oss << node.mode << "," << node.cutoff << ","
				<< node.q << "," << node.gainDB << "|";
		}

		return oss.str();
	}
	// ���ַ��������л�
	bool DeserializeFromString(const std::string& data) {
		std::istringstream iss(data);
		std::string token;

		try {
			// ��ȡ������
			if (!std::getline(iss, token, '|')) return false;
			float sampleRate = std::stof(token);

			// ��ȡ�ڵ�����
			if (!std::getline(iss, token, '|')) return false;
			int nodeCount = std::stoi(token);

			// �������״̬
			Clear();
			designer.SetSampleRate(sampleRate);

			// ��ȡÿ���ڵ�
			for (int i = 0; i < nodeCount; ++i) {
				if (!std::getline(iss, token, '|')) return false;

				std::istringstream nodeStream(token);
				std::string param;

				if (!std::getline(nodeStream, param, ',')) return false;
				int mode = std::stoi(param);

				if (!std::getline(nodeStream, param, ',')) return false;
				float cutoff = std::stof(param);

				if (!std::getline(nodeStream, param, ',')) return false;
				float q = std::stof(param);

				if (!std::getline(nodeStream, param, ',')) return false;
				float gainDB = std::stof(param);

				AddNode(mode, cutoff, q, gainDB);
			}

			return true;
		}
		catch (...) {
			return false;
		}
	}
};