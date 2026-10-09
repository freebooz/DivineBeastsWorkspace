#include "GamePlatformDebugPrivate.h"

#include "Registry/GamePlatformDebugRegistry.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/ActorComponent.h"
#include "State/GamePlatformCharacterStateView.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NavigationSystem.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Types/GamePlatformAITypes.h"
#include "Types/GamePlatformTelemetryTypes.h"

namespace
{
    using FCollectFunction = TFunction<bool(
        const FGamePlatformDebugCollectContext&,
        FGamePlatformDebugSnapshot&)>;

    class FGamePlatformLambdaDebugProvider final
        : public IGamePlatformDebugStateProvider
    {
    public:
        FGamePlatformLambdaDebugProvider(
            FName InProviderId,
            EGamePlatformDebugProviderCost InCost,
            bool bInRequiresTarget,
            FCollectFunction InCollect)
            : ProviderId(InProviderId)
            , Cost(InCost)
            , bRequiresTarget(bInRequiresTarget)
            , CollectFunction(MoveTemp(InCollect))
        {
        }

        virtual FName GetProviderId() const override
        {
            return ProviderId;
        }

        virtual bool CanCollect(
            const FGamePlatformDebugCollectContext& Context) const override
        {
            if (!Context.World.IsValid())
            {
                return false;
            }

            return !bRequiresTarget || Context.Target.IsValid();
        }

        virtual bool CollectSnapshot(
            const FGamePlatformDebugCollectContext& Context,
            FGamePlatformDebugSnapshot& OutSnapshot) const override
        {
            return CollectFunction
                ? CollectFunction(Context, OutSnapshot)
                : false;
        }

        virtual EGamePlatformDebugProviderCost GetEstimatedCost() const override
        {
            return Cost;
        }

    private:
        FName ProviderId;
        EGamePlatformDebugProviderCost Cost;
        bool bRequiresTarget = false;
        FCollectFunction CollectFunction;
    };

    TArray<FName> RegisteredProviderIds;

    FString BoolText(bool bValue)
    {
        return bValue ? TEXT("true") : TEXT("false");
    }

    FString NetModeText(ENetMode NetMode)
    {
        switch (NetMode)
        {
        case NM_Standalone:
            return TEXT("Standalone");
        case NM_DedicatedServer:
            return TEXT("DedicatedServer");
        case NM_ListenServer:
            return TEXT("ListenServer");
        case NM_Client:
            return TEXT("Client");
        default:
            return TEXT("Unknown");
        }
    }

    FString GuidSummary(const FGuid& Guid)
    {
        if (!Guid.IsValid())
        {
            return TEXT("None");
        }

        const FString Full = Guid.ToString(EGuidFormats::Digits);
        return Full.Left(12);
    }

    void AddNA(
        FGamePlatformDebugSnapshot& Snapshot,
        FName Key,
        const TCHAR* DisplayName,
        const TCHAR* Reason)
    {
        Snapshot.AddField(
            Key,
            DisplayName,
            FString::Printf(TEXT("N/A (%s)"), Reason),
            EGamePlatformDebugValueType::String,
            EGamePlatformDebugSeverity::Info);
    }

    bool CollectStatus(
        const FGamePlatformDebugCollectContext&,
        FGamePlatformDebugSnapshot& Snapshot)
    {
#if UE_BUILD_TEST
        const TCHAR* BuildName = TEXT("Test");
#elif UE_BUILD_DEVELOPMENT
        const TCHAR* BuildName = TEXT("Development");
#elif UE_BUILD_DEBUG
        const TCHAR* BuildName = TEXT("Debug");
#else
        const TCHAR* BuildName = TEXT("NonShipping");
#endif

        const FGamePlatformDebugRegistry& Registry =
            FGamePlatformDebugRegistry::Get();

        Snapshot.AddField(
            TEXT("Build"),
            TEXT("构建配置"),
            BuildName);
        Snapshot.AddField(
            TEXT("ProviderCount"),
            TEXT("状态提供者数量"),
            FString::FromInt(Registry.GetProviderIds().Num()),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("CommandCount"),
            TEXT("调试命令数量"),
            FString::FromInt(Registry.GetCommandDescriptors().Num()),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("RemoteDebug"),
            TEXT("远程调试"),
            TEXT("Disabled by default; no arbitrary remote query RPC in v1"));
        Snapshot.AddField(
            TEXT("ShippingPolicy"),
            TEXT("正式发布策略"),
            TEXT("Module allow-list + UE_BUILD_SHIPPING gate"));
        return true;
    }

