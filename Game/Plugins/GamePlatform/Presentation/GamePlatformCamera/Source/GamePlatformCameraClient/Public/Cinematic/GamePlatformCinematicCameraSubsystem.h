// 客户端本地玩家电影机位控制：只借用外部机位，不生成Actor，不接管游戏输入与网络权威。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformCinematicCameraSubsystem.generated.h"

class AActor;
class APlayerController;
class UGamePlatformCinematicCameraProfile;

/** 临时展示镜头的本地生命周期由此子系统管理；失去Controller或切图时安全失效。 */
UCLASS()
class GAMEPLATFORMCAMERACLIENT_API UGamePlatformCinematicCameraSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

    /** 进入已存在的电影机位。目标必须属于本地玩家当前世界；失败时不改变视角。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Camera|Cinematic")
    bool BeginCinematicView(AActor* CameraActor, const UGamePlatformCinematicCameraProfile* Profile);

    /** 仅当本子系统仍拥有当前ViewTarget时才恢复，避免覆盖更高优先级的死亡/传送/观战机位。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Camera|Cinematic")
    bool EndCinematicView();

    /** 查询本地子系统当前是否仍拥有电影机位。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Camera|Cinematic")
    bool IsCinematicViewActive() const;

private:
    void ClearOwnedView();
    TWeakObjectPtr<AActor> PreviousViewTarget;
    TWeakObjectPtr<AActor> OwnedViewTarget;
    TWeakObjectPtr<APlayerController> OwnedController;
    TWeakObjectPtr<UWorld> OwnedWorld;
    float RestoreBlendTime = 0.0f;
};
