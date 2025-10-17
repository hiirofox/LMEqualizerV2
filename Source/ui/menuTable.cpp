#include "menuTable.h"

// ============ ʵ�ֲ��� ============

MenuTable::MenuTable() : currentPageIndex(-1)
{
    floatingMenu = std::make_unique<FloatingMenu>(*this);
    addAndMakeVisible(floatingMenu.get());
    floatingMenu->setAlwaysOnTop(true);
}

MenuTable::~MenuTable()
{
    floatingMenu.reset();
    for (auto* page : pages)
    {
        for (auto* comp : page->components)
        {
            if (comp != nullptr)
                removeChildComponent(comp);
        }
    }
}

void MenuTable::switchToPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= pages.size())
        return;

    // ���ص�ǰҳ����������
    if (currentPageIndex >= 0 && currentPageIndex < pages.size())
    {
        for (auto* comp : pages[currentPageIndex]->components)
        {
            if (comp != nullptr)
                comp->setVisible(false);
        }
    }

    // ��ʾ��ҳ����������
    currentPageIndex = pageIndex;
    for (auto* comp : pages[currentPageIndex]->components)
    {
        if (comp != nullptr)
        {
            comp->setVisible(true);
            comp->toBack();
        }
    }

    // ���²˵���ť��ѡ��״̬
    floatingMenu->setSelectedButton(pageIndex);

    // ȷ���˵�ʼ�������ϲ�
    floatingMenu->toFront(false);
}

void MenuTable::paint(juce::Graphics& g)
{
    // ����MenuTable�߿�
    g.setColour(juce::Colour(0xFF00FF00));
    g.drawRect(getLocalBounds(), 2.5f);
}

void MenuTable::resized()
{
    // ��������ҳ�������������bounds
    for (auto* page : pages)
    {
        for (auto* comp : page->components)
        {
            if (comp != nullptr)
                comp->setBounds(getLocalBounds());
        }
    }

    // ���²˵�λ�úʹ�С
    if (floatingMenu != nullptr)
        floatingMenu->updateMenuLayout();
}

// ============ CustomMenuButton ʵ�� ============

MenuTable::CustomMenuButton::CustomMenuButton(const juce::String& text, std::function<void()> callback)
    : buttonText(text), onClick(callback), isHovered(false), isPressed(false), isSelected(false)
{
    setInterceptsMouseClicks(true, true);
}

void MenuTable::CustomMenuButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ���ư�ť����
    if (isPressed)
    {
        g.setColour(juce::Colour(0xFF003300));
        g.fillRect(bounds);
    }
    else if (isHovered)
    {
        g.setColour(juce::Colour(0xFF002200));
        g.fillRect(bounds);
    }
    else
    {
        g.setColour(juce::Colour(0xFF1a1a1a));
        g.fillRect(bounds);
    }

    // ���ư�ť�߿�
    g.setColour(juce::Colour(0xFF00AA00));
    g.drawRect(bounds, 1.0f);

    // ��������
    if (isSelected)
        g.setColour(juce::Colour(0xFF00FFFF)); // ѡ��״̬������ɫ
    else
        g.setColour(juce::Colour(0xFF00AAAA)); // δѡ��״̬������ɫ
    g.setFont(14.0f);
    g.drawText(buttonText, bounds, juce::Justification::centred);
}

void MenuTable::CustomMenuButton::mouseEnter(const juce::MouseEvent& e)
{
    isHovered = true;
    repaint();
}

void MenuTable::CustomMenuButton::mouseExit(const juce::MouseEvent& e)
{
    isHovered = false;
    repaint();
}

void MenuTable::CustomMenuButton::mouseDown(const juce::MouseEvent& e)
{
    isPressed = true;
    repaint();
}

void MenuTable::CustomMenuButton::mouseUp(const juce::MouseEvent& e)
{
    isPressed = false;
    repaint();

    if (isHovered && onClick)
        onClick();
}

