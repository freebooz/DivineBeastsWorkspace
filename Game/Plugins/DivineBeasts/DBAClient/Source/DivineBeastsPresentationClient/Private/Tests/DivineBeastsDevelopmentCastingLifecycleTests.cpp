// 项目客户端施法生命周期真实回归：调用原Cast组件、WorldSubsystem、AnimInstance和平台Submit。
// RED阶段只新增本测试及friend声明，生产.cpp不改；缺资源/夹具错误不算缺陷RED。
// 普通前缀9项；CrashRegression前缀2项单独运行，旧实现可能在TMap失效/FindChecked处致命退出。
// 主端确认项目身份为同代权威Avatar接收合同；身份事件只重评估当前事实，不伪造Combat复制事件。
// 夹具使用隔离无物理World及其引擎上下文，不启动PIE、后端、Data预热或真实Niagara，不保存任何资产。
// 提供者仅是请求边界替身：验证完整请求合同并记录自有实例，不能作为资源、GPU或联机通过证据。
#include "CoreMinimal.h" // EditorOnly宏由锁定引擎公共配置定义，不能在公共头加载前求值。
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA
#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsDevelopmentCastingWorldSubsystem.h"
#include "Tests/DivineBeastsDevelopmentCastingReentryObserver.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

/** 仅测试拥有的完整生命周期夹具；friend读取账本不是业务接口，期待值从外部行为独立给定。 */
struct FDivineBeastsDevelopmentCastingTestFixture
{
    FAutomationTestBase& Test;
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform;
    TStrongObjectPtr<UDivineBeastsDevelopmentCastingWorldSubsystem> Service;
    TStrongObjectPtr<UDivineBeastsDevelopmentCastingReentryObserver> Observer;
    TStrongObjectPtr<USkeletalMesh> MeshAsset;
    TStrongObjectPtr<UAnimMontage> FireMontage, FrostMontage;
    ACharacter* Character = nullptr;
    UGamePlatformAbilityCastComponent* Cast = nullptr;
    UGamePlatformCombatComponent* Combat = nullptr;
    UDivineBeastsCharacterComponent* Identity = nullptr;
    FDelegateHandle RequestObserver;
    FGuid Provider;
    TSet<FGuid> LiveRequests;
    TFunction<void()> CancellationAction;
    int32 Attempts = 0, Accepted = 0, CancelNotifications = 0;
    bool bProviderReady = true, bDeinitialized = false;