    bool CollectWorld(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        UWorld* World = Context.World.Get();
        if (!World)
        {
            return false;
        }

        Snapshot.AddField(
            TEXT("WorldId"),
            TEXT("世界编号"),
            FString::Printf(TEXT("%u"), World->GetUniqueID()),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("Map"),
            TEXT("地图"),
            World->GetMapName());
        Snapshot.AddField(
            TEXT("NetMode"),
            TEXT("网络模式"),
            NetModeText(World->GetNetMode()));
        Snapshot.AddField(
            TEXT("ServerRole"),
            TEXT("服务器角色"),
            NetModeText(World->GetNetMode()));
        Snapshot.AddField(
            TEXT("WorldGeneration"),
            TEXT("世界代次"),
            FString::FromInt(static_cast<int32>(World->GetUniqueID())),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("WorldReady"),
            TEXT("世界就绪"),
            BoolText(World->HasBegunPlay()),
            EGamePlatformDebugValueType::Boolean);
        Snapshot.AddField(
            TEXT("StreamingLevels"),
            TEXT("流送关卡数量"),
            FString::FromInt(World->GetStreamingLevels().Num()),
            EGamePlatformDebugValueType::Integer);

        const UNavigationSystemV1* NavSystem =
            FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        Snapshot.AddField(
            TEXT("NavigationReady"),
            TEXT("导航就绪"),
            BoolText(NavSystem != nullptr),
            EGamePlatformDebugValueType::Boolean);

        AddNA(Snapshot, TEXT("ExperienceId"), TEXT("体验编号"), TEXT("GamePlatformWorld尚未公开稳定接口"));
        AddNA(Snapshot, TEXT("Region"), TEXT("区域"), TEXT("GamePlatformWorld尚未公开稳定接口"));
        AddNA(Snapshot, TEXT("DataLayers"), TEXT("数据层摘要"), TEXT("避免全量扫描World Partition"));
        return true;
    }

