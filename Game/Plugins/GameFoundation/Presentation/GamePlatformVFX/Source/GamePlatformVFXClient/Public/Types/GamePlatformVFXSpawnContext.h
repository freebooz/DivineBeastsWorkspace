#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXSpawnContext.generated.h"

class AActor;
class USceneComponent;

/** 单次 VFX 播放的世界/附着上下文；不写回 Definition 静态资产。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXSpawnContext
{
    GENERATED_BODY()

    /** 未附着时使用的世界变换。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FTransform WorldTransform = FTransform::Identity;

    /** 可选附着组件。异步加载期间使用弱引用，目标失效后请求会失败。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<USceneComponent> AttachComponent;

    /** 附着 Socket / Bone 名称。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName AttachSocket = NAME_None;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> SourceActor;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> TargetActor;
};
