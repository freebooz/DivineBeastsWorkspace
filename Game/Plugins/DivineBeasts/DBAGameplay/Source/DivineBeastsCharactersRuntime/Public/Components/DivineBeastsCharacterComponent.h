#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
// 可信Owner与配置快照使用Actor完整合同，公开头直接声明该依赖；Ready实现已外置，不依赖宿主PCH或旧内联描述。
#include "GameFramework/Actor.h"
#include "Initialization/GamePlatformCharacterInitializer.h"
#include "State/GamePlatformCharacterStateView.h"
#include "Identity/DivineBeastsZodiacIdentity.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsCharacterComponent.generated.h"

class UDivineBeastsHeroDefinition;
class UGamePlatformAbilitySystemComponent;
struct FGamePlatformAbilityAvatarBindingSnapshot;

/** FDivineBeastsCharacterReadinessChangedNative（项目角色就绪变化）。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsCharacterReadinessChangedNative,
    bool);

/**
 * FDivineBeastsCharacterRuntimeState（神兽联盟角色原子运行状态）。
 * Hero身份、生肖、出生/Avatar代次和服务器认可的Definition版本作为一个复制单元发布，
 * 防止客户端分别收到多个字段时把不同代次的数据临时拼成错误角色状态。
 */
USTRUCT()
struct DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsCharacterRuntimeState
{
    GENERATED_BODY()

    /** Shared契约定义的十二生肖稳定英雄编号。 */
    UPROPERTY()
    FName HeroDefinitionId = NAME_None;

    /** 项目本地生肖枚举；必须能由HeroDefinitionId确定性推导。 */
    UPROPERTY()
    EDivineBeastsZodiacIdentity ZodiacIdentity = EDivineBeastsZodiacIdentity::Rat;

    /** 当前Pawn出生代次。 */
    UPROPERTY()
    int32 SpawnGeneration = 0;

    /** 当前Avatar绑定代次。 */
    UPROPERTY()
    int32 AvatarGeneration = 0;

    /** 服务器实际加载并认可的Definition结构版本；0表示尚未确认。 */
    UPROPERTY()
    int32 DefinitionVersion = 0;

    /** 服务器实际加载并认可的Definition内容修订号；客户端必须一致后才可Ready。 */
    UPROPERTY()
    FString ContentRevision;
};

/**
 * UDivineBeastsCharacterComponent（神兽联盟项目角色组件）。
 * 负责可信运行身份、Definition lease、移动/碰撞配置、Generation和Readiness；
 * 不拥有ASC、技能、伤害、AI Brain、装备或表现资源。
 */