    bool CollectCharacter(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AActor* Actor = Context.Target.Get();
        if (!Actor)
        {
            return false;
        }

        Snapshot.AddField(TEXT("Actor"), TEXT("对象"), Actor->GetName());
        Snapshot.AddField(TEXT("Class"), TEXT("类型"), Actor->GetClass()->GetName());
        Snapshot.AddField(
            TEXT("Authority"),
            TEXT("权威端"),
            BoolText(Actor->HasAuthority()),
            EGamePlatformDebugValueType::Boolean);
        Snapshot.AddField(
            TEXT("Location"),
            TEXT("位置"),
            Actor->GetActorLocation().ToCompactString(),
            EGamePlatformDebugValueType::Vector);
        Snapshot.AddField(
            TEXT("Velocity"),
            TEXT("速度"),
            Actor->GetVelocity().ToCompactString(),
            EGamePlatformDebugValueType::Vector);
        Snapshot.AddField(
            TEXT("LifecycleState"),
            TEXT("生命周期"),
            Actor->HasActorBegunPlay() ? TEXT("BegunPlay") : TEXT("NotBegunPlay"));

        if (const ACharacter* Character = Cast<ACharacter>(Actor))
        {
            const UCharacterMovementComponent* Movement =
                Character->GetCharacterMovement();
            Snapshot.AddField(
                TEXT("MovementMode"),
                TEXT("移动模式"),
                Movement ? Movement->GetMovementName() : TEXT("N/A"));
        }
        else
        {
            AddNA(Snapshot, TEXT("MovementMode"), TEXT("移动模式"), TEXT("目标不是Character"));
        }

        // 持久CharacterId属于隐私/玩家数据边界，平台只读接口刻意不公开。
        AddNA(Snapshot, TEXT("CharacterId"), TEXT("角色编号"), TEXT("平台角色状态接口不公开持久档案ID"));

        IGamePlatformCharacterStateView* CharacterStateView = nullptr;
        TInlineComponentArray<UActorComponent*> ActorComponents(Actor);
        for (UActorComponent* Component : ActorComponents)
        {
            if (IGamePlatformCharacterStateView* Candidate =
                    Cast<IGamePlatformCharacterStateView>(Component))
            {
                CharacterStateView = Candidate;
                break;
            }
        }

        if (CharacterStateView)
        {
            Snapshot.AddField(
                TEXT("HeroDefinitionId"),
                TEXT("英雄定义编号"),
                CharacterStateView->GetCharacterStateHeroDefinitionId().ToString());
            Snapshot.AddField(
                TEXT("HeroDefinitionVersion"),
                TEXT("英雄定义版本"),
                FString::FromInt(CharacterStateView->GetCharacterStateDefinitionVersion()),
                EGamePlatformDebugValueType::Number);
            Snapshot.AddField(
                TEXT("HeroContentRevision"),
                TEXT("英雄内容修订"),
                CharacterStateView->GetCharacterStateContentRevision());
            Snapshot.AddField(
                TEXT("CharacterReady"),
                TEXT("角色就绪"),
                BoolText(CharacterStateView->IsCharacterStateReady()),
                EGamePlatformDebugValueType::Boolean);
        }
        else
        {
            AddNA(Snapshot, TEXT("HeroDefinitionId"), TEXT("英雄定义编号"), TEXT("目标未提供平台角色状态接口"));
            AddNA(Snapshot, TEXT("HeroDefinitionVersion"), TEXT("英雄定义版本"), TEXT("目标未提供平台角色状态接口"));
            AddNA(Snapshot, TEXT("HeroContentRevision"), TEXT("英雄内容修订"), TEXT("目标未提供平台角色状态接口"));
            AddNA(Snapshot, TEXT("CharacterReady"), TEXT("角色就绪"), TEXT("目标未提供平台角色状态接口"));
        }
        AddNA(Snapshot, TEXT("Equipment"), TEXT("装备摘要"), TEXT("不跨越PlayerServices权限边界"));
        AddNA(Snapshot, TEXT("ProgressionLevel"), TEXT("成长等级"), TEXT("不跨越PlayerServices权限边界"));

        if (Context.SourceView == EGamePlatformDebugSourceView::Client)
        {
            AddNA(
                Snapshot,
                TEXT("ClientServerTransformDelta"),
                TEXT("客户端/服务器Transform差"),
                TEXT("服务器视角由Gameplay Debugger权威采集"));
        }
        return true;
    }

    bool CollectAbility(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AActor* Actor = Context.Target.Get();
        if (!Actor)
        {
            return false;
        }

        UAbilitySystemComponent* ASC =
            UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
                Actor,
                true);
        if (!ASC)
        {
            AddNA(Snapshot, TEXT("ASC"), TEXT("技能系统组件"), TEXT("目标未提供ASC"));
            return true;
        }

        Snapshot.AddField(TEXT("Owner"), TEXT("ASC拥有者"), GetNameSafe(ASC->GetOwnerActor()));
        Snapshot.AddField(TEXT("Avatar"), TEXT("ASC化身"), GetNameSafe(ASC->GetAvatarActor()));
        Snapshot.AddField(
            TEXT("ActivatableAbilities"),
            TEXT("可激活技能数量"),
            FString::FromInt(ASC->GetActivatableAbilities().Num()),
            EGamePlatformDebugValueType::Integer);

        FGameplayTagContainer OwnedTags;
        ASC->GetOwnedGameplayTags(OwnedTags);
        FString TagText = OwnedTags.ToStringSimple(false);
        if (!Context.Filter.IsEmpty() && !TagText.Contains(Context.Filter))
        {
            TagText = TEXT("(filtered)");
        }
        Snapshot.AddField(TEXT("GameplayTags"), TEXT("玩法标签"), TagText);

