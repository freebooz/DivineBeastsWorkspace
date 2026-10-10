// 神兽联盟项目面板只配置语义键，不拥有纹理/StyleClass；主题资产唯一归属DBAUIPack_Core。
#include "Panels/DivineBeastsPanelWidget.h"
#include "Styling/DivineBeastsUIStyleBindings.h"

UDivineBeastsPanelWidget::UDivineBeastsPanelWidget()
{
    // 标准面板约定：已有Widget使用同名控件就自动采用统一主题；旧面板没有这些控件则不修改外观。
    // 三项均为可选，避免其他已有复杂HUD/任务面板缺少命名控件时被统一样式阻断。
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("PanelBackground"), DivineBeasts::UI::Styling::Keys::BorderPanel, EGamePlatformUIStyleKind::Border, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("PanelTitle"), DivineBeasts::UI::Styling::Keys::TextTitle, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("CloseButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button, true));
}
