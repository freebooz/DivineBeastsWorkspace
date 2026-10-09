// 仅自动化夹具，禁止作为生产交互目标配置；模拟Custom提交中替换选项导致同步取消，没有外部服务副作用。
#pragma once
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformInteractable.h"
#include "Types/GamePlatformInteractionEvent.h"
#include "InteractionReentrantCommitFixture.generated.h"
class UGamePlatformInteractableComponent;
UCLASS(Transient, NotBlueprintable)
class AInteractionReentrantCommitFixture : public AActor, public IGamePlatformInteractable
{
    GENERATED_BODY()
public:
    /** 自动化监听者拥有一次性回调，并保存广播值验证旧终态没有混入新会话。 */
    TFunction<void()> ResultHandler;
    TArray<FGamePlatformInteractionEvent> Events;
    TArray<FGamePlatformInteractionSession> Sessions;
    UFUNCTION() void HandleResult(const FGamePlatformInteractionResult& Result) { (void)Result; auto Once = MoveTemp(ResultHandler); if (Once) { Once(); } }
    UFUNCTION() void HandleEvent(const FGamePlatformInteractionEvent& Event) { Events.Add(Event); }
    UFUNCTION() void HandleSession(const FGamePlatformInteractionSession& Session) { Sessions.Add(Session); }
    /** 实际副作用是清空选项并取消目标会话；返回原始处理器结果供上层验证不应再完成。 */
    virtual bool CommitInteraction(const FGamePlatformInteractionSession& Session,
        const FGamePlatformInteractionOption& Option) override;
};