int MenuTable::CustomMenuButton::getRequiredWidth() const
{
    juce::Font font(14.0f);
    return font.getStringWidth(buttonText) + horizontalPadding * 2;
}

void MenuTable::CustomMenuButton::setSelected(bool shouldBeSelected)
{
    if (isSelected != shouldBeSelected)
    {
        isSelected = shouldBeSelected;
        repaint();
    }
}

// ============ FloatingMenu ʵ�� ============

MenuTable::FloatingMenu::FloatingMenu(MenuTable& owner)
    : owner(owner), isDraggingMenu(false)
{
    setInterceptsMouseClicks(true, true);
}

void MenuTable::FloatingMenu::paint(juce::Graphics& g)
{
    // ���Ʋ˵�����
    g.fillAll(juce::Colour(0xFF1a1a1a));

    // ���Ʋ˵��߿�
    g.setColour(juce::Colour(0xFF00AA00));
    g.drawRect(getLocalBounds(), 1.0f);
}

void MenuTable::FloatingMenu::resized()
{
    int x = menuPadding;
    int y = menuPadding;

    for (auto* button : menuButtons)
    {
        int buttonWidth = button->getRequiredWidth();
        int buttonHeight = menuHeight - menuPadding * 2;
        button->setBounds(x, y, buttonWidth, buttonHeight);
        x += buttonWidth + buttonSpacing;
    }
}

void MenuTable::FloatingMenu::mouseDown(const juce::MouseEvent& e)
{
    // ����Ƿ����ڰ�ť����֮�⣨�հ�����
    bool clickedOnButton = false;
    for (auto* button : menuButtons)
    {
        if (button->getBounds().contains(e.getPosition()))
        {
            clickedOnButton = true;
            break;
        }
    }

    if (!clickedOnButton)
    {
        isDraggingMenu = true;
        dragOffset = e.getPosition();
        toFront(false);
    }
}

void MenuTable::FloatingMenu::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingMenu)
    {
        auto newPos = getPosition() + e.getPosition() - dragOffset;

        // �����ڸ������Χ��
        auto parentBounds = getParentComponent()->getLocalBounds();
        newPos.x = juce::jlimit(0, parentBounds.getWidth() - getWidth(), newPos.x);
        newPos.y = juce::jlimit(0, parentBounds.getHeight() - getHeight(), newPos.y);

        setTopLeftPosition(newPos);
    }
}

void MenuTable::FloatingMenu::mouseUp(const juce::MouseEvent& e)
{
    isDraggingMenu = false;
}

void MenuTable::FloatingMenu::addMenuItem(const juce::String& name)
{
    int buttonIndex = menuButtons.size();

    auto* button = new CustomMenuButton(name, [this, buttonIndex]()
        {
            owner.switchToPage(buttonIndex);
        });

    menuButtons.add(button);
    addAndMakeVisible(button);

    updateMenuLayout();
}

void MenuTable::FloatingMenu::updateMenuLayout()
{
    // ����˵�������ܿ��
    int totalWidth = menuPadding;
    for (auto* button : menuButtons)
    {
        totalWidth += button->getRequiredWidth() + buttonSpacing;
    }
    if (menuButtons.size() > 0)
        totalWidth -= buttonSpacing; // �Ƴ����һ����ť��ļ��
    totalWidth += menuPadding;

    // ����˵���û��λ�ã���ʼ�������Ͻ�
    if (getBounds().isEmpty())
    {
        setBounds(10, 10, totalWidth, menuHeight);
    }
    else
    {
        // ֻ���¿�ȣ����ֵ�ǰλ��
        setSize(totalWidth, menuHeight);
    }

    resized();
}

void MenuTable::FloatingMenu::setSelectedButton(int index)
{
    // ȡ�����а�ť��ѡ��״̬
    for (auto* button : menuButtons)
    {
        button->setSelected(false);
    }

    // ����ָ����ťΪѡ��״̬
    if (index >= 0 && index < menuButtons.size())
    {
        menuButtons[index]->setSelected(true);
    }
}