        TArray<FGameplayAttribute> Attributes;
        ASC->GetAllAttributes(Attributes);
        Snapshot.AddField(
            TEXT("Attributes"),
            TEXT("属性数量"),
            FString::FromInt(Attributes.Num()),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("ActiveGameplayEffects"),
            TEXT("活动GameplayEffect数量"),
            FString::FromInt(ASC->GetActiveEffects(FGameplayEffectQuery()).Num()),
            EGamePlatformDebugValueType::Integer);
        AddNA(Snapshot, TEXT("Cooldowns"), TEXT("冷却摘要"), TEXT("首版不每帧展开全部Effect"));
        return true;
    }

    bool CollectCombat(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AActor* Actor = Context.Target.Get();
        UGamePlatformCombatComponent* Combat =
            Actor ? Actor->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
        if (!Combat)
        {
            AddNA(Snapshot, TEXT("Combat"), TEXT("战斗组件"), TEXT("目标未挂载GamePlatformCombatComponent"));
            return true;
        }

        Snapshot.AddField(TEXT("Health"), TEXT("生命值"), FString::SanitizeFloat(Combat->GetCombatHealth()), EGamePlatformDebugValueType::Number);
        Snapshot.AddField(TEXT("MaxHealth"), TEXT("最大生命值"), FString::SanitizeFloat(Combat->GetCombatMaxHealth()), EGamePlatformDebugValueType::Number);
        Snapshot.AddField(TEXT("Dead"), TEXT("死亡状态"), BoolText(Combat->IsCombatDead()), EGamePlatformDebugValueType::Boolean);
        Snapshot.AddField(TEXT("AvatarGeneration"), TEXT("化身代次"), FString::FromInt(Combat->GetCombatAvatarGeneration()), EGamePlatformDebugValueType::Integer);
        AddNA(Snapshot, TEXT("ControlTags"), TEXT("控制标签"), TEXT("无只读公开摘要接口"));
        AddNA(Snapshot, TEXT("LastCombatEvent"), TEXT("最近战斗事件"), TEXT("无只读公开历史接口"));
        AddNA(Snapshot, TEXT("LastDamageHeal"), TEXT("最近伤害/治疗"), TEXT("无只读公开历史接口"));
        return true;
    }

    bool CollectAI(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AActor* Actor = Context.Target.Get();
        UGamePlatformAIStateComponent* State =
            Actor ? Actor->FindComponentByClass<UGamePlatformAIStateComponent>() : nullptr;
        if (!State)
        {
            AddNA(Snapshot, TEXT("AIState"), TEXT("AI状态"), TEXT("目标未挂载GamePlatformAIStateComponent"));
            return true;
        }

        const FGamePlatformAIStateSnapshot& AI = State->GetSnapshot();
        Snapshot.AddField(TEXT("AIEntityId"), TEXT("AI实体编号"), GuidSummary(AI.AIEntityId));
        Snapshot.AddField(TEXT("AIDefinitionId"), TEXT("AI定义编号"), AI.AIDefinitionId.ToString());
        Snapshot.AddField(TEXT("PublicState"), TEXT("公开状态"), UEnum::GetValueAsString(AI.PublicState));
        Snapshot.AddField(TEXT("Target"), TEXT("目标"), GuidSummary(AI.CurrentTargetEntityId));
        Snapshot.AddField(TEXT("MovementIntent"), TEXT("移动意图"), AI.MovementIntentTag.ToString());
        Snapshot.AddField(TEXT("CombatIntent"), TEXT("战斗意图"), AI.CombatIntentTag.ToString());
        Snapshot.AddField(TEXT("StateRevision"), TEXT("状态修订"), FString::FromInt(AI.StateRevision), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("AIInstanceGeneration"), TEXT("AI实例代次"), FString::FromInt(AI.AIInstanceGeneration), EGamePlatformDebugValueType::Integer);
        AddNA(Snapshot, TEXT("UpdateTier"), TEXT("更新层级"), TEXT("共享AI接口未公开"));
        AddNA(Snapshot, TEXT("Home"), TEXT("归属地状态"), TEXT("共享AI接口未公开"));
        AddNA(Snapshot, TEXT("Blackboard"), TEXT("黑板白名单"), TEXT("完整Blackboard仅服务器原生GameplayDebugger查看"));
        return true;
    }

    bool CollectNavigation(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        UWorld* World = Context.World.Get();
        const UNavigationSystemV1* NavSystem =
            World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
        Snapshot.AddField(
            TEXT("NavigationReady"),
            TEXT("导航系统"),
            NavSystem ? TEXT("Ready") : TEXT("Unavailable"));

        if (AActor* Target = Context.Target.Get())
        {
            Snapshot.AddField(TEXT("TargetLocation"), TEXT("目标位置"), Target->GetActorLocation().ToCompactString(), EGamePlatformDebugValueType::Vector);
        }

        AddNA(Snapshot, TEXT("NavigationProfile"), TEXT("导航配置"), TEXT("共享层未公开当前请求Profile"));
        AddNA(Snapshot, TEXT("LastRequestId"), TEXT("最近请求编号"), TEXT("服务器导航服务未向Runtime共享历史"));
        AddNA(Snapshot, TEXT("PathStatus"), TEXT("路径状态"), TEXT("避免读取ServerOnly内部对象"));
        AddNA(Snapshot, TEXT("CostLength"), TEXT("路径成本/长度"), TEXT("避免高频重算路径"));
        AddNA(Snapshot, TEXT("Filter"), TEXT("查询过滤器"), TEXT("无当前请求公开接口"));
        AddNA(Snapshot, TEXT("Invoker"), TEXT("导航Invoker"), TEXT("不做全量Actor扫描"));
        AddNA(Snapshot, TEXT("SmartLink"), TEXT("智能链接"), TEXT("无当前请求公开接口"));
        return true;
    }

    bool CollectNetwork(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        UWorld* World = Context.World.Get();
        if (!World)
        {
            return false;
        }

        Snapshot.AddField(TEXT("NetMode"), TEXT("网络模式"), NetModeText(World->GetNetMode()));

        APlayerController* PC = nullptr;
        if (const APawn* Pawn = Cast<APawn>(Context.Target.Get()))
        {
            PC = Cast<APlayerController>(Pawn->GetController());
        }
        if (!PC)
        {
            PC = World->GetFirstPlayerController();
        }

        Snapshot.AddField(
            TEXT("Connection"),
            TEXT("连接"),
            PC && PC->GetNetConnection() ? TEXT("Connected") : TEXT("N/A"));

        if (const APlayerState* PS = PC ? PC->PlayerState : nullptr)
        {
            Snapshot.AddField(
                TEXT("RTT"),
                TEXT("往返时延"),
                FString::Printf(TEXT("%.2f ms"), PS->GetPingInMilliseconds()),
                EGamePlatformDebugValueType::Number);
        }
        else
        {
            AddNA(Snapshot, TEXT("RTT"), TEXT("往返时延"), TEXT("无PlayerState"));
        }

        AddNA(Snapshot, TEXT("PacketLoss"), TEXT("丢包"), TEXT("使用Networking Insights/net.*查看"));
        AddNA(Snapshot, TEXT("Bytes"), TEXT("网络字节"), TEXT("使用Networking Insights查看"));
        AddNA(Snapshot, TEXT("RPCRate"), TEXT("RPC速率"), TEXT("使用Network Trace查看"));
        AddNA(Snapshot, TEXT("Correction"), TEXT("位置修正"), TEXT("当前共享接口不可可靠读取"));
        AddNA(Snapshot, TEXT("Replication"), TEXT("复制摘要"), TEXT("使用Networking Insights查看"));
        return true;
    }

    bool CollectOnline(
        const FGamePlatformDebugCollectContext&,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AddNA(Snapshot, TEXT("LoginState"), TEXT("登录状态"), TEXT("GamePlatformOnline当前未公开调试只读适配器"));
        AddNA(Snapshot, TEXT("PlayerId"), TEXT("玩家编号"), TEXT("避免未经脱敏读取身份"));
        AddNA(Snapshot, TEXT("Environment"), TEXT("环境"), TEXT("GamePlatformOnline当前未公开调试只读适配器"));
        AddNA(Snapshot, TEXT("LastBackendError"), TEXT("最近后端错误"), TEXT("GamePlatformOnline当前未公开调试只读适配器"));
        AddNA(Snapshot, TEXT("ServiceHealth"), TEXT("服务健康摘要"), TEXT("复用现有Health时再接入"));
        return true;
    }

    bool CollectSession(
        const FGamePlatformDebugCollectContext&,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AddNA(Snapshot, TEXT("SessionId"), TEXT("会话编号摘要"), TEXT("GamePlatformSession当前仍为骨架"));
        AddNA(Snapshot, TEXT("MatchId"), TEXT("比赛编号"), TEXT("GamePlatformSession当前仍为骨架"));
        AddNA(Snapshot, TEXT("Assignment"), TEXT("分配"), TEXT("GamePlatformSession当前仍为骨架"));
        AddNA(Snapshot, TEXT("Destination"), TEXT("目标服务器"), TEXT("GamePlatformSession当前仍为骨架"));
        AddNA(Snapshot, TEXT("TransferState"), TEXT("迁移状态"), TEXT("GamePlatformSession当前仍为骨架"));
        AddNA(Snapshot, TEXT("TicketId"), TEXT("迁移票据编号摘要"), TEXT("不读取完整TransferTicket"));
        AddNA(Snapshot, TEXT("ExpiresAt"), TEXT("迁移票据过期时间"), TEXT("无只读公开接口"));
        AddNA(Snapshot, TEXT("Consumed"), TEXT("迁移票据消费状态"), TEXT("无只读公开接口"));
        return true;
    }

    bool CollectLoading(
        const FGamePlatformDebugCollectContext&,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        AddNA(Snapshot, TEXT("LoadingState"), TEXT("加载状态"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("BlockingReason"), TEXT("阻塞原因"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("SuppressTokens"), TEXT("抑制令牌数量"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("AssetLeases"), TEXT("资产租约"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("PendingDefinitions"), TEXT("待加载定义"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("ClientReady"), TEXT("客户端就绪"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("ServerActiveGate"), TEXT("服务器激活门禁"), TEXT("GamePlatformLoading当前仍为骨架"));
        AddNA(Snapshot, TEXT("TimeoutReason"), TEXT("超时原因"), TEXT("GamePlatformLoading当前仍为骨架"));
        return true;
    }

    bool CollectTelemetry(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& Snapshot)
    {
        UWorld* World = Context.World.Get();
        UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
        UGamePlatformTelemetrySubsystem* Telemetry =
            GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr;

        if (!Telemetry)
        {
            AddNA(Snapshot, TEXT("Telemetry"), TEXT("遥测"), TEXT("GameInstance未提供TelemetrySubsystem"));
            return true;
        }

        const FGamePlatformTelemetryDiagnostics Diagnostics =
            Telemetry->GetDiagnostics();
        Snapshot.AddField(TEXT("BufferDepth"), TEXT("缓冲深度"), FString::FromInt(Diagnostics.BufferDepth), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("BufferBytes"), TEXT("缓冲字节"), FString::Printf(TEXT("%lld"), Diagnostics.BufferBytes), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("RecordedTotal"), TEXT("记录总数"), FString::Printf(TEXT("%lld"), Diagnostics.RecordedTotal), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("DroppedCounts"),
            TEXT("丢弃总数"),
            FString::Printf(
                TEXT("%lld"),
                Diagnostics.DroppedVerbose +
                    Diagnostics.DroppedNormal +
                    Diagnostics.DroppedCritical),
            EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("SampledOut"), TEXT("采样丢弃"), FString::Printf(TEXT("%lld"), Diagnostics.SampledOutTotal), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("RateLimited"), TEXT("限流总数"), FString::Printf(TEXT("%lld"), Diagnostics.RateLimitedTotal), EGamePlatformDebugValueType::Integer);

        Snapshot.AddField(TEXT("Enabled"), TEXT("启用状态"), BoolText(Diagnostics.bEnabled), EGamePlatformDebugValueType::Boolean);
        Snapshot.AddField(TEXT("SinkHealth"), TEXT("输出器健康"), Diagnostics.SinkHealth.ToString());
        Snapshot.AddField(TEXT("PendingNetworkBatches"), TEXT("待发送网络批次"), FString::FromInt(Diagnostics.PendingNetworkBatches), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("SubmittedBatches"), TEXT("已提交批次"), FString::Printf(TEXT("%lld"), Diagnostics.SubmittedBatches), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("FailedBatches"), TEXT("失败批次"), FString::Printf(TEXT("%lld"), Diagnostics.FailedBatches), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(TEXT("LastFlushRecords"), TEXT("最近刷新记录数"), FString::FromInt(Diagnostics.LastFlushRecords), EGamePlatformDebugValueType::Integer);
        Snapshot.AddField(
            TEXT("LastFlush"),
            TEXT("最近刷新"),
            Diagnostics.LastFlushUtc.GetTicks() > 0 ? Diagnostics.LastFlushUtc.ToIso8601() : TEXT("N/A"));
        Snapshot.AddField(
            TEXT("SinkLastSuccess"),
            TEXT("输出器最近成功"),
            Diagnostics.SinkLastSuccessUtc.GetTicks() > 0 ? Diagnostics.SinkLastSuccessUtc.ToIso8601() : TEXT("N/A"));
        Snapshot.AddField(
            TEXT("SinkLastFailure"),
            TEXT("输出器最近失败"),
            Diagnostics.SinkLastFailureUtc.GetTicks() > 0 ? Diagnostics.SinkLastFailureUtc.ToIso8601() : TEXT("N/A"));
        Snapshot.AddField(
            TEXT("SinkLastError"),
            TEXT("输出器最近错误"),
            Diagnostics.SinkLastError.IsEmpty() ? TEXT("None") : Diagnostics.SinkLastError);
        AddNA(Snapshot, TEXT("Sampling"), TEXT("采样策略"), TEXT("Telemetry当前按Schema定义，不公开运行期全量策略枚举"));
        return true;
    }

    void AddProvider(
        FName Id,
        EGamePlatformDebugProviderCost Cost,
        bool bRequiresTarget,
        FCollectFunction Collect)
    {
        TSharedRef<IGamePlatformDebugStateProvider> Provider =
            MakeShared<FGamePlatformLambdaDebugProvider>(
                Id,
                Cost,
                bRequiresTarget,
                MoveTemp(Collect));

        if (FGamePlatformDebugRegistry::Get().RegisterStateProvider(Provider))
        {
            RegisteredProviderIds.Add(Id);
        }
        else
        {
            UE_LOG(
                LogGamePlatformDebug,
                Warning,
                TEXT("State Provider already registered: %s"),
                *Id.ToString());
        }
    }
}

void GamePlatformDebugPrivate::RegisterBuiltInProviders()
{
    RegisteredProviderIds.Reset();
    AddProvider(TEXT("Status"), EGamePlatformDebugProviderCost::Cheap, false, &CollectStatus);
    AddProvider(TEXT("World"), EGamePlatformDebugProviderCost::Cheap, false, &CollectWorld);
    AddProvider(TEXT("Character"), EGamePlatformDebugProviderCost::Cheap, true, &CollectCharacter);
    AddProvider(TEXT("Ability"), EGamePlatformDebugProviderCost::Moderate, true, &CollectAbility);
    AddProvider(TEXT("Combat"), EGamePlatformDebugProviderCost::Cheap, true, &CollectCombat);
    AddProvider(TEXT("AI"), EGamePlatformDebugProviderCost::Cheap, true, &CollectAI);
    AddProvider(TEXT("Navigation"), EGamePlatformDebugProviderCost::Moderate, false, &CollectNavigation);
    AddProvider(TEXT("Online"), EGamePlatformDebugProviderCost::Cheap, false, &CollectOnline);
    AddProvider(TEXT("Session"), EGamePlatformDebugProviderCost::Cheap, false, &CollectSession);
    AddProvider(TEXT("Loading"), EGamePlatformDebugProviderCost::Cheap, false, &CollectLoading);
    AddProvider(TEXT("Telemetry"), EGamePlatformDebugProviderCost::Cheap, false, &CollectTelemetry);
    AddProvider(TEXT("Network"), EGamePlatformDebugProviderCost::Cheap, false, &CollectNetwork);
}

void GamePlatformDebugPrivate::UnregisterBuiltInProviders()
{
    FGamePlatformDebugRegistry& Registry = FGamePlatformDebugRegistry::Get();
    for (const FName Id : RegisteredProviderIds)
    {
        Registry.UnregisterStateProvider(Id);
    }
    RegisteredProviderIds.Reset();
}
