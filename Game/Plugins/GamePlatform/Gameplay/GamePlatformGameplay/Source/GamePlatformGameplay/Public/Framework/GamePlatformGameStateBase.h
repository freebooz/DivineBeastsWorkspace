#pragma once
#include "GameFramework/GameStateBase.h"
#include "GamePlatformGameStateBase.generated.h"
class UGamePlatformExperienceComponent;

/** 服务器体验快照的唯一全员复制载体；不拥有账号、票据或第二套体验状态机。 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API AGamePlatformGameStateBase : public AGameStateBase
{
    GENERATED_BODY()
public:
    AGamePlatformGameStateBase();
    /** 当前GameState创建并持有的组件；游戏线程使用，世界结束后禁止缓存。 */
    UGamePlatformExperienceComponent* GetExperienceComponent() const { return Experience; }
private:
    UPROPERTY(VisibleAnywhere, Category="GamePlatform|Gameplay")
    TObjectPtr<UGamePlatformExperienceComponent> Experience;
};