    explicit FDivineBeastsDevelopmentCastingTestFixture(FAutomationTestBase& InTest) : Test(InTest) {}
    ~FDivineBeastsDevelopmentCastingTestFixture()
    {
        CancellationAction = nullptr;
        if (Observer.IsValid()) Observer->StartedAction = nullptr;
        if (Character && IsValid(Character) && Character->GetMesh()->GetAnimInstance() && Observer.IsValid())
            Character->GetMesh()->GetAnimInstance()->OnMontageStarted.RemoveDynamic(
                Observer.Get(), &UDivineBeastsDevelopmentCastingReentryObserver::HandleMontageStarted);
        if (Service.IsValid() && !bDeinitialized) Deinitialize();
        if (Platform.IsValid())
        {
            Platform->OnRequestObserved().Remove(RequestObserver);
            Platform->UnregisterProvider(Provider);
        }
        if (World.IsValid())
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World.Get());
            World->DestroyWorld(false);
        }
    }

    /** 真实保存资源只读加载；任何前置缺失立即失败，不能把资源不存在误算生命周期RED。 */
    bool Initialize(bool bInstallAnimation = true)
    {
        if (!Test.TestNotNull(TEXT("夹具前置：真实Engine"), GEngine)) return false;
        MeshAsset.Reset(LoadObject<USkeletalMesh>(nullptr,
            TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
        FireMontage.Reset(LoadObject<UAnimMontage>(nullptr,
            TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AM_DBA_DevFireballCast.AM_DBA_DevFireballCast")));
        FrostMontage.Reset(LoadObject<UAnimMontage>(nullptr,
            TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AM_DBA_DevFrostboltCast.AM_DBA_DevFrostboltCast")));
        if (!Test.TestNotNull(TEXT("夹具前置：已保存真实Mesh"), MeshAsset.Get()) ||
            !Test.TestNotNull(TEXT("夹具前置：已保存Fire Montage"), FireMontage.Get()) ||
            !Test.TestNotNull(TEXT("夹具前置：已保存Frost Montage"), FrostMontage.Get())) return false;
        if (!Test.TestTrue(TEXT("夹具前置：骨架一致且两个Montage均有效"),
            MeshAsset->GetSkeleton() == FireMontage->GetSkeleton() &&
            MeshAsset->GetSkeleton() == FrostMontage->GetSkeleton() &&
            FireMontage->GetPlayLength() >= 0.7f && FrostMontage->GetPlayLength() >= 0.7f)) return false;
        const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Settings));
        if (!Test.TestNotNull(TEXT("夹具前置：独立真实World"), World.Get())) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
        World->InitializeActorsForPlay(FURL());
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        Platform.Reset(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
        Service.Reset(NewObject<UDivineBeastsDevelopmentCastingWorldSubsystem>(World.Get()));
        Observer.Reset(NewObject<UDivineBeastsDevelopmentCastingReentryObserver>());
        Service->LifetimeToken = FGuid::NewGuid();
        Service->PlatformPresentation = Platform.Get();
        // 资源已经真实Load并强持有；仅绕过本测试不覆盖的Data预热，不伪造Data完成或租约。
        Service->bReady = true;
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Character = World->SpawnActor<ACharacter>(Spawn);
        if (!Test.TestNotNull(TEXT("夹具前置：真实角色"), Character)) return false;
        Cast = NewObject<UGamePlatformAbilityCastComponent>(Character);
        Combat = NewObject<UGamePlatformCombatComponent>(Character);
        Identity = NewObject<UDivineBeastsCharacterComponent>(Character);
        for (UActorComponent* Component : {static_cast<UActorComponent*>(Cast),
                static_cast<UActorComponent*>(Combat), static_cast<UActorComponent*>(Identity)})
        { Character->AddInstanceComponent(Component); Component->RegisterComponent(); }
        if (!SetIdentityGeneration(1)) return false;
        Character->DispatchBeginPlay();
        if (bInstallAnimation && !InstallAnimation()) return false;
        Provider = Platform->RegisterProvider(TEXT("Test.DevelopmentCasting.RequestBoundary"), 0,
            FGamePlatformPresentationProviderHandler::CreateLambda([this](const FGamePlatformPresentationRequest& Request)
            {
                if (Request.PredictionState == EGamePlatformPresentationPredictionState::Cancelled)
                { return LiveRequests.Remove(Request.RequestId) > 0; }
                if (!bProviderReady || !Request.IsValid() || !Request.AttachComponent.IsValid() ||
                    Request.Lifetime != EGamePlatformPresentationLifetime::Persistent ||
                    Request.Context.AvatarGeneration <= 0 || Request.RequestGeneration <= 0 ||
                    Request.SourceId != TEXT("test.development.cast@1") ||
                    (Request.SemanticTag.ToString() != TEXT("Presentation.DivineBeasts.Development.Magic.Fireball.Cast") &&
                     Request.SemanticTag.ToString() != TEXT("Presentation.DivineBeasts.Development.Magic.Frostbolt.Cast") &&
                     Request.SemanticTag.ToString() != TEXT("Presentation.DivineBeasts.Development.Magic.DirectedMissile.Cast"))) return false;
                LiveRequests.Add(Request.RequestId); return true;
            }));
        RequestObserver = Platform->OnRequestObserved().AddLambda(
            [this](const FGamePlatformPresentationRequest& Request, EGamePlatformPresentationSubmitResult Result)
            {
                // 本夹具原有计数只针对动画/VFX；音效由独立观察者检验。
                if(!Request.SemanticTag.ToString().StartsWith(TEXT("Presentation.DivineBeasts.Development.Magic."))) return;
                if (Request.PredictionState == EGamePlatformPresentationPredictionState::Cancelled)
                {
                    ++CancelNotifications;
                    auto Action = MoveTemp(CancellationAction);
                    if (Action) Action();
                    return;
                }
                ++Attempts;
                if (Result == EGamePlatformPresentationSubmitResult::Submitted) ++Accepted;
            });
        Service->ObserveActor(Character);
        return Test.TestTrue(TEXT("夹具前置：真实Cast订阅建立"), Cast->OnCastFactsChanged().IsBoundToObject(Service.Get()));
    }

    /** 真实Mesh装配触发引擎OnAnimInitialized；不从测试直接调用拟新增的生产重试方法。 */
    bool InstallAnimation()
    {
        Character->GetMesh()->SetSkeletalMesh(MeshAsset.Get());
        Character->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
        auto* Anim = Character->GetMesh()->GetAnimInstance();
        if (!Test.TestNotNull(TEXT("夹具前置：实际AnimInstance"), Anim)) return false;
        Anim->OnMontageStarted.AddUniqueDynamic(Observer.Get(),
            &UDivineBeastsDevelopmentCastingReentryObserver::HandleMontageStarted);
        return true;
    }

    /** 只构造隔离观察者接收到的复制身份值；不调用权威初始化、不发网络或加载Hero定义。 */
    bool SetIdentityGeneration(int32 Generation)
    {
        const FStructProperty* Runtime = FindFProperty<FStructProperty>(Identity->GetClass(), TEXT("RuntimeState"));
        const FIntProperty* Avatar = Runtime ? FindFProperty<FIntProperty>(Runtime->Struct, TEXT("AvatarGeneration")) : nullptr;
        if (!Test.TestNotNull(TEXT("夹具前置：真实角色身份代次字段"), Avatar)) return false;
        Avatar->SetPropertyValue_InContainer(Runtime->ContainerPtrToValuePtr<void>(Identity), Generation);
        return true;
    }
    /** 真实公开发布API，默认保留0.7秒生命周期夹具；2秒冰箭回归显式传入，不改变权威技能。 */
    bool Publish(FGuid Id, bool bFrost = false, int32 Generation = 1, float DurationSeconds = 0.7f)
    {
        return Cast->PublishConfirmedCast(Id, TEXT("test.development.cast@1"),
            bFrost ? FName(TEXT("DBA.Development.Frostbolt")) : FName(TEXT("DBA.Development.Fireball")), Generation, DurationSeconds);
    }
    /** 给生产内容就绪入口投递确定状态；本用例只测重试，不宣称Data包激活/资源加载成功。 */
    void NotifyContentActive()
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        Handle.Id = FGuid::NewGuid(); Handle.ScopeId = FGuid::NewGuid(); Handle.Generation = 1;
        Service->HandlePack(Handle, EDivineBeastsPresentationContentPackState::Active, FString());
    }
    int32 RegisteredAnchorCount() const
    {
        TArray<USceneComponent*> Components; Character->GetComponents(Components);
        int32 Count = 0;
        for (const auto* Component : Components)
        {
            if (!Component || !Component->IsRegistered() || Component->GetClass() != USceneComponent::StaticClass()) continue;
            const bool bHand = Component->GetAttachParent() == Character->GetMesh() &&
                (Component->GetAttachSocketName() == TEXT("hand_r") || Component->GetAttachSocketName() == TEXT("hand_l"));
            const bool bGround = Component->GetAttachParent() == Character->GetCapsuleComponent() &&
                Component->GetRelativeLocation().Equals(FVector(0, 0, -Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
            if (bHand || bGround) ++Count;
        }
        return Count;
    }
    bool HasOwnedStopTimer() const
    {
        const auto* Binding = Service->Bindings.Find(Cast);
        return Binding && World->GetTimerManager().TimerExists(Binding->StopTimer);
    }
    FGuid OwnedActivation() const
    { const auto* Binding = Service->Bindings.Find(Cast); return Binding ? Binding->ActivationId : FGuid(); }
    int32 BindingCount() const { return Service->Bindings.Num(); }
    void Deinitialize() { bDeinitialized = true; Service->Deinitialize(); }
    /** 构造已经过时的接收快照；实际通知仍走Cast组件已公开的同一个原生委托。 */
    void NotifyOldFacts(const FGamePlatformAbilityCastFacts& Facts) { Cast->OnCastFactsChanged().Broadcast(Cast, Facts); }
    /** 仅供致命回归；真实新角色/组件使生产TMap扩容，不测试C++容器本身。 */
    bool GrowBindingsDuringStarted()
    {
        const void* Before = Service->Bindings.Find(Cast);
        for (int32 Index = 0; Index < 128; ++Index)
        {
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Other = World->SpawnActor<ACharacter>(Spawn);
            auto* OtherCast = NewObject<UGamePlatformAbilityCastComponent>(Other);
            Other->AddInstanceComponent(OtherCast); OtherCast->RegisterComponent(); Service->ObserveActor(Other);
        }
        return Before != Service->Bindings.Find(Cast);
    }
};

// 每项注释说明它捕获的生产缺陷；return只结束Automation调度，断言失败仍是实际失败。
#define DBA_CASTING_TEST(ClassName, Suffix) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "DivineBeasts.Presentation.DevelopmentCasting.Regression." Suffix, \
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool ClassName::RunTest(const FString&)

/** 数字1定向飞弹：从真实施法事实生成左右独立请求，参数来自权威窗口，取消两手都释放。 */
DBA_CASTING_TEST(FDivineBeastsDirectedMissileDualPalmTest, "DirectedMissileDualPalmLifecycle")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    TMap<FName,FGamePlatformPresentationRequest> Palms;
    const auto Watch=F.Platform->OnRequestObserved().AddLambda(
        [&Palms](const FGamePlatformPresentationRequest& Request,EGamePlatformPresentationSubmitResult Result)
        {
            if(Result==EGamePlatformPresentationSubmitResult::Submitted &&
               Request.PredictionState!=EGamePlatformPresentationPredictionState::Cancelled &&
               Request.SemanticTag.ToString()==TEXT("Presentation.DivineBeasts.Development.Magic.DirectedMissile.Cast") &&
               Request.AttachComponent.IsValid())
                Palms.Add(Request.AttachComponent->GetAttachSocketName(),Request);
        });
    const FGuid Id=FGuid::NewGuid();
    TestTrue(TEXT("真实1号前摇事实发布"),F.Cast->PublishConfirmedCast(Id,TEXT("test.development.cast@1"),
        TEXT("DBA.Development.DirectedMissile"),1,0.5f));
    TestEqual(TEXT("1号前摇必须提交两只手"),F.LiveRequests.Num(),2);
    TestEqual(TEXT("两手真实挂点独立"),Palms.Num(),2);
    TestEqual(TEXT("两个受控锚点"),F.RegisteredAnchorCount(),2);
    for(const auto& Pair:Palms)
    {
        const auto& Request=Pair.Value;
        TestTrue(TEXT("只接受实际左右手骨骼"),Pair.Key==TEXT("hand_l") || Pair.Key==TEXT("hand_r"));
        TestEqual(TEXT("使用复制施法窗口"),Request.FloatParameters.FindRef(TEXT("User.CastDurationSec")),0.5f);
        TestTrue(TEXT("起始时间偏移显式传递"),Request.FloatParameters.Contains(TEXT("User.ElapsedAtStartSec")));
        TestEqual(TEXT("左右旋向分别配置"),Request.FloatParameters.FindRef(TEXT("User.HandSideSign")),
            Pair.Key==TEXT("hand_l") ? -1.0f : 1.0f);
    }
    F.NotifyContentActive();
    TestEqual(TEXT("内容就绪重试不重复生成"),F.LiveRequests.Num(),2);
    TestTrue(TEXT("取消真实1号前摇"),F.Cast->CancelConfirmedCast(Id));
    TestEqual(TEXT("取消后两手请求全部撤销"),F.LiveRequests.Num(),0);
    TestEqual(TEXT("取消后两手锚点全部释放"),F.RegisteredAnchorCount(),0);
    F.Platform->OnRequestObserved().Remove(Watch);
    return true;
}

/** 首掌Submit同步取消时不能遗留首掌，也不能继续创建次掌。 */
DBA_CASTING_TEST(FDivineBeastsDirectedMissilePalmReentryTest, "DirectedMissilePalmSubmitCancellation")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const FGuid Id=FGuid::NewGuid(); bool bCancelled=false;
    const auto Watch=F.Platform->OnRequestObserved().AddLambda(
        [&](const FGamePlatformPresentationRequest& Request,EGamePlatformPresentationSubmitResult Result)
        {
            if(!bCancelled && Result==EGamePlatformPresentationSubmitResult::Submitted &&
               Request.PredictionState!=EGamePlatformPresentationPredictionState::Cancelled &&
               Request.SemanticTag.ToString()==TEXT("Presentation.DivineBeasts.Development.Magic.DirectedMissile.Cast"))
            { bCancelled=true; F.Cast->CancelConfirmedCast(Id); }
        });
    F.Cast->PublishConfirmedCast(Id,TEXT("test.development.cast@1"),TEXT("DBA.Development.DirectedMissile"),1,0.5f);
    TestTrue(TEXT("首掌提交内确实同步取消"),bCancelled);
    TestEqual(TEXT("同步取消不留下任一掌请求"),F.LiveRequests.Num(),0);
    TestEqual(TEXT("同步取消不留下任一掌锚点"),F.RegisteredAnchorCount(),0);
    TestEqual(TEXT("取消后不继续提交次掌"),F.Accepted,1);
    F.Platform->OnRequestObserved().Remove(Watch);
    return true;
}

/** 火球/冰箭默认0.5秒事实必须同样铺满真实动作，不能让火球沿用固定原始速率。 */
DBA_CASTING_TEST(FDivineBeastsCastingHalfSecondSpellsTest, "FireAndFrostHalfSecondMontage")
{
    for (const bool bFrost : {false, true})
    {
        FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
        const FGuid Id=FGuid::NewGuid();
        TestTrue(TEXT("0.5秒火冰事实受理"),F.Publish(Id,bFrost,1,0.5f));
        auto* Montage=bFrost ? F.FrostMontage.Get() : F.FireMontage.Get();
        const auto* Instance=F.Character->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(Montage);
        if (TestNotNull(TEXT("真实火冰动作实例"),Instance))
            TestEqual(TEXT("动作完整映射复制的0.5秒窗口"),Instance->GetPlayRate(),Montage->GetPlayLength()/0.5f);
        TestTrue(TEXT("取消0.5秒起手"),F.Cast->CancelConfirmedCast(Id));
        TestEqual(TEXT("取消撤销自己的表现请求"),F.LiveRequests.Num(),0);
    }
    return true;
}

/** 非默认显式2秒冰箭：仍按复制时长缩放，不能把客户端硬编码成0.5秒。 */
DBA_CASTING_TEST(FDivineBeastsCastingFrostTwoSecondsTest, "FrostTwoSecondMontage")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const FGuid Id=FGuid::NewGuid();
    TestTrue(TEXT("2秒冰箭事实受理"),F.Publish(Id,true,1,2.0f));
    auto* Anim=F.Character->GetMesh()->GetAnimInstance();
    const auto* Instance=Anim->GetActiveInstanceForMontage(F.FrostMontage.Get());
    if (TestNotNull(TEXT("真实冰箭动作实例"),Instance))
        TestEqual(TEXT("完整动作铺满复制的2秒窗口"),Instance->GetPlayRate(),F.FrostMontage->GetPlayLength()/2.0f);
    TestEqual(TEXT("服务器事实保持2秒"),F.Cast->GetCastFacts().DurationSeconds,2.0f);
    TestTrue(TEXT("取消2秒起手"),F.Cast->CancelConfirmedCast(Id));
    TestEqual(TEXT("取消撤销自己的表现请求"),F.LiveRequests.Num(),0);
    return true;
}

/** 缺陷1：真实OnMontageStarted同步取消后，旧栈不得再创建锚点、Timer或补交VFX。 */
DBA_CASTING_TEST(FDivineBeastsCastingStartedCancellationTest, "StartedCancellationLeavesNoLateOwnership")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const FGuid Id = FGuid::NewGuid(); bool bCancelled = false;
    F.Observer->StartedAction = [&](UAnimMontage*) { bCancelled = F.Cast->CancelConfirmedCast(Id); };
    TestTrue(TEXT("原确认发布受理"), F.Publish(Id));
    TestTrue(TEXT("真实同步启动通知取消当前施法"), bCancelled);
    TestFalse(TEXT("取消后事实非活跃"), F.Cast->GetCastFacts().bActive);
    TestEqual(TEXT("取消通知返回后不得补交起手VFX"), F.Attempts, 0);
    TestEqual(TEXT("取消后不得留下手部锚点"), F.RegisteredAnchorCount(), 0);
    TestFalse(TEXT("取消后不得留下失去所有者的Timer"), F.HasOwnedStopTimer());
    return true;
}

/** 缺陷1：启动通知发布后继时，旧栈不得覆盖后继VFXRequestId或遗留前继锚点。 */
DBA_CASTING_TEST(FDivineBeastsCastingStartedSuccessorTest, "StartedSuccessorRetainsRequestOwnership")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const FGuid First = FGuid::NewGuid(), Next = FGuid::NewGuid(); bool bNextAccepted = false;
    F.Observer->StartedAction = [&](UAnimMontage*) { bNextAccepted = F.Publish(Next, true); };
    TestTrue(TEXT("前继确认发布"), F.Publish(First)); TestTrue(TEXT("启动通知内真实发布后继"), bNextAccepted);
    TestEqual(TEXT("只后继提交一次有效VFX"), F.Accepted, 1);
    TestEqual(TEXT("账本属于后继"), F.OwnedActivation(), Next);
    TestEqual(TEXT("后继只持有一个技能挂点，冰箭遵循脚底法阵合同"), F.RegisteredAnchorCount(), 1);
    TestTrue(TEXT("真实撤销后继"), F.Cast->CancelConfirmedCast(Next));
    TestEqual(TEXT("撤销必须命中后继真实RequestId"), F.LiveRequests.Num(), 0);
    TestEqual(TEXT("后继撤销后锚点全部释放"), F.RegisteredAnchorCount(), 0);
    return true;
}

