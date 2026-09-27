#pragma once

#include "Screens/DivineBeastsLoadingScreen.h"
#include "DivineBeastsLoadingTravelScreen.generated.h"

class UDivineBeastsLoadingViewModel;

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
};
