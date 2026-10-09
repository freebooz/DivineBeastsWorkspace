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
 * 所有实现必须异步完成并在游戏线程回调一次，禁止在Dedicated Server游戏线程执行阻塞HTTP/数据库访问。
 * Begin返回false不得执行回调；接受后必须明确成功、失败或结果未知。端口不拥有Subsystem/World。
 * BeginPersist/Complete以(PlayerId, QuestInstanceId, EventIds)事务性幂等：None代表本批全部提交；
 * DuplicateEvent仅在本批全部EventIds已提交时返回同实例的权威快照，不能将部分重复伪装成整批成功。
 * CompletionAlreadyCommitted必须返回相同CompletionId的Completed权威快照；这两种结果不会盲重放事件。
 * RevisionConflict保证本批无事件写入，可重新加载后以原EventId重放；部分已处理、落库回包丢失等无法证明的结果
 * 必须返回PersistenceOutcomeUnknown并保留原幂等身份重试，禁止改成RevisionConflict导致本地再次累加。
 * 后端尚无生产Quest端点；此合同是实现门禁，不是生产持久化验收。
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