/** 缺陷2：先有动画、后有提供者/包就绪，重试只能补VFX，不能被动画Activation去重挡住。 */
DBA_CASTING_TEST(FDivineBeastsCastingLateContentTest, "ContentActivationRetriesVFXWithoutReplayingMontage")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    F.bProviderReady = false; TestTrue(TEXT("内容未就绪时真实发布"), F.Publish(FGuid::NewGuid()));
    TestEqual(TEXT("内容未就绪没有受理VFX"), F.Accepted, 0);
    TestTrue(TEXT("内容未就绪仍运行可选独立动画"), F.Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.FireMontage.Get()));
    const int32 Before = F.Observer->StartedNotifications;
    F.bProviderReady = true; F.NotifyContentActive(); F.NotifyContentActive();
    TestEqual(TEXT("就绪和重复就绪只受理一次VFX"), F.Accepted, 1);
    TestEqual(TEXT("VFX重试不重播动画"), F.Observer->StartedNotifications, Before);
    TestEqual(TEXT("重复就绪只有一个活跃Request"), F.LiveRequests.Num(), 1);
    return true;
}

/** 缺陷3：事实先到、实际AnimInstance稍后创建，只能靠真实初始化通知重评估当前窗口。 */
DBA_CASTING_TEST(FDivineBeastsCastingLateAnimationTest, "AnimationInitializationRetriesCurrentWindow")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize(false)) return false;
    TestTrue(TEXT("无动画实例时事实已真实发布"), F.Publish(FGuid::NewGuid()));
    TestEqual(TEXT("动画未就绪不提交起手"), F.Accepted, 0);
    if (!F.InstallAnimation()) return false;
    TestTrue(TEXT("实际OnAnimInitialized后立即运行当前施法"), F.Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.FireMontage.Get()));
    TestEqual(TEXT("动画就绪后当前窗口提交一次起手"), F.Accepted, 1);
    return true;
}

