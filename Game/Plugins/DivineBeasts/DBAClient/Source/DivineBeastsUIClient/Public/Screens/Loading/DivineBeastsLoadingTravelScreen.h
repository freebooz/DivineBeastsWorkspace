#pragma once

#include "Screens/DivineBeastsLoadingScreen.h"
#include "DivineBeastsLoadingTravelScreen.generated.h"

class UDivineBeastsLoadingViewModel;
class UTextBlock;

/**
 * UDivineBeastsLoadingTravelScreen（神兽联盟加载/切服页面 C++ 基类）。
 *
 * 适用于 SessionAdmission、ExpectedWorld、ExpectedExperience、CharacterBinding、
 * GameplayData、ProjectReadiness 等真实加载事务。
 * 页面只消费 UDivineBeastsLoadingViewModel 的真实快照，不自行生成假进度。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoadingTravelScreen
    : public UDivineBeastsLoadingScreen
{
    GENERATED_BODY()

public:
    /** 返回与现有项目 Catalog 对齐的稳定页面标识。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Loading")
    FName GetLoadingScreenId() const
    {
        return TEXT("UI.Screen.LoadingTravel");
    }

    /** 返回类型安全的加载 ViewModel。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Loading")
    UDivineBeastsLoadingViewModel* GetLoadingViewModel() const;

protected:
    /** 平台完成激活/VM更换的寿命与代次校验后，读取当前快照；游戏线程，不另建委托。 */
    virtual void RefreshInitialState() override;
    /** 平台唯一订阅完成页面代次过滤后通知；Revision为状态序号，PageGeneration为当前VM页面代次。 */
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;

private:
    /** 仅激活页且当前VM仍拥有PageGeneration时展示真实阶段文本；过期/失活直接忽略。
     * Revision为平台状态事件序号，显示读取当前快照，不以序号制造进度；无Tick/新订阅。
     * 保留原UFUNCTION签名和StageText字段身份，已有Widget Blueprint无需迁移字段/父类。
     */
    UFUNCTION()
    void RefreshLoadingPresentation(int32 Revision, int32 PageGeneration);
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> StageText = nullptr;
};
