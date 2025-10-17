#pragma once

#include <JuceHeader.h>

class MenuTable : public juce::Component
{
public:
    MenuTable();
    ~MenuTable() override;

    // �������Ͳ˵���ɱ�����汾��
    template<typename... Components>
    void addMenuAndComponent(const juce::String& menuName, Components*... components)
    {
        auto* pageInfo = new PageInfo();
        pageInfo->name = menuName;

        // �����������ӵ�ҳ��
        addComponentsToPage(pageInfo, components...);

        pages.add(pageInfo);

        floatingMenu->addMenuItem(menuName);

        // ����ǵ�һ��ҳ�棬�Զ���ʾ
        if (pages.size() == 1)
        {
            switchToPage(0);
        }
    }

    // �л���ָ��ҳ��
    void switchToPage(int pageIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class CustomMenuButton : public juce::Component
    {
    public:
        CustomMenuButton(const juce::String& text, std::function<void()> callback);

        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        int getRequiredWidth() const;
        void setSelected(bool shouldBeSelected);

    private:
        juce::String buttonText;
        std::function<void()> onClick;
        bool isHovered;
        bool isPressed;
        bool isSelected;

        static constexpr int horizontalPadding = 20;
        static constexpr int verticalPadding = 8;
    };

    class FloatingMenu : public juce::Component
    {
    public:
        FloatingMenu(MenuTable& owner);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        void addMenuItem(const juce::String& name);
        void updateMenuLayout();
        void setSelectedButton(int index);

    private:
        MenuTable& owner;
        juce::OwnedArray<CustomMenuButton> menuButtons;
        juce::Point<int> dragOffset;
        bool isDraggingMenu;

        static constexpr int menuPadding = 10;
        static constexpr int buttonSpacing = 5;
        static constexpr int menuHeight = 45;
    };

    struct PageInfo
    {
        juce::Array<juce::Component*> components;
        juce::String name;
    };

    // �����������ݹ�������
    template<typename T, typename... Rest>
    void addComponentsToPage(PageInfo* pageInfo, T* first, Rest*... rest)
    {
        if (first != nullptr)
        {
            pageInfo->components.add(first);
            addChildComponent(first);
            first->setBounds(getLocalBounds());
        }
        addComponentsToPage(pageInfo, rest...);
    }

    // �ݹ���ֹ����
    void addComponentsToPage(PageInfo* pageInfo) {}

    juce::OwnedArray<PageInfo> pages;
    int currentPageIndex;
    std::unique_ptr<FloatingMenu> floatingMenu;

    friend class FloatingMenu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuTable)
};