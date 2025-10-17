#include "menuTable.h"

// ============ 实现部分 ============

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

    // 隐藏当前页面的所有组件
    if (currentPageIndex >= 0 && currentPageIndex < pages.size())
    {
        for (auto* comp : pages[currentPageIndex]->components)
        {
            if (comp != nullptr)
                comp->setVisible(false);
        }
    }

    // 显示新页面的所有组件
    currentPageIndex = pageIndex;
    for (auto* comp : pages[currentPageIndex]->components)
    {
        if (comp != nullptr)
        {
            comp->setVisible(true);
            comp->toBack();
        }
    }

    // 更新菜单按钮的选中状态
    floatingMenu->setSelectedButton(pageIndex);

    // 确保菜单始终在最上层
    floatingMenu->toFront(false);
}

void MenuTable::paint(juce::Graphics& g)
{
    // 绘制MenuTable边框
    g.setColour(juce::Colour(0xFF00FF00));
    g.drawRect(getLocalBounds(), 2.5f);
}

void MenuTable::resized()
{
    // 更新所有页面中所有组件的bounds
    for (auto* page : pages)
    {
        for (auto* comp : page->components)
        {
            if (comp != nullptr)
                comp->setBounds(getLocalBounds());
        }
    }

    // 更新菜单位置和大小
    if (floatingMenu != nullptr)
        floatingMenu->updateMenuLayout();
}

// ============ CustomMenuButton 实现 ============

MenuTable::CustomMenuButton::CustomMenuButton(const juce::String& text, std::function<void()> callback)
    : buttonText(text), onClick(callback), isHovered(false), isPressed(false), isSelected(false)
{
    setInterceptsMouseClicks(true, true);
}

void MenuTable::CustomMenuButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 绘制按钮背景
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

    // 绘制按钮边框
    g.setColour(juce::Colour(0xFF00AA00));
    g.drawRect(bounds, 1.0f);

    // 绘制文字
    if (isSelected)
        g.setColour(juce::Colour(0xFF00FFFF)); // 选中状态：亮青色
    else
        g.setColour(juce::Colour(0xFF00AAAA)); // 未选中状态：暗青色
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

// ============ FloatingMenu 实现 ============

MenuTable::FloatingMenu::FloatingMenu(MenuTable& owner)
    : owner(owner), isDraggingMenu(false)
{
    setInterceptsMouseClicks(true, true);
}

void MenuTable::FloatingMenu::paint(juce::Graphics& g)
{
    // 绘制菜单背景
    g.fillAll(juce::Colour(0xFF1a1a1a));

    // 绘制菜单边框
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
    // 检查是否点击在按钮区域之外（空白区域）
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

        // 限制在父组件范围内
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
    // 计算菜单所需的总宽度
    int totalWidth = menuPadding;
    for (auto* button : menuButtons)
    {
        totalWidth += button->getRequiredWidth() + buttonSpacing;
    }
    if (menuButtons.size() > 0)
        totalWidth -= buttonSpacing; // 移除最后一个按钮后的间距
    totalWidth += menuPadding;

    // 如果菜单还没有位置，初始化到左上角
    if (getBounds().isEmpty())
    {
        setBounds(10, 10, totalWidth, menuHeight);
    }
    else
    {
        // 只更新宽度，保持当前位置
        setSize(totalWidth, menuHeight);
    }

    resized();
}

void MenuTable::FloatingMenu::setSelectedButton(int index)
{
    // 取消所有按钮的选中状态
    for (auto* button : menuButtons)
    {
        button->setSelected(false);
    }

    // 设置指定按钮为选中状态
    if (index >= 0 && index < menuButtons.size())
    {
        menuButtons[index]->setSelected(true);
    }
}