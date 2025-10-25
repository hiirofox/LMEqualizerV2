#pragma once
#include <JuceHeader.h>
#include <math.h>
#include "../dsp/equalizer.h"

#pragma once
#include <JuceHeader.h>
#include <math.h>
#include "../dsp/equalizer.h"
class EqualizerUI : public juce::Component
{
public:
	// ��������
	static constexpr float MIN_FREQ = 10.01f;
	static constexpr float MAX_FREQ = 24000.0f;
	static constexpr float MIN_DB = -30.0f;
	static constexpr float MAX_DB = 30.0f;
	static constexpr float BORDER_WIDTH = 2.5f;
	static constexpr float NODE_RADIUS = 6.0f;
	static constexpr float NODE_LINE_WIDTH = 2.0f;
	static constexpr float RESPONSE_LINE_WIDTH = 2.0f;
	static constexpr float SELECTED_LINE_WIDTH = 1.0f;
	static constexpr juce::uint32 BORDER_COLOR = 0xff00ff00;
	static constexpr juce::uint32 NODE_COLOR = 0xff00ff00;
	static constexpr juce::uint32 RESPONSE_COLOR = 0xff7777ff;
	static constexpr juce::uint32 SELECTED_COLOR = 0xff007700;
	static constexpr juce::uint32 GRID_COLOR = 0xff808080;
	static constexpr float Q_WHEEL_SENSITIVITY = 0.1f;
	static constexpr float MIN_Q = 0.1f;
	static constexpr float MAX_Q = 40.0f;
	// ���캯��
	EqualizerUI(Equalizer& eq) : equalizer(eq), selectedNodeId(-1), isDragging(false),
		isEditingLabel(false), editingNodeId(-1), editingLabelType(LABEL_NONE)
	{

		// ��ʼ���ı��༭��
		addChildComponent(labelEditor);
		labelEditor.setJustification(juce::Justification::centred);
		labelEditor.setBorder(juce::BorderSize<int>(2));
		labelEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
		labelEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.8f));
		labelEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white);

		labelEditor.onReturnKey = [this]() { finishLabelEditing(true); };
		labelEditor.onEscapeKey = [this]() { finishLabelEditing(false); };
		labelEditor.onFocusLost = [this]() { finishLabelEditing(true); };

	}
	// ��дpaint����
	void paint(juce::Graphics& g) override
	{
		auto bounds = getLocalBounds();
		// �����ڲ��������򣨼�ȥ�߿�
		auto innerBounds = bounds.toFloat().reduced(BORDER_WIDTH);
		// ��������ͱ�ǩ
		drawGrid(g, innerBounds);
		// ����ѡ�нڵ��Ƶ����Ӧ���Ȼ��ƣ��ᱻ������Ӧ�ڵ���
		if (selectedNodeId >= 0 && equalizer.IsNodeActive(selectedNodeId))
		{
			drawSelectedNodeResponse(g, innerBounds);
		}
		// ����Ƶ����Ӧ
		drawFrequencyResponse(g, innerBounds);
		// ���ƽڵ�
		drawNodes(g, innerBounds);
		// ���Ʊ߿�
		g.setColour(juce::Colour(BORDER_COLOR));
		g.drawRect(bounds.toFloat(), BORDER_WIDTH);
	}
	// ����¼�����
	void mouseDown(const juce::MouseEvent& event) override
	{
		// ������ڱ༭��ǩ����������ط������༭
		if (isEditingLabel && !labelEditor.getBounds().contains(event.getPosition().toInt()))
		{
			finishLabelEditing(true);
			return;
		}
		if (event.mods.isRightButtonDown())
		{
			handleRightClick(event);
			return;
		}
		auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);

		// ����Ƿ����˱�ǩ
		int labelNodeId;
		LabelType labelType;
		if (getLabelAtPosition(event.position, bounds, labelNodeId, labelType))
		{
			startLabelEditing(labelNodeId, labelType);
			return;
		}
		int nodeId = getNodeAtPosition(event.position, bounds);
		if (nodeId >= 0)
		{
			selectedNodeId = nodeId;
			isDragging = true;
			dragStartPos = event.position;
			repaint();
		}
		else
		{
			selectedNodeId = -1;
			repaint();
		}
	}
	void mouseDrag(const juce::MouseEvent& event) override
	{
		if (isDragging && selectedNodeId >= 0)
		{
			auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);
			auto freq = positionToFrequency(event.position.x, bounds);
			auto gain = positionToGain(event.position.y, bounds);
			equalizer.UpdateNodeFreqGain(selectedNodeId, freq, gain);
			repaint();
		}
	}
	void mouseUp(const juce::MouseEvent& event) override
	{
		isDragging = false;
	}
	void mouseDoubleClick(const juce::MouseEvent& event) override
	{
		auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);
		int nodeId = getNodeAtPosition(event.position, bounds);
		if (nodeId >= 0)
		{
			// ˫���ڵ㣬��������Ϊ0dB
			equalizer.ResetNodeGain(nodeId);
			repaint();
		}
		else
		{
			// ˫���հ���������½ڵ�
			auto freq = positionToFrequency(event.position.x, bounds);
			auto gain = positionToGain(event.position.y, bounds);
			int newNodeId = equalizer.AddNode(MODE_PEAKING, freq, 1.0f, gain);
			selectedNodeId = newNodeId;
			repaint();
		}
	}
	void mouseWheelMove(const juce::MouseEvent& event,
		const juce::MouseWheelDetails& wheel) override
	{
		//if (selectedNodeId >= 0)
		{
			auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);
			int nodeId = getNodeAtPosition(event.position, bounds);
			//if (nodeId == selectedNodeId)
			if (nodeId >= 0)
			{
				auto node = equalizer.GetNode(nodeId);
				float newQ = node.q + wheel.deltaY * Q_WHEEL_SENSITIVITY * 10.0f;
				newQ = juce::jlimit(MIN_Q, MAX_Q, newQ);
				equalizer.UpdateNodeQ(nodeId, newQ);
				repaint();
			}
		}
	}
	// ����ѡ�еĽڵ�
	void setSelectedNode(int nodeId)
	{
		selectedNodeId = nodeId;
		repaint();
	}
	// ��ȡѡ�еĽڵ�ID
	int getSelectedNode() const
	{
		return selectedNodeId;
	}
