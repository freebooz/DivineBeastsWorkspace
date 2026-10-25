#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInventoryTypes.h"
#include "Components/GamePlatformSlotWidget.h"

class UUserWidget;

/** 背包显示采用平台通用槽位值；物品名称与具体颜色由项目展示适配器填充。 */
using FDivineBeastsInventoryCellDisplay = FGamePlatformUISlotCellPresentation;

/**
 * FDivineBeastsInventoryCellPresenter（背包格展示适配器）。
 * 将背包格/快捷栏共用的纯展示规则与真实UMG控件映射从页面业务类移出。
 * 不订阅世界、不轮询Tick、不创建资源加载器或发送任何权威写入命令。
 */
class FDivineBeastsInventoryCellPresenter final
{
public:
    /** 纯展示投影：Slot为空表示该页未返回有效槽位，而非服务端物品。 */
    static FDivineBeastsInventoryCellDisplay MakeInventoryCell(
        const FGamePlatformInventorySlotViewModel* Slot,
        bool bIconReady, bool bReady, const FString& SelectedItemInstanceId);

    /** 纯展示投影：快捷栏只显示当前背包实例的引用，不复制业务状态。 */
    static FDivineBeastsInventoryCellDisplay MakeQuickbarCell(
        const FGamePlatformInventoryQuickbarViewModel* Quick,
        int32 Index, bool bIconReady, bool bReady, int32 SelectedQuickbarIndex);

    /** 使用现有Monolith命名控件落地显示，保持原有按钮委托、槽位几何和工具提示规则。 */
    static void ApplyCell(
        UUserWidget& Screen, int32 Index, bool bQuickbar, bool bIconReady,
        const FDivineBeastsInventoryCellDisplay& Cell);
};
