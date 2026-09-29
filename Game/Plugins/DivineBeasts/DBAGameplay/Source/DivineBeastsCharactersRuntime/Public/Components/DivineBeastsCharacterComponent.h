#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Initialization/GamePlatformCharacterInitializer.h"
#include "State/GamePlatformCharacterStateView.h"
#include "Identity/DivineBeastsZodiacIdentity.h"
#include "DivineBeastsCharacterComponent.generated.h"

struct FStreamableHandle;
class UDivineBeastsHeroDefinition;

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

    // IGamePlatformCharacterStateView（平台角色状态只读接口）
    virtual FName GetCharacterStateHeroDefinitionId() const override { return GetHeroDefinitionId(); }
    virtual int32 GetCharacterStateSpawnGeneration() const override { return GetSpawnGeneration(); }
    virtual int32 GetCharacterStateAvatarGeneration() const override { return GetAvatarGeneration(); }
    virtual int32 GetCharacterStateDefinitionVersion() const override { return GetDefinitionVersion(); }
    virtual FString GetCharacterStateContentRevision() const override { return GetDefinitionContentRevision(); }
    virtual bool IsCharacterStateReady() const override { return IsCharacterReady(); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    bool IsCharacterReady() const
    {
        return GetOwner() && GetOwner()->HasAuthority()
            ? bServerReady
            : bServerReady && bLocalReady;
    }

    const UDivineBeastsHeroDefinition* GetLoadedDefinition() const
    {
        return LoadedDefinition;
    }

    FDivineBeastsCharacterReadinessChangedNative& OnReadinessChanged()
    {
        return ReadinessChanged;
    }

private:
    /** 任一Hero身份、代次或Definition修订变化都重新建立本地Definition租约，避免旧异步请求污染新角色。 */
    UFUNCTION()
    void OnRep_RuntimeState();

    UFUNCTION()
    void OnRep_ServerReady();

    void BeginDefinitionLoad();
    void CancelDefinitionLease();
    void HandleDefinitionLoaded(
        UDivineBeastsHeroDefinition* Definition,
        int32 ExpectedRequestGeneration,
        int32 ExpectedSpawnGeneration,
        int32 ExpectedAvatarGeneration);

    bool ApplyDefinition(
        const UDivineBeastsHeroDefinition& Definition,
        FString& OutError);

    bool IsIdentityStructurallyValid() const;
    void UpdateReadiness();
    void BroadcastReadinessIfChanged(bool bPreviousReady);

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
    TSharedPtr<FStreamableHandle> DefinitionLease;

    FDivineBeastsCharacterReadinessChangedNative ReadinessChanged;
};