private:
	// ��ǩ����ö��
	enum LabelType
	{
		LABEL_NONE,
		LABEL_FREQUENCY,
		LABEL_GAIN
	};
	Equalizer& equalizer;
	int selectedNodeId;
	bool isDragging;
	juce::Point<float> dragStartPos;

	// ��ǩ�༭��س�Ա����
	juce::TextEditor labelEditor;
	bool isEditingLabel;
	int editingNodeId;
	LabelType editingLabelType;
	// ��ʼ�༭��ǩ
	void startLabelEditing(int nodeId, LabelType labelType)
	{
		setWantsKeyboardFocus(true);

		if (isEditingLabel)
			finishLabelEditing(true);
		isEditingLabel = true;
		editingNodeId = nodeId;
		editingLabelType = labelType;
		auto node = equalizer.GetNode(nodeId);
		auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);

		float nodeX = frequencyToPosition(node.cutoff, bounds);
		float nodeY = gainToPosition(node.gainDB, bounds);
		// ���ñ༭���ı� - �޸�Ƶ�ʵ�λ����
		juce::String currentText;
		if (labelType == LABEL_FREQUENCY)
		{
			// ��������ʾ��ǩһ�µĸ�ʽ
			if (node.cutoff >= 1000)
			{
				float kFreq = node.cutoff / 1000.0f;
				currentText = juce::String(kFreq, 2) + "k";  // ���k��λ
			}
			else
			{
				currentText = juce::String((int)node.cutoff);
			}
		}
		else if (labelType == LABEL_GAIN)
		{
			currentText = juce::String(node.gainDB, 1);
		}
		labelEditor.setText(currentText);

		// ����༭��λ�� - �����ߴ��Ը���ƥ���ı�
		juce::Rectangle<int> editorBounds;
		int editorWidth = 60;  // ���ӿ�������ɸ����ַ�
		int editorHeight = 18; // ��΢���Ӹ߶�

		if (labelType == LABEL_FREQUENCY)
		{
			float labelY = (node.gainDB >= 0) ?
				(nodeY - NODE_RADIUS - 20) :
				(nodeY + NODE_RADIUS + 5);
			editorBounds = juce::Rectangle<int>((int)nodeX - editorWidth / 2, (int)labelY - 2,
				editorWidth, editorHeight);
		}
		else if (labelType == LABEL_GAIN)
		{
			float labelY = (node.gainDB >= 0) ?
				(nodeY - NODE_RADIUS - 20 - 16) :
				(nodeY + NODE_RADIUS + 5 + 16);
			editorBounds = juce::Rectangle<int>((int)nodeX - editorWidth / 2, (int)labelY - 2,
				editorWidth, editorHeight);
		}
		labelEditor.setBounds(editorBounds);
		labelEditor.setVisible(true);
		labelEditor.grabKeyboardFocus();
		labelEditor.selectAll();
	}
	// �޸�finishLabelEditing��������ȷ����Ƶ�ʵ�λ
	void finishLabelEditing(bool applyChanges)
	{
		setWantsKeyboardFocus(false);

		if (!isEditingLabel)
			return;
		if (applyChanges)
		{
			juce::String text = labelEditor.getText().trim();

			if (editingLabelType == LABEL_FREQUENCY)
			{
				// ����Ƶ������ - �Ľ���λ����
				float value;

				if (text.endsWithIgnoreCase("k"))
				{
					// �Ƴ�k��׺��ת��
					juce::String numberPart = text.dropLastCharacters(1);
					value = numberPart.getFloatValue() * 1000.0f;
				}
				else if (text.endsWithIgnoreCase("hz"))
				{
					// �Ƴ�hz��׺
					juce::String numberPart = text.dropLastCharacters(2);
					value = numberPart.getFloatValue();
				}
				else
				{
					// �����֣��ж��Ƿ���Ҫת��
					value = text.getFloatValue();
					// ����������С��100�����֣������û���ͼ����kHz
					// ���������ǰ�������˼��⣬�û�����ʲô����ʲô
				}

				value = juce::jlimit(MIN_FREQ, MAX_FREQ, value);
				auto node = equalizer.GetNode(editingNodeId);
				equalizer.UpdateNodeFreqGain(editingNodeId, value, node.gainDB);
			}
			else if (editingLabelType == LABEL_GAIN)
			{
				// ������������ - �Ƴ����ܵ�dB��׺
				juce::String cleanText = text;
				if (text.endsWithIgnoreCase("db"))
				{
					cleanText = text.dropLastCharacters(2).trim();
				}

				float value = cleanText.getFloatValue();
				value = juce::jlimit(MIN_DB, MAX_DB, value);
				auto node = equalizer.GetNode(editingNodeId);
				equalizer.UpdateNodeFreqGain(editingNodeId, node.cutoff, value);
			}
		}
		labelEditor.setVisible(false);
		isEditingLabel = false;
		editingNodeId = -1;
		editingLabelType = LABEL_NONE;
		repaint();
	}
	// ���λ���Ƿ��ڱ�ǩ��
	bool getLabelAtPosition(const juce::Point<float>& pos, const juce::Rectangle<float>& bounds,
		int& nodeId, LabelType& labelType)
	{
		auto activeIds = equalizer.GetActiveNodeIds();
		for (int id : activeIds)
		{
			auto node = equalizer.GetNode(id);
			float nodeX = frequencyToPosition(node.cutoff, bounds);
			float nodeY = gainToPosition(node.gainDB, bounds);
			// ���Ƶ�ʱ�ǩ
			float freqLabelY = (node.gainDB >= 0) ?
				(nodeY - NODE_RADIUS - 20) :
				(nodeY + NODE_RADIUS + 5);
			juce::Rectangle<float> freqLabelRect(nodeX - 25, freqLabelY, 50, 15);
			if (freqLabelRect.contains(pos))
			{
				nodeId = id;
				labelType = LABEL_FREQUENCY;
				return true;
			}
			// ��������ǩ
			float gainLabelY = (node.gainDB >= 0) ?
				(nodeY - NODE_RADIUS - 20 - 16) :
				(nodeY + NODE_RADIUS + 5 + 16);
			juce::Rectangle<float> gainLabelRect(nodeX - 25, gainLabelY, 50, 15);
			if (gainLabelRect.contains(pos))
			{
				nodeId = id;
				labelType = LABEL_GAIN;
				return true;
			}
		}
		nodeId = -1;
		labelType = LABEL_NONE;
		return false;
	}
	// ����˽�з������ֲ���...
	// [�����������֮ǰ��˽�з�������λ��ת�����������ƺ�����]

	// λ��ת������ - ��Ϊ��������
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

	float gainToPosition(float gain, const juce::Rectangle<float>& bounds) const
	{
		float ratio = (gain - MIN_DB) / (MAX_DB - MIN_DB);
		return bounds.getBottom() - ratio * bounds.getHeight();
	}

	float positionToGain(float y, const juce::Rectangle<float>& bounds) const
	{
		float ratio = (bounds.getBottom() - y) / bounds.getHeight();
		ratio = juce::jlimit(0.0f, 1.0f, ratio);
		return MIN_DB + ratio * (MAX_DB - MIN_DB);
	}
	// ��������ͱ�ǩ - �Ľ��Ķ����̶�
	void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		// ����Ƶ�������ߺͱ�ǩ�������̶ȣ�
		drawFrequencyGrid(g, bounds);
		// �������������ߺͱ�ǩ
		drawGainGrid(g, bounds);
	}

	void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		// ��ҪƵ�ʿ̶� (��̶�)
		std::vector<float> majorFreqs = {
			10,100,1000,10000,100000
		};
		// ��ҪƵ�ʿ̶� (С�̶�)
		std::vector<float> minorFreqs;
		for (int i = 2; i <= 9; ++i)
		{
			minorFreqs.push_back(i * 10);
			minorFreqs.push_back(i * 100);
			minorFreqs.push_back(i * 1000);
			minorFreqs.push_back(i * 10000);
		}
		// ����С�̶�
		g.setColour(juce::Colour(GRID_COLOR).withAlpha(0.2f));
		for (float freq : minorFreqs)
		{
			if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			{
				float x = frequencyToPosition(freq, bounds);
				g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
			}
		}
		// ���ƴ�̶Ⱥͱ�ǩ
		g.setColour(juce::Colour(GRID_COLOR).withAlpha(0.5f));
		for (float freq : majorFreqs)
		{
			if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			{
				float x = frequencyToPosition(freq, bounds);
				g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
			}
		}
		majorFreqs = {
			10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000
		};
		for (float freq : majorFreqs)
		{
			if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			{
				float x = frequencyToPosition(freq, bounds);
				// ����Ƶ�ʱ�ǩ���ײ���
				juce::String label = formatFrequencyLabel(freq);
				g.setColour(juce::Colour(0xffffffff).withAlpha(0.75f));
				g.drawText(label, x - 20, bounds.getBottom() - 20, 40, 15, juce::Justification::centred);
				g.setColour(juce::Colour(GRID_COLOR).withAlpha(0.5f));
			}
		}
	}

	void drawGainGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		// ����̶�
		std::vector<float> gains = { -40, -30, -20, -10, 0, 10, 20, 30, 40 };
		g.setColour(juce::Colour(GRID_COLOR).withAlpha(0.5f));
		for (float gain : gains)
		{
			if (gain > MIN_DB && gain < MAX_DB)
			{
				float y = gainToPosition(gain, bounds);
				g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
				// ���������ǩ���Ҳࣩ������dB��λ
				juce::String label = (gain > 0 ? "+" : "") + juce::String((int)gain);
				g.setColour(juce::Colour(0xffffffff).withAlpha(0.75f));
				g.drawText(label, bounds.getRight() - 32, y - 8, 30, 16,
					juce::Justification::centred);
				g.setColour(juce::Colour(GRID_COLOR).withAlpha(0.5f));
			}
		}
	}
	// ��ʽ��Ƶ�ʱ�ǩ
	juce::String formatFrequencyLabel(float freq) const
	{
		if (freq >= 1000)
		{
			float kFreq = freq / 1000.0f;
			if (kFreq == (int)kFreq)
				return juce::String((int)kFreq) + "k";
			else
				return juce::String(kFreq, 1) + "k";
		}
		else
		{
			return juce::String((int)freq);
		}
	}
	// ��ʽ���ڵ�Ƶ�ʱ�ǩ (���ڽڵ���ʾ)
	juce::String formatNodeFrequencyLabel(float freq) const
	{
		if (freq >= 1000)
		{
			float kFreq = freq / 1000.0f;
			return juce::String(kFreq, 2) + "k";
		}
		else
		{
			return juce::String((int)freq);
		}
	}

	juce::String formatNodeDBLabel(float dB) const
	{
		return juce::String(dB, 1) + "dB";
	}
	// ����Ƶ����Ӧ
	void drawFrequencyResponse(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		juce::Path responsePath;
		bool firstPoint = true;
		int numPoints = (int)bounds.getWidth();
		for (int i = 0; i < numPoints; ++i)
		{
			float x = bounds.getX() + i;
			double freq = positionToFrequency(x, bounds);
			auto response = equalizer.GetTotalFrequencyResponse(freq);
			float gainDB = 20.0f * std::log10(std::abs(response));
			float y = gainToPosition(gainDB, bounds);
			if (y < 0)y = 0;
			if (y > getHeight())y = getHeight();
			if (firstPoint)
			{
				responsePath.startNewSubPath(x, y);
				firstPoint = false;
			}
			else
			{
				responsePath.lineTo(x, y);
			}
		}
		g.setColour(juce::Colour(RESPONSE_COLOR));
		g.strokePath(responsePath, juce::PathStrokeType(RESPONSE_LINE_WIDTH));
	}
	// ����ѡ�нڵ��Ƶ����Ӧ
	void drawSelectedNodeResponse(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		juce::Path responsePath;
		bool firstPoint = true;
		int numPoints = (int)bounds.getWidth();
		for (int i = 0; i < numPoints; ++i)
		{
			float x = bounds.getX() + i;
			float freq = positionToFrequency(x, bounds);
			auto response = equalizer.GetFrequencyResponse(selectedNodeId, freq);
			float gainDB = 20.0f * std::log10(std::abs(response));
			float y = gainToPosition(gainDB, bounds);
			if (firstPoint)
			{
				responsePath.startNewSubPath(x, y);
				firstPoint = false;
			}
			else
			{
				responsePath.lineTo(x, y);
			}
		}
		g.setColour(juce::Colour(SELECTED_COLOR));
		g.strokePath(responsePath, juce::PathStrokeType(SELECTED_LINE_WIDTH));
	}
	// ���ƽڵ�
	void drawNodes(juce::Graphics& g, const juce::Rectangle<float>& bounds)
	{
		auto activeIds = equalizer.GetActiveNodeIds();
		for (int id : activeIds)
		{
			auto node = equalizer.GetNode(id);
			float x = frequencyToPosition(node.cutoff, bounds);
			float y = gainToPosition(node.gainDB, bounds);
			// ���ƽڵ�ԲȦ
			g.setColour(juce::Colour(NODE_COLOR));
			g.drawEllipse(x - NODE_RADIUS, y - NODE_RADIUS,
				NODE_RADIUS * 2, NODE_RADIUS * 2, NODE_LINE_WIDTH);
			// �����ѡ�еĽڵ㣬���ԲȦ
			if (id == selectedNodeId)
			{
				g.setColour(juce::Colour(NODE_COLOR).withAlpha(0.3f));
				g.fillEllipse(x - NODE_RADIUS, y - NODE_RADIUS,
					NODE_RADIUS * 2, NODE_RADIUS * 2);
			}
			// ������ڱ༭״̬�����Ʊ�ǩ
			if (!isEditingLabel || editingNodeId != id)
			{
				drawNodeFrequencyLabel(g, bounds, x, y, node.cutoff, node.gainDB);
			}
		}
	}
	// ���ƽڵ�Ƶ�ʱ�ǩ
	void drawNodeFrequencyLabel(juce::Graphics& g, const juce::Rectangle<float>& bounds,
		float nodeX, float nodeY, float freq, float gainDB)
	{
		juce::String labelf = formatNodeFrequencyLabel(freq);
		juce::String labeldb = formatNodeDBLabel(gainDB);
		float labelY1, labelY2;
		if (gainDB >= 0)
		{
			// �ڽڵ��Ϸ���ʾ
			labelY1 = nodeY - NODE_RADIUS - 20;
			labelY2 = nodeY - NODE_RADIUS - 20 - 16;
		}
		else
		{
			// �ڽڵ��·���ʾ
			labelY1 = nodeY + NODE_RADIUS + 5;
			labelY2 = nodeY + NODE_RADIUS + 5 + 16;
		}
		g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(0.95f));
		g.drawText(labelf, nodeX - 25, labelY1, 50, 15, juce::Justification::centred);
		g.setColour(juce::Colour(0xFF00FFFF).withAlpha(0.95f));
		g.drawText(labeldb, nodeX - 25, labelY2, 50, 15, juce::Justification::centred);
	}
	// ��ȡλ�ô��Ľڵ�ID
	int getNodeAtPosition(const juce::Point<float>& pos, const juce::Rectangle<float>& bounds)
	{
		auto activeIds = equalizer.GetActiveNodeIds();
		for (int id : activeIds)
		{
			auto node = equalizer.GetNode(id);
			float x = frequencyToPosition(node.cutoff, bounds);
			float y = gainToPosition(node.gainDB, bounds);
			float distance = pos.getDistanceFrom(juce::Point<float>(x, y));
			if (distance <= NODE_RADIUS + 5) // ���һЩ�ݲ�
			{
				return id;
			}
		}
		return -1;
	}
	// �����Ҽ����
	void handleRightClick(const juce::MouseEvent& event)
	{
		auto bounds = getLocalBounds().toFloat().reduced(BORDER_WIDTH);
		int nodeId = getNodeAtPosition(event.position, bounds);
		if (nodeId >= 0)
		{
			showNodeContextMenu(nodeId, event.getScreenPosition());
		}
	}
	// ��ʾ�ڵ������Ĳ˵�
	void showNodeContextMenu(int nodeId, juce::Point<int> screenPos)
	{
		juce::PopupMenu menu;
		// ����˲�������ѡ��
		menu.addSectionHeader("Filter Type");
		for (int i = 0; i < Equalizer::GetNumFilterModes(); ++i)
		{
			menu.addItem(100 + i, Equalizer::GetFilterModeName(i),
				true, equalizer.GetNode(nodeId).mode == i);
		}
		menu.addSeparator();
		menu.addItem(200, "Delete Node");
		menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
			juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
			[this, nodeId](int result)
			{
				if (result >= 100 && result < 200)
				{
					// �����˲�������
					int newMode = result - 100;
					equalizer.SetNodeMode(nodeId, newMode);
					repaint();
				}
				else if (result == 200)
				{
					// ɾ���ڵ�
					if (selectedNodeId == nodeId)
						selectedNodeId = -1;
					equalizer.DeleteNode(nodeId);
					repaint();
				}
			});
	}
};