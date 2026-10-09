// GamePlatformProgression公开服务器权威适配合同：上层调用方遵循下述线程/参数/终态；后端拥有长期数据权威，资产由数据/内容服务拥有。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformProgressionTypes.h"

using FGamePlatformProgressionRequirementCompletion =
    TFunction<void(
        FGamePlatformProgressionRequirementResult,
        EGamePlatformProgressionError)>;

/**
 * Dedicated Server（专用服务器）使用的中立成长资格提供器。
 * 实现可来自PlayerData查询/缓存；共享模块不依赖HTTP。
 */
class GAMEPLATFORMPROGRESSION_API IGamePlatformProgressionSnapshotProvider
{
public:
    virtual ~IGamePlatformProgressionSnapshotProvider() = default;

    /** 仅服务器游戏线程：非空已授权PlayerId，Requirement必须匹配主体/轨道/正等级，调用方先验证角色归属。
     * true只表示查询受理，Completion须游戏线程恰一次交付来源版本/结果/领域错误；false不回调。
     * 缓存过期/网络失败必须明确错误，不能返回固定满足；取消等待不授予XP或回滚后端事务。 */
    virtual bool BeginCheckRequirement(
        const FString& PlayerId,
        const FGamePlatformProgressionRequirement& Requirement,
        FGamePlatformProgressionRequirementCompletion Completion) = 0;
};