/** 缺陷3：新施法先于角色身份，身份随后到达而Combat副本仍旧；项目身份是只读同代接收合同。 */
DBA_CASTING_TEST(FDivineBeastsCastingLateIdentityTest, "IdentityGenerationRetriesBeforeCombatReplica")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    TestTrue(TEXT("代次2施法先到"), F.Publish(FGuid::NewGuid(), false, 2));
    TestEqual(TEXT("代次1身份不能播放代次2窗口"), F.Accepted, 0);
    if (!F.SetIdentityGeneration(2)) return false;
    F.Identity->OnIdentityChanged().Broadcast();
    TestEqual(TEXT("夹具保持真实Combat副本仍为旧代次1"), F.Combat->GetCombatAvatarGeneration(), 1);
    TestTrue(TEXT("同代角色身份到达后当前施法恢复"), F.Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.FireMontage.Get()));
    TestEqual(TEXT("权威Avatar接收合同就绪后补一次起手"), F.Accepted, 1);
    return true;
}

/** 缺陷4：A取消广播先重入发布B，返回外层后仍会投递A的值快照，不能因此停止B。 */
DBA_CASTING_TEST(FDivineBeastsCastingOldCancellationTest, "NestedCancellationCannotStopSuccessor")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const FGuid First = FGuid::NewGuid(), Next = FGuid::NewGuid();
    TestTrue(TEXT("前继发布"), F.Publish(First)); bool bNextAccepted = false;
    const FDelegateHandle Reentry = F.Cast->OnCastFactsChanged().AddLambda(
        [&](UGamePlatformAbilityCastComponent*, const FGamePlatformAbilityCastFacts& Facts)
        { if (Facts.ActivationId == First && !Facts.bActive) bNextAccepted = F.Publish(Next, true); });
    TestTrue(TEXT("真实取消A并进入嵌套发布"), F.Cast->CancelConfirmedCast(First));
    F.Cast->OnCastFactsChanged().Remove(Reentry);
    TestTrue(TEXT("嵌套B受理"), bNextAccepted);
    TestEqual(TEXT("组件最新事实属于B"), F.Cast->GetCastFacts().ActivationId, Next);
    TestEqual(TEXT("旧取消快照不能清除B账本"), F.OwnedActivation(), Next);
    TestTrue(TEXT("后继Montage仍运行"), F.Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.FrostMontage.Get()));
    TestEqual(TEXT("后继起手仍只有一个活跃Request"), F.LiveRequests.Num(), 1);
    return true;
}

