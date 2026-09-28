#pragma once

#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "DivineBeastsBootViewModel.generated.h"

/**
 * UDivineBeastsBootViewModel（神兽联盟启动封面视图模型）。
 *
 * 启动页属于真实前台初始化过程，因此直接复用 UDivineBeastsLoadingViewModel：
 * - 有平台 Loading Token（加载令牌）时显示真实阶段与真实进度；
 * - 没有可量化进度时保持 Progress < 0，界面只能显示不确定进度表现；
 * - 同时继承项目 ViewState（视图状态），用于观察 Boot / Initialize 等流程阶段。
 *
 * 本类不读取磁盘、不创建 Tick、不保存公司或游戏 LOGO 的硬资产引用。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsBootViewModel
    : public UDivineBeastsLoadingViewModel
{
    GENERATED_BODY()

public:
    /** 当前是否已经进入 Authentication（认证）阶段；进入后主路由应切换到 Login 页面。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Boot")
    bool IsAuthenticationStageReached() const
    {
        return GetStateRef().CurrentStep == TEXT("DBA.Flow.Authentication");
    }

    /** 只有平台真实快照提供非负值时才允许显示百分比进度。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Boot")
    bool HasAuthoritativeProgress() const
    {
        return GetLoadingSnapshot().Progress >= 0.0f;
    }
};
