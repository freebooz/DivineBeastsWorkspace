#pragma once

#include "Screens/DivineBeastsLoadingScreen.h"
#include "DivineBeastsBootScreen.generated.h"

class UDivineBeastsBootViewModel;

/**
 * UDivineBeastsBootScreen（神兽联盟启动封面页面 C++ 基类）。
 *
 * 职责：
 * - 作为 WBP_DBA_UI_Boot（神兽联盟启动封面蓝图）的唯一项目父类。
 * - 复用平台 Loading Screen（加载页面）的不可返回约束，避免初始化事务尚未结束时被用户关闭。
 * - 只消费 UDivineBeastsBootViewModel（启动视图模型）的事件驱动状态，不自行轮询资源或流程。
 * - 公司 LOGO、游戏 LOGO、背景图等真实美术资源由后续 DBAUIPack_Core（核心UI内容包）拥有，
 *   本 C++ 类不硬引用二进制资产。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsBootScreen
    : public UDivineBeastsLoadingScreen
{
    GENERATED_BODY()

public:
    /** 返回稳定页面标识，与项目 UI Catalog（界面目录）保持一致。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Boot")
    FName GetBootScreenId() const
    {
        return TEXT("UI.Screen.Boot");
    }

    /** 返回类型安全的启动 ViewModel；类型不匹配时返回 nullptr。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Boot")
    UDivineBeastsBootViewModel* GetBootViewModel() const;
};
