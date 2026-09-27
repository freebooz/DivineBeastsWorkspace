#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIScreenDefinition.generated.h"

class UGamePlatformUIScreen;

/** 页面注册与异步打开的唯一数据定义。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIScreenDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FName ScreenId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TSoftClassPtr<UGamePlatformUIScreen> WidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    EGamePlatformUILayer Layer = EGamePlatformUILayer::Screen;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    EGamePlatformUIInputMode InputMode = EGamePlatformUIInputMode::GameAndUI;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    EGamePlatformUIPausePolicy PausePolicy = EGamePlatformUIPausePolicy::Never;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    EGamePlatformUITransition Transition = EGamePlatformUITransition::Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TArray<TSoftObjectPtr<UObject>> PreloadAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FGameplayTagContainer RequiredTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FGameplayTagContainer BlockedTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    FName DefaultFocusWidgetName = NAME_None;

    /** 显式声明为LocalPlayer全局页面时才允许跨LoadMap保留。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    bool bSurvivesTravel = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
    TMap<FName, TSoftClassPtr<UGamePlatformUIScreen>> PlatformWidgetVariants;

    /** 注册前的跨游戏安全/结构校验；不验证业务权限。 */
    bool ValidateDefinition(FText& OutReason) const;
};