/** 控制用例：先播放自有Fire，再由外消费者播放同组Frost；撤销旧Fire不能误停后来Frost。 */
DBA_CASTING_TEST(FDivineBeastsCastingUnownedMontageTest, "CancellationPreservesUnownedMontage")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    auto* Anim = F.Character->GetMesh()->GetAnimInstance();
    const FGuid Id = FGuid::NewGuid(); TestTrue(TEXT("本服务实际启动Fire"), F.Publish(Id));
    TestTrue(TEXT("外消费者随后实际播放同组Frost"), Anim->Montage_Play(F.FrostMontage.Get(), 1.f, EMontagePlayReturnType::MontageLength, 0.f, false) > 0);
    TestTrue(TEXT("撤销自有Fire"), F.Cast->CancelConfirmedCast(Id));
    TestTrue(TEXT("撤销后另一消费者Frost继续运行"), Anim->Montage_IsPlaying(F.FrostMontage.Get()));
    return true;
}

/** 同资产缺口：Started内外消费者真实重播Fire后，服务不能把返回时的活动实例误认领为自有实例。 */
DBA_CASTING_TEST(FDivineBeastsCastingSameAssetReplayedDuringStartedTest, "SameAssetReplayedDuringStartedRemainsUnowned")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    auto* Anim = F.Character->GetMesh()->GetAnimInstance();
    const FGuid Id = FGuid::NewGuid();
    float ForeignPlayedLength = 0.0f;
    int32 ForeignInstanceId = INDEX_NONE;
    // 既有观察者先取走StartedAction，因此嵌套真实Montage_Play不会无限递归；不改资产/槽位/骨架。
    F.Observer->StartedAction = [&](UAnimMontage* StartedMontage)
    {
        if (StartedMontage != F.FireMontage.Get()) return;
        ForeignPlayedLength = Anim->Montage_Play(F.FireMontage.Get(), 1.0f,
            EMontagePlayReturnType::MontageLength, 0.0f, false);
        if (const auto* Instance = Anim->GetActiveInstanceForMontage(F.FireMontage.Get()))
            ForeignInstanceId = Instance->GetInstanceID();
    };
    TestTrue(TEXT("本服务真实发布并启动Fire"), F.Publish(Id));
    if (!TestTrue(TEXT("前置：Started内同一资产外部重播真实受理"), ForeignPlayedLength > 0.0f) ||
        !TestTrue(TEXT("前置：外部重播实际实例ID有效"), ForeignInstanceId != INDEX_NONE) ||
        !TestTrue(TEXT("前置：取消前外部Fire正在播放"), Anim->Montage_IsPlaying(F.FireMontage.Get()))) return false;
    TestTrue(TEXT("真实撤销本服务原施法事实"), F.Cast->CancelConfirmedCast(Id));
    TestTrue(TEXT("取消ownFact不能停止Started内同资产外部重播"), Anim->Montage_IsPlaying(F.FireMontage.Get()));
    const auto* RemainingInstance = Anim->GetActiveInstanceForMontage(F.FireMontage.Get());
    TestTrue(TEXT("取消后保留的是原外部实例而非重新播放"),
        RemainingInstance && RemainingInstance->GetInstanceID() == ForeignInstanceId);
    TestFalse(TEXT("原施法取消后没有自有StopTimer"), F.HasOwnedStopTimer());
    TestEqual(TEXT("原施法取消后没有自有手部锚点"), F.RegisteredAnchorCount(), 0);
    TestEqual(TEXT("原施法取消后自有VFX请求已释放"), F.LiveRequests.Num(), 0);
    return true;
}

