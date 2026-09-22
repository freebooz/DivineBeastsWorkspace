#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformExperienceDefinition.generated.h"

/** 第一版重新生成策略；不包含死亡、战斗、货币或比赛复活规则。 */
UENUM(BlueprintType)
enum class EGamePlatformRespawnPolicy : uint8
{
    /** 不提供重新生成；断开或失败由上层会话处理。 */
    Disabled,
    /** 仅服务器可信代码可显式请求一次完整重新生成流程。 */
    ServerAuthorized
};

/** 体验装配项声明；FactoryId由服务器批准的类型化工厂解析，不接受反射类名或脚本路径。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformExperienceAssemblyEntry
{
    GENERATED_BODY()

    /** 体验内唯一装配身份；非None且不得重复。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName AssemblyId;

    /** 组合根预先注册的批准工厂键；未知键使必需项启动失败。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName FactoryId;

    /** 必须先成功激活的装配项身份；定义校验拒绝未知项、重复项、自依赖和环。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FName> Dependencies;

    /** true表示失败使整个体验回滚；false表示记录诊断后跳过该项。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bRequired = true;
};

/**
 * 平台中立体验定义。LogicalId即ExperienceId；DataVersion、RequiredDefinitions和稳定资产身份复用Data。
 * 不直接引用输入配置、UI、Niagara、项目英雄、技能、装备或后端端点。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMGAMEPLAY_API UGamePlatformExperienceDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /** 允许承载本体验的世界逻辑身份，至少一项；由服务器当前World快照匹配。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FGamePlatformId> SupportedWorldIds;

    /** 中立用途键，仅用于配置和诊断；项目不得依赖显示文本判断权限。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName Purpose;

    /**
     * 双端共享定义；每项必须同时位于RequiredDefinitions，随体验根租约递归持有。
     * 不放纯客户端表现配置，也不包含服务器私有定义。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FPrimaryAssetId> SharedDefinitions;

    /**
     * 仅服务器另行申请的定义；不得位于RequiredDefinitions，防止客户端根租约递归拉入。
     * 本数组仍只允许GamePlatformDefinition身份，缺失会使服务器体验启动失败。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FPrimaryAssetId> ServerDefinitions;

    /** 默认中立Pawn定义；必须是合法GamePlatformDefinition并位于RequiredDefinitions。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FPrimaryAssetId DefaultPawnDefinitionId;

    /** 服务器出生策略注册键；默认PlayerStart由插件内建，项目可在启动前注册批准策略。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName SpawnPolicyId = TEXT("PlayerStart");

    /** 经校验的有向无环装配项；按依赖顺序准备和激活，排空时逆序撤销。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FGamePlatformExperienceAssemblyEntry> AssemblyEntries;

    /** 玩家等待体验就绪的截止秒数；有限正数且不得超过全局安全上限。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(ClampMin="0.001", Units="s"))
    float PlayerExperienceTimeoutSeconds = 30.f;

    /** 出生候选、区域或碰撞条件等待截止秒数；有限正数且不得超过全局安全上限。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(ClampMin="0.001", Units="s"))
    float SpawnTimeoutSeconds = 30.f;

    /** Pawn被控制后等待拥有者准备确认的截止秒数；有限正数且不得超过全局安全上限。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(ClampMin="0.001", Units="s"))
    float ClientPreparationTimeoutSeconds = 60.f;

    /** 本体验等待队列上限；1..全局安全上限，慢玩家不冻结世界或其他玩家。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(ClampMin="1"))
    int32 MaximumWaitingPlayers = 64;

    /** 第一版仅Disabled或ServerAuthorized，不建立任意客户端复活入口。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    EGamePlatformRespawnPolicy RespawnPolicy = EGamePlatformRespawnPolicy::Disabled;

    /** true仅允许显式非Shipping开发装配选择；不是生产缺配置时的回退。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    bool bDevelopmentOnly = false;

    /** 游戏线程纯字段校验；不加载资产、不访问网络、不修改值。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
