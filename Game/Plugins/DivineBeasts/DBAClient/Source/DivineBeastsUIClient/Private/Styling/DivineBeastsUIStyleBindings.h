// 神兽联盟界面语义样式键：仅声明用途，统一视觉资源由第三层DBAUIPack_Core主题Definition决定。
#pragma once

#include "Styling/GamePlatformUIThemeTypes.h"

namespace DivineBeasts::UI::Styling
{
    /** 五个业务共用语义键；新增项目按钮必须优先重用而不是在每个页面生成独立纹理ID。 */
    namespace Keys
    {
        inline constexpr TCHAR ButtonPrimary[] = TEXT("UI.Style.Button.Primary");
        inline constexpr TCHAR ButtonSecondary[] = TEXT("UI.Style.Button.Secondary");
        inline constexpr TCHAR TextBody[] = TEXT("UI.Style.Text.Body");
        inline constexpr TCHAR TextTitle[] = TEXT("UI.Style.Text.Title");
        inline constexpr TCHAR BorderPanel[] = TEXT("UI.Style.Border.Panel");
    }

    /** 生成一个项目控件→平台中立样式语义绑定；不在构造时加载资源或修改UObject默认样式。 */
    inline FGamePlatformUIWidgetStyleBinding MakeBinding(
        const TCHAR* WidgetName, const TCHAR* StyleId,
        EGamePlatformUIStyleKind Kind, bool bOptional = false)
    {
        FGamePlatformUIWidgetStyleBinding Result;
        Result.WidgetName = FName(WidgetName);
        Result.StyleId = FName(StyleId);
        Result.Kind = Kind;
        Result.bOptional = bOptional;
        Result.bAllowParentFallback = false;
        Result.bPreserveFontSize = true; // 保留已验收的原有字号和宽高。
        return Result;
    }
}
