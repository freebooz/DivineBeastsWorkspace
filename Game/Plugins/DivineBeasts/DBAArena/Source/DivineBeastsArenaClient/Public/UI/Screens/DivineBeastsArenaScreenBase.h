#pragma once

#include "UI/GamePlatformMobaArenaScreenBase.h"
#include "DivineBeastsArenaScreenBase.generated.h"

/**
 * UDivineBeastsArenaScreenBase（神兽联盟竞技页面基类）。
 *
 * 只增加项目层稳定 SurfaceId（界面表面身份）；
 * 竞技状态、页面生命周期和输入策略继续复用 MobaCommon / GamePlatform。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaScreenBase
    : public UGamePlatformMobaArenaScreenBase
{
    GENERATED_BODY()
    friend class FDivineBeastsArenaModeSelectionRegressionTest;

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    FName GetExpectedSurfaceId() const { return ExpectedSurfaceId; }

protected:
    /** 页面激活生命周期内接线命名按钮/列表；只提交原生适配命令，不切图、不产生匹配权威状态。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision,int32 PageGeneration) override;
    /** 通过模式目录解析容量，禁止从枚举整数或字符串数字推算。 */
    void ModeSelected(UObject* Item);
    UFUNCTION() void StartMatchmaking();
    UFUNCTION() void CancelMatchmaking();
    UFUNCTION() void ReturnWorld();
    UFUNCTION() void CloseArenaPage();
    FName SelectedMode=NAME_None;
    FText CommandFeedback;
    /** 模式列表刷新不能把程序性取消选择当作用户修改。 */
    bool bRebuildingModes=false;
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Arena|UI")
    FName ExpectedSurfaceId = NAME_None;
};