UCLASS(ClassGroup=(DivineBeasts), meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSCHARACTERSRUNTIME_API UDivineBeastsCharacterComponent final
    : public UActorComponent
    , public IGamePlatformCharacterStateView
{
    GENERATED_BODY()

public:
    UDivineBeastsCharacterComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 仅服务器可信Spawn/Admission路径调用；客户端无RPC自报入口。 */
    bool AuthorityBindTrustedContext(
        const FGamePlatformCharacterInitializationContext& Context,
        FString& OutError);

    /** 游戏线程可信出生适配器借用同世界已Succeeded预热租约；自身租约须已受理。借用者不释放它，所有者须保持到自身完成。 */
    bool TryUsePreloadedDefinition(const FGamePlatformDataLease& WarmupLease, UObject& WarmupOwner, FString& OutError);

    /** 任意身份/Definition/Generation变化后可重复调用，幂等重评估。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Character")
    void RefreshInitialization();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    FName GetHeroDefinitionId() const { return RuntimeState.HeroDefinitionId; }

    /** 仅Owner得到持久CharacterId；远端观察者默认不复制。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    FString GetCharacterId() const { return CharacterId; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    EDivineBeastsZodiacIdentity GetZodiacIdentity() const
    {
        return RuntimeState.ZodiacIdentity;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    int32 GetSpawnGeneration() const { return RuntimeState.SpawnGeneration; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    int32 GetAvatarGeneration() const { return RuntimeState.AvatarGeneration; }

    /** 返回服务器认可的Definition结构版本；0表示尚未完成版本确认。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    int32 GetDefinitionVersion() const { return RuntimeState.DefinitionVersion; }

    /** 返回服务器认可的Definition内容修订号；客户端Definition必须与其一致。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    FString GetDefinitionContentRevision() const { return RuntimeState.ContentRevision; }

    /** GT只读已成功可信绑定的内部操作身份；首次绑定前为空，每次接纳（含相同字段/代次）都会改变。
     * 调用者只能在AuthorityBind成功返回后立即捕获，用于后续同步回调后的自有资源复核；
     * 它不是准入票据、权限或客户端命令，不能凭此ID批准角色/比赛。失败返回无法取得原调用身份。 */
    FGuid GetTrustedContextOperationId() const { return TrustedContextOperationId; }

    // IGamePlatformCharacterStateView（平台角色状态只读接口）
    virtual FName GetCharacterStateHeroDefinitionId() const override { return GetHeroDefinitionId(); }
    virtual int32 GetCharacterStateSpawnGeneration() const override { return GetSpawnGeneration(); }
    virtual int32 GetCharacterStateAvatarGeneration() const override { return GetAvatarGeneration(); }
    virtual int32 GetCharacterStateDefinitionVersion() const override { return GetDefinitionVersion(); }
    virtual FString GetCharacterStateContentRevision() const override { return GetDefinitionContentRevision(); }
    virtual bool IsCharacterStateReady() const override { return IsCharacterReady(); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    bool IsCharacterReady() const;

    const UDivineBeastsHeroDefinition* GetLoadedDefinition() const
    {
        return LoadedDefinition;
    }
    /** 真实Data加载最近结果；接纳不等于Ready，失败明确保留结果供组合根显示/诊断，不包含敏感身份。 */
    FGamePlatformResult GetLastDefinitionLoadResult() const { return LastDefinitionLoadResult; }

    FDivineBeastsCharacterReadinessChangedNative& OnReadinessChanged()
    {
        return ReadinessChanged;
    }

private:
    /** 配置栈借用的不可变身份快照；引擎碰撞/GAS及Data同步通知返回后，旧栈只能核查，不能清后继状态。 */
    struct FInitializationSnapshot
    {
        TWeakObjectPtr<AActor> Owner;
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<UDivineBeastsHeroDefinition> Definition;
        TWeakObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;
        TWeakObjectPtr<AActor> AbilityAvatar;
        int32 AbilityAvatarGeneration = 0;
        FGuid ContextOperationId;
        int32 RequestGeneration = 0;
        int32 SpawnGeneration = 0;
        int32 AvatarGeneration = 0;
        FName HeroDefinitionId;
        int32 DefinitionVersion = 0;
        FString ContentRevision;
        bool bAuthority = false;
    };
    FInitializationSnapshot CaptureInitializationSnapshot() const;
    /** 可选配置操作ID区分同身份的递归Refresh；不向外部服务公布或复制。 */
    bool IsInitializationCurrent(const FInitializationSnapshot& Snapshot, FGuid InitializationId = FGuid()) const;
    /** 任一Hero身份、代次或Definition修订变化都重新建立本地Definition租约，避免旧异步请求污染新角色。 */
    UFUNCTION()
    void OnRep_RuntimeState();

    UFUNCTION()
    void OnRep_ServerReady();

    /** 只读Data当前状态；自身需求成功或同World活所有者的真实预热需求成功才能使用Definition。 */
    bool HasReadableDefinitionResources() const;
    TWeakObjectPtr<UObject> BorrowedWarmupOwner;
    FGamePlatformDataLease BorrowedWarmupLease;
    void BeginDefinitionLoad();
    void CancelDefinitionLease();
    void HandleDefinitionLoaded(
        UDivineBeastsHeroDefinition* Definition,
        int32 ExpectedRequestGeneration,
        int32 ExpectedSpawnGeneration,
        int32 ExpectedAvatarGeneration);

    bool ApplyDefinition(
        const UDivineBeastsHeroDefinition& Definition,
        const FInitializationSnapshot& Snapshot,
        FGuid InitializationId,
        FString& OutError);

    bool IsIdentityStructurallyValid() const;
    void UpdateReadiness();
    void BroadcastReadinessIfChanged(bool bPreviousReady);
    /** 当前ASC存在则绑定Avatar事件，ActorInfo就绪后注入项目只读Gate；不代替宿主绑定ActorInfo。 */
    void RefreshActivationGateBinding();
    void HandleAbilityAvatarBindingChanged(const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot);
    /** 组件结束时撤销本组件Gate并移除原生委托；ASC可继续存活，不留下旧项目Owner。 */
    void DetachActivationGate();

    /** Owner-only持久身份，避免向所有观察者复制PlayerData档案ID。 */
    UPROPERTY(Replicated, Transient)
    FString CharacterId;

    /** 面向所有相关观察者的原子角色运行状态；不包含持久CharacterId等隐私字段。 */
    UPROPERTY(ReplicatedUsing=OnRep_RuntimeState, Transient)
    FDivineBeastsCharacterRuntimeState RuntimeState;

    UPROPERTY(Replicated, Transient)
    bool bPersistentCharacterIdRequired = true;

    UPROPERTY(ReplicatedUsing=OnRep_ServerReady, Transient)
    bool bServerReady = false;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsHeroDefinition> LoadedDefinition = nullptr;

    bool bLocalReady = false;
    bool bConfigurationApplied = false;
    int32 DefinitionRequestGeneration = 0;
    /** 每次接纳可信身份绑定签发操作身份；同步Ready监听者的真实后继绑定或退出会使旧栈失败关闭。 */
    FGuid TrustedContextOperationId;
    /** 每次Refresh签发，递归配置会使原栈失效；不能只靠最外层AuthorityBind末端检查。 */
    FGuid InitializationOperationId;
    /** 成功后持续持有至结束/身份变化；不能在完成回调中提前释放。 */
    FGamePlatformDataLease DefinitionLease;
    FGamePlatformResult LastDefinitionLoadResult;
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> BoundAbilitySystem;
    FDelegateHandle AvatarBindingChangedHandle;
    bool bEndingPlay = false;

    FDivineBeastsCharacterReadinessChangedNative ReadinessChanged;
};
