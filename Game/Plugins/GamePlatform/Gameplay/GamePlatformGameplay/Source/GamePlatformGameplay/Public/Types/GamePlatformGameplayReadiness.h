#pragma once

#include "CoreMinimal.h"
#include "GamePlatformGameplayReadiness.generated.h"

/**
 * 当前拥有者的准备确认令牌。它只关联当前复制状态，不是认证票据或反作弊证明；
 * 服务器收到报告后仍重新检查实际连接、准入、Pawn拥有关系及全部代次。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformPreparationToken
{
    GENERATED_BODY()

    /** 每次Pawn确认阶段随机签发的不可复用身份，只通过拥有者通道复制。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGuid TokenId;

    /** 当前世界运行身份。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGuid WorldContextGeneration;

    /** 当前服务器体验代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 ExperienceEpoch = 0;

    /** 当前世界内玩家登记代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 PlayerGeneration = 0;

    /** 当前受控Pawn代次；授权重新生成后必须变化。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 PawnGeneration = 0;

    /** 签发时玩家公开状态修订；旧状态报告不能推进新状态。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 StateRevision = 0;

    /** 仅检查值形状；是否属于当前拥有者和当前记录必须由服务器登记表判定。 */
    bool IsValid() const
    {
        return TokenId.IsValid() && WorldContextGeneration.IsValid() && ExperienceEpoch > 0
            && PlayerGeneration > 0 && PawnGeneration > 0 && StateRevision > 0;
    }

    bool operator==(const FGamePlatformPreparationToken& Other) const
    {
        return TokenId == Other.TokenId && WorldContextGeneration == Other.WorldContextGeneration
            && ExperienceEpoch == Other.ExperienceEpoch && PlayerGeneration == Other.PlayerGeneration
            && PawnGeneration == Other.PawnGeneration && StateRevision == Other.StateRevision;
    }
};

/**
 * 客户端开放前准备事实；全部只是本机当前状态报告，不赋予服务器权限。
 * GameplayInputEnabled故意不参与IsComplete，避免输入开放与服务器Active形成循环。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformLocalPreparationFacts
{
    GENERATED_BODY()

    /** 当前体验代次的本机Data租约可读。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bClientExperiencePrepared = false;

    /** 当前拥有者已识别正确Pawn及Pawn代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bClientPawnBound = false;

    /** 可选Input适配已准备配置；共享插件不依赖InputClient。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bInputProfilePrepared = false;

    /** 可选Input适配已绑定当前接收器；重生后必须重新建立。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bBindingsReady = false;

    /** 仅供诊断；输入是否开放是报告完成后的下游结果，不参与准备提交。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bGameplayInputEnabled = false;

    /** 开放前四项事实是否完整；不检查服务器权威资格。 */
    bool IsComplete() const
    {
        return bClientExperiencePrepared && bClientPawnBound && bInputProfilePrepared && bBindingsReady;
    }
};
