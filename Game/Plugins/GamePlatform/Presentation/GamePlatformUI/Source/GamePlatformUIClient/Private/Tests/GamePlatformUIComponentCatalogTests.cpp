#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformResourceBarWidget.h"
#include "Components/GamePlatformPortraitWidget.h"
#include "Components/GamePlatformSlotWidget.h"
#include "Components/GamePlatformCountdownWidget.h"
#include "Components/GamePlatformMinimapWidget.h"
#include "Components/GamePlatformPartyRosterWidget.h"
#include "Components/GamePlatformSlotBarWidget.h"
#include "Components/GamePlatformStatusEffectTrayWidget.h"
#include "Components/GamePlatformQuestTrackerWidget.h"
#include "Components/GamePlatformInteractionPromptWidget.h"
#include "Components/GamePlatformSettingRowWidget.h"
#include "Components/GamePlatformTooltipWidget.h"
#include "Components/GamePlatformCastProgressWidget.h"
#include "Components/GamePlatformTargetFrameWidget.h"
#include "Components/GamePlatformCombatAlertWidget.h"
#include "Components/GamePlatformComponentWidget.h"

/**
 * 平台公共UI组件目录/反射边界审查：
 * 检查游戏跨项目常用Widget确实继承中立基础类，并保留Blueprint可扩展契约。
 * 测试不创建游戏Actor、模拟服务器状态、装配外部项目蓝图或访问磁盘凭据。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIComponentHierarchyTest,
    "GamePlatform.UI.Components.Hierarchy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIComponentHierarchyTest::RunTest(const FString&)
{
    UClass* BaseClass = UGamePlatformComponentWidget::StaticClass();

    const TArray<UClass*> Components = {
        UGamePlatformResourceBarWidget::StaticClass(), // 资源条
        UGamePlatformPortraitWidget::StaticClass(), // 头像
        UGamePlatformSlotWidget::StaticClass(), // 技能/物品槽位
        UGamePlatformSlotBarWidget::StaticClass(), // 快捷/技能栏
        UGamePlatformMinimapWidget::StaticClass(), // 小地图
        UGamePlatformPartyRosterWidget::StaticClass(), // 队伍成员
        UGamePlatformCountdownWidget::StaticClass(), // 倒计时
        UGamePlatformStatusEffectTrayWidget::StaticClass(), // 状态效果
        UGamePlatformQuestTrackerWidget::StaticClass(), // 任务追踪
        UGamePlatformInteractionPromptWidget::StaticClass(), // 交互提示
        UGamePlatformSettingRowWidget::StaticClass(), // 设置项
        UGamePlatformTooltipWidget::StaticClass(), // 悬浮提示
        UGamePlatformCastProgressWidget::StaticClass(), // 通用施法/引导进度条
        UGamePlatformTargetFrameWidget::StaticClass(), // 目标/焦点/首领状态框
        UGamePlatformCombatAlertWidget::StaticClass() // 关键战斗机制警告
    };

    TSet<UClass*> UniqueClasses;
    for (UClass* ComponentClass : Components)
    {
        TestNotNull(TEXT("UI组件类必须具有真实反射身份"), ComponentClass);
        if (!ComponentClass)
        {
            continue;
        }
        TestTrue(TEXT("UI组件必须继承通用组件基类"),
            ComponentClass->IsChildOf(BaseClass));
        TestTrue(TEXT("UI组件必须保持抽象类型，供项目蓝图派生"),
            ComponentClass->HasAnyClassFlags(CLASS_Abstract));
        TestFalse(TEXT("UI组件不能重复登记"), UniqueClasses.Contains(ComponentClass));
        UniqueClasses.Add(ComponentClass);
    }

    TestEqual(TEXT("独立公共UI组件总数"), Components.Num(), 15);
    FGamePlatformUIResourceBarState HealthState;
    HealthState.MaximumValue = 100.0;
    HealthState.CurrentValue = 150.0;
    TestTrue(TEXT("资源条归一化不能超过1"),
        FMath::IsNearlyEqual(HealthState.GetNormalizedValue(), 1.0));
    HealthState.MaximumValue = 0.0;
    TestTrue(TEXT("最大值无效时资源进度归零"),
        FMath::IsNearlyZero(HealthState.GetNormalizedValue()));
    return true;
}

#endif
