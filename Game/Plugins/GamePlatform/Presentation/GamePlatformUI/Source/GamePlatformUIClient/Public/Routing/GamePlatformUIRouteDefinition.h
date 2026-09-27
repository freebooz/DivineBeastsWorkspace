#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIRouteDefinition.generated.h"

/** UI Route 只描述导航元数据，不编码项目业务流程。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIRouteDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FName RouteId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FName ScreenId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    EGamePlatformUILayer Layer = EGamePlatformUILayer::Screen;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    EGamePlatformUITransition Transition = EGamePlatformUITransition::Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FGameplayTagContainer RequiredTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FGameplayTagContainer BlockedTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FName FallbackRouteId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Route")
    FName BackRouteId = NAME_None;

    /** Route自身结构校验；目标Screen存在性由UI Manager验证。 */
    bool ValidateDefinition(FText& OutReason) const;
};