/** 控制用例：过期输入不能由新的动画/内容就绪事件复活；手写时间只构造网络接收快照，不改世界时钟。 */
DBA_CASTING_TEST(FDivineBeastsCastingExpiredFactsTest, "ExpiredFactsCannotReplayAfterReadiness")
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize(false)) return false;
    FGamePlatformAbilityCastFacts Expired;
    Expired.ActivationId = FGuid::NewGuid(); Expired.AbilityId = TEXT("test.development.cast@1");
    Expired.PresentationStyleId = TEXT("DBA.Development.Fireball"); Expired.AvatarGeneration = 1;
    Expired.bActive = true; Expired.StartedServerSeconds = -2.0; Expired.DurationSeconds = 0.7f;
    // 设置实际只读复制源，随后通过组件真实通知和引擎初始化事件进入生产代码。
    const FStructProperty* Property = FindFProperty<FStructProperty>(F.Cast->GetClass(), TEXT("Facts"));
    if (!TestNotNull(TEXT("夹具前置：真实复制Facts字段"), Property)) return false;
    *Property->ContainerPtrToValuePtr<FGamePlatformAbilityCastFacts>(F.Cast) = Expired;
    F.NotifyOldFacts(Expired); if (!F.InstallAnimation()) return false; F.NotifyContentActive();
    TestFalse(TEXT("已过期窗口不能补播Montage"), F.Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.FireMontage.Get()));
    TestEqual(TEXT("已过期窗口不能补交VFX"), F.Accepted, 0);
    TestEqual(TEXT("已过期窗口不能生成锚点"), F.RegisteredAnchorCount(), 0);
    return true;
}
#undef DBA_CASTING_TEST

