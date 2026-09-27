#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformQuestTypes.h"

using FGamePlatformQuestLoadCompletion =
    TFunction<void(
        TArray<FGamePlatformQuestSnapshot>,
        EGamePlatformQuestError)>;

using FGamePlatformQuestMutationCompletion =
    TFunction<void(
        FGamePlatformQuestSnapshot,
        EGamePlatformQuestError)>;

/**
 * 任务持久化端口。
 * 所有实现必须异步完成，禁止在Dedicated Server（专用服务器）游戏线程执行阻塞HTTP/数据库访问。
 */
class GAMEPLATFORMQUESTSERVER_API IGamePlatformQuestPersistencePort
{
public:
    virtual ~IGamePlatformQuestPersistencePort() = default;

    /** 返回true仅表示请求已成功启动，不代表持久化已成功。 */
    virtual bool BeginLoadPlayerQuestSnapshot(
        const FString& PlayerId,
        FGamePlatformQuestLoadCompletion Completion) = 0;

    virtual bool BeginAcceptQuest(
        const FString& PlayerId,
        const FGamePlatformQuestSnapshot& ProposedSnapshot,
        EGamePlatformQuestRepeatPolicy RepeatPolicy,
        FGamePlatformQuestMutationCompletion Completion) = 0;

    virtual bool BeginPersistQuestEvents(
        const FString& PlayerId,
        const FGamePlatformQuestSnapshot& ProposedSnapshot,
        int64 ExpectedRevision,
        const TArray<FGuid>& EventIds,
        FGamePlatformQuestMutationCompletion Completion) = 0;

    virtual bool BeginCompleteQuest(
        const FString& PlayerId,
        const FString& CharacterId,
        const FGamePlatformQuestSnapshot& ProposedSnapshot,
        int64 ExpectedRevision,
        const TArray<FGuid>& EventIds,
        const FGuid& CompletionId,
        FGamePlatformQuestMutationCompletion Completion) = 0;

    virtual bool BeginAbandonQuest(
        const FString& PlayerId,
        const FGamePlatformQuestSnapshot& ProposedSnapshot,
        int64 ExpectedRevision,
        FGamePlatformQuestMutationCompletion Completion) = 0;
};