/** 致命缺陷：Deinitialize取消VFX同步通知销毁Actor，EndPlay移除账本后FindChecked不能再借用旧条目。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastingDeinitializeReentryTest,
    "DivineBeasts.Presentation.DevelopmentCasting.CrashRegression.DeinitializeActorEndPlayDuringCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastingDeinitializeReentryTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    TestTrue(TEXT("真实动作启动"), F.Publish(FGuid::NewGuid()));
    bool bDestroyed = false;
    F.CancellationAction = [&] { bDestroyed = F.Character->Destroy(); };
    AddInfo(TEXT("CRASH_RED_TRIGGER: Deinitialize -> StopCast -> Submit(Cancel) -> Actor::Destroy -> OnEndPlay -> Bindings.Remove"));
    F.Deinitialize();
    TestTrue(TEXT("取消通知真实销毁Actor"), bDestroyed);
    TestEqual(TEXT("关闭后账本全部释放"), F.BindingCount(), 0);
    TestEqual(TEXT("关闭后实例全部撤销"), F.LiveRequests.Num(), 0);
    return true;
}

/** 致命缺陷：Montage同步启动通知使真实绑定表搬迁，返回原栈必须重新取得自己的Timer/Anchor账本。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastingMapGrowthReentryTest,
    "DivineBeasts.Presentation.DevelopmentCasting.CrashRegression.BindingGrowthDuringMontageStarted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastingMapGrowthReentryTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    bool bMoved = false;
    F.Observer->StartedAction = [&](UAnimMontage*) { bMoved = F.GrowBindingsDuringStarted(); };
    AddInfo(TEXT("CRASH_RED_TRIGGER: Montage_Play -> OnMontageStarted -> ObserveActor x128 -> Bindings relocation -> original continuation"));
    TestTrue(TEXT("真实发布窗口"), F.Publish(FGuid::NewGuid()));
    TestTrue(TEXT("夹具确实使实际账本地址搬迁"), bMoved);
    TestTrue(TEXT("搬迁后自有StopTimer必须在当前账本"), F.HasOwnedStopTimer());
    TestEqual(TEXT("搬迁后锚点只创建一次"), F.RegisteredAnchorCount(), 1);
    TestTrue(TEXT("撤销当前动作"), F.Cast->CancelConfirmedCast(F.Cast->GetCastFacts().ActivationId));
    TestEqual(TEXT("当前账本必须仍拥有可清理锚点"), F.RegisteredAnchorCount(), 0);
    return true;
}
/** 声音须在真实施法窗口开始，VFX不可用不静音；重复通知不重播，取消立即收敛。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastingAudioTimingTest,
    "DivineBeasts.Presentation.DevelopmentCasting.AudioStartsWithCastAndStopsOnCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastingAudioTimingTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentCastingTestFixture F(*this); if (!F.Initialize()) return false;
    const auto SoundTag=FGameplayTag::RequestGameplayTag(TEXT("Presentation.SSLM.Audio.Spell.Frost.Cast"));
    int32 Started=0, Cancelled=0; FGuid SoundRequest;
    const auto Provider=F.Platform->RegisterProvider(TEXT("Test.CastAudio"),1,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& R)
        { return R.SemanticTag==SoundTag; }));
    const auto Observer=F.Platform->OnRequestObserved().AddLambda(
        [&](const FGamePlatformPresentationRequest& R,EGamePlatformPresentationSubmitResult Result)
        {
            if(R.SemanticTag!=SoundTag) return;
            if(R.PredictionState==EGamePlatformPresentationPredictionState::Cancelled)
            { ++Cancelled; TestEqual(TEXT("取消属于原施法声音"),R.RequestId,SoundRequest); }
            else if(Result==EGamePlatformPresentationSubmitResult::Submitted)
            { ++Started; SoundRequest=R.RequestId; }
        });
    F.bProviderReady=false; const auto CastId=FGuid::NewGuid();
    TestTrue(TEXT("真实开始两秒施法"),F.Publish(CastId,true,1,2.0f));
    TestEqual(TEXT("弹体尚未生成时施法声已经开始，VFX缺失不影响"),Started,1);
    F.NotifyContentActive(); TestEqual(TEXT("重复就绪不重播声音"),Started,1);
    TestTrue(TEXT("真实取消施法"),F.Cast->CancelConfirmedCast(CastId));
    TestEqual(TEXT("取消施法同时取消声音"),Cancelled,1);
    F.NotifyContentActive(); TestEqual(TEXT("取消后不补播"),Started,1);
    TestTrue(TEXT("开始下一次独立施法"),F.Publish(FGuid::NewGuid(),true,1,2.0f));
    TestEqual(TEXT("新施法可以独立起播"),Started,2);
    F.Deinitialize();
    TestEqual(TEXT("世界作用域失活取消仍在施法的声音"),Cancelled,2);
    F.NotifyContentActive(); TestEqual(TEXT("失活后内容通知不能复活声音"),Started,2);
    F.Platform->OnRequestObserved().Remove(Observer); F.Platform->UnregisterProvider(Provider);
    return true;
}
#endif
