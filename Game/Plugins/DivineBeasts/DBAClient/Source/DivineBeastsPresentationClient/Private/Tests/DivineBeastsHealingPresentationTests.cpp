/**
 * SSLM项目层治疗表现目标与生命周期回归；Editor自动化在游戏线程调用。
 * 使用真实World、LocalPlayer、ACharacter胶囊、平台ASC/Combat与确认反馈总线；
 * 通过Combat公开重生/属性结算入口建立代次和死亡，不反射写入私有战斗状态。
 * Active内容包账本及Platform provider仅是本测试的请求边界替身，不加载/保存资源，
 * 不证明Niagara、Data预载、网络复制、Cook或最终视觉已经通过。
 * friend访问仅用于装配隔离服务与直接投递非法事件；不访问其他模块Private头文件。
 * 所有观察者和provider均在本作用域退出前撤销，World拥有角色、组件和总线。
 */
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformCombatSpec.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHealingPresentationTest,
    "DivineBeasts.Presentation.Healing.TargetAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHealingPresentationTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("前置：真实Engine"), GEngine)) return false;
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &Settings));
    TStrongObjectPtr<UWorld> OtherWorld(UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &Settings));
    if (!TestTrue(TEXT("前置：两个隔离真实World"), World.IsValid() && OtherWorld.IsValid())) return false;
    World->InitializeActorsForPlay(FURL()); OtherWorld->InitializeActorsForPlay(FURL());
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Service(NewObject<UDivineBeastsPresentationClientSubsystem>(Player.Get()));
    TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    FDelegateHandle Observer; FGuid Provider;
    ON_SCOPE_EXIT
    {
        Service->UnbindDevelopmentMagicWorld();
        Platform->OnRequestObserved().Remove(Observer);
        Platform->UnregisterProvider(Provider);
        OtherWorld->DestroyWorld(false); World->DestroyWorld(false);
    };

    // 只替代Data/目录预载的受理边界；生命周期解绑和治疗目标投影仍调用生产实现。
    Service->DevelopmentMagicWorld = World.Get(); Service->ScopeId = FGuid::NewGuid();
    Service->DevelopmentMagicBindingToken = FGuid::NewGuid(); Service->PlatformPresentation = Platform.Get();
    FDivineBeastsPresentationContentPackHandle Pack;
    Pack.Id = FGuid::NewGuid(); Pack.ScopeId = Service->ScopeId; Pack.Generation = 1;
    UDivineBeastsPresentationClientSubsystem::FActivePack Active; Active.Handle = Pack;
    Service->DevelopmentMagicPack = Pack; Service->ActivePacks.Add(Pack.Id, Active);
    auto* Feedback = World->GetSubsystem<UGamePlatformCombatFeedbackWorldSubsystem>();
    if (!TestNotNull(TEXT("前置：真实World确认战斗总线"), Feedback)) return false;
    Service->DevelopmentHealingFeedbackHandle = Feedback->OnConfirmedFeedback().AddUObject(
        Service.Get(), &UDivineBeastsPresentationClientSubsystem::HandleDevelopmentHealingFeedback);

    const auto SpawnCombatCharacter = [](UWorld& OwnerWorld, const FVector& Location, UGamePlatformCombatComponent*& OutCombat)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Character = OwnerWorld.SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Character) return static_cast<ACharacter*>(nullptr);
        Character->GetCharacterMovement()->DisableMovement();
        auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Character);
        OutCombat = NewObject<UGamePlatformCombatComponent>(Character);
        Character->AddInstanceComponent(ASC); Character->AddInstanceComponent(OutCombat);
        ASC->RegisterComponent(); OutCombat->RegisterComponent();
        ASC->BindAbilityActorInfo(Character, Character); Character->DispatchBeginPlay();
        return Character;
    };
    UGamePlatformCombatComponent *SourceCombat = nullptr, *TargetCombat = nullptr, *ForeignCombat = nullptr;
    ACharacter* Source = SpawnCombatCharacter(*World, FVector(-800, 100, 900), SourceCombat);
    ACharacter* Target = SpawnCombatCharacter(*World, FVector(650, 200, 450), TargetCombat);
    ACharacter* Foreign = SpawnCombatCharacter(*OtherWorld, FVector(100, 0, 450), ForeignCombat);
    if (!TestTrue(TEXT("前置：真实来源、受疗者和另一World角色"), Source && Target && Foreign && SourceCombat && TargetCombat && ForeignCombat) ||
        !TestTrue(TEXT("前置：平台公开重生真实推进目标代次"), TargetCombat->ResetForNewAvatar(2))) return false;

    int32 Requests = 0; FGamePlatformPresentationRequest LastRequest;
    EGamePlatformPresentationSubmitResult LastResult = EGamePlatformPresentationSubmitResult::InvalidRequest;
    Provider = Platform->RegisterProvider(TEXT("Test.Healing.RequestBoundary"), 0,
        FGamePlatformPresentationProviderHandler::CreateLambda([](const FGamePlatformPresentationRequest& Request)
        { return Request.IsValid(); }));
    Observer = Platform->OnRequestObserved().AddLambda(
        [&](const FGamePlatformPresentationRequest& Request, EGamePlatformPresentationSubmitResult Result)
        {
            if (Request.PredictionState == EGamePlatformPresentationPredictionState::Cancelled) return;
            ++Requests; LastRequest = Request; LastResult = Result;
        });
    FGamePlatformCombatEvent Event;
    Event.EventId = FGuid::NewGuid(); Event.EventType = EGamePlatformCombatEventType::Healing;
    Event.SourceActor = Source; Event.TargetActor = Target;
    Event.SourceAbilityId = TEXT("dba.ability.rat_placeholder_primary@1");
    Event.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Event.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration(); Event.WorldContextGeneration = 1;
    Event.AppliedMagnitude = 20.f; Event.AppliedToHealth = 20.f;
    Feedback->DispatchConfirmedFeedback(Event);
    if (!TestEqual(TEXT("同代确认治疗向唯一平台提交一次"), Requests, 1)) return false;
    TestEqual(TEXT("平台provider实际受理"), LastResult, EGamePlatformPresentationSubmitResult::Submitted);
    TestTrue(TEXT("附着到实际受疗者根/胶囊而不是来源角色"),
        LastRequest.AttachComponent.Get() == Target->GetRootComponent() && LastRequest.AttachComponent.Get() != Source->GetRootComponent());
    const FVector ExpectedFeet = Target->GetActorLocation() - FVector(0, 0, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    TestTrue(TEXT("位置是受疗者胶囊脚底"), LastRequest.SourceLocation.Equals(ExpectedFeet, .01));
    TestFalse(TEXT("没有误用来源角色位置"), LastRequest.SourceLocation.Equals(Source->GetActorLocation(), .01));
    TestEqual(TEXT("治疗是有限时长表现"), LastRequest.Lifetime, EGamePlatformPresentationLifetime::Timed);
    TestEqual(TEXT("请求跟随目标化身代次"), LastRequest.RequestGeneration, Event.TargetAvatarGeneration);
    TestEqual(TEXT("治疗映射到唯一Impact语义"), LastRequest.SemanticTag.ToString(),
        FString(TEXT("Presentation.DivineBeasts.Development.Magic.Healing.Impact")));

    // 十二个已批准Primary身份必须全部可投影；不通过任意前缀/版本放宽白名单。
    for (const TCHAR* Hero : {TEXT("ox"), TEXT("tiger"), TEXT("rabbit"), TEXT("dragon"), TEXT("snake"),
        TEXT("horse"), TEXT("goat"), TEXT("monkey"), TEXT("rooster"), TEXT("dog"), TEXT("boar")})
    {
        Event.EventId = FGuid::NewGuid();
        Event.SourceAbilityId = FName(*(FString(TEXT("dba.ability.")) + Hero + TEXT("_placeholder_primary@1")));
        Feedback->DispatchConfirmedFeedback(Event);
    }
    TestEqual(TEXT("十二生肖Primary身份均投影"), Requests, 12);
    const auto Reject = [&](FGamePlatformCombatEvent Rejected, const TCHAR* Label)
    {
        Rejected.EventId = FGuid::NewGuid();
        Feedback->DispatchConfirmedFeedback(Rejected);
        TestEqual(Label, Requests, 12);
    };
    auto Rejected = Event; Rejected.TargetAvatarGeneration = 1;
    Reject(Rejected, TEXT("旧目标代次不播放"));
    Rejected = Event; Rejected.EventType = EGamePlatformCombatEventType::Damage;
    Reject(Rejected, TEXT("非Healing事件不播放"));
    Rejected = Event; Rejected.SourceAbilityId = TEXT("dba.ability.rat_placeholder_primary@2");
    Reject(Rejected, TEXT("未批准的技能版本不播放"));
    Rejected = Event; Rejected.SourceAbilityId = TEXT("dba.ability.rat_placeholder_fireball@1");
    Reject(Rejected, TEXT("其它真实槽位不误播治疗"));
    Rejected = Event; Rejected.TargetActor = Foreign; Rejected.TargetAvatarGeneration = ForeignCombat->GetCombatAvatarGeneration();
    Service->HandleDevelopmentHealingFeedback(Rejected); // 直接投递绕过总线的World预检，独立验证适配器仍拒绝跨World。
    TestEqual(TEXT("另一World目标不播放"), Requests, 12);
    FGamePlatformCombatSpec Damage;
    Damage.EventId = FGuid::NewGuid(); Damage.Source = Source; Damage.Target = Target;
    Damage.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Damage.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration();
    Damage.Magnitude = TargetCombat->GetCombatMaxHealth() + 1.f;
    if (!TestTrue(TEXT("公开平台伤害路径真实受理"), SourceCombat->ApplyDamage(Damage).IsSuccess())) return false;
    if (!TestTrue(TEXT("通过真实Combat属性结算进入死亡"), TargetCombat->IsCombatDead())) return false;
    Reject(Event, TEXT("死亡受疗者不播放旧治疗"));

    // 先恢复同一角色为真实新化身并证明可播放，再解绑；避免把死亡拒绝误当作解绑成功。
    if (!TestTrue(TEXT("公开重生入口建立目标新化身"), TargetCombat->ResetForNewAvatar(3))) return false;
    Event.EventId = FGuid::NewGuid(); Event.TargetAvatarGeneration = 3;
    Feedback->DispatchConfirmedFeedback(Event);
    TestEqual(TEXT("解绑前新化身治疗仍可播放"), Requests, 13);

    // Standalone/ListenServer没有客户端Multicast回调；必须从真实权威事件桥接到同一去重总线。
    // 源与受疗者都会收到同一Combat事件，重复观察Actor不能造成两次花瓣请求。
    Service->HandleDevelopmentMagicSpawn(Source); Service->HandleDevelopmentMagicSpawn(Target);
    Service->HandleDevelopmentMagicSpawn(Source);
    auto* TargetASC = Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    TargetASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetHealthAttribute(), TargetCombat->GetCombatMaxHealth() * .5f);
    FGamePlatformCombatSpec Healing;
    Healing.EventId = FGuid::NewGuid(); Healing.Source = Source; Healing.Target = Target;
    Healing.SourceAbilityId = TEXT("dba.ability.rat_placeholder_primary@1");
    Healing.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Healing.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration(); Healing.Magnitude = 20.f;
    TestTrue(TEXT("本地主机通过真实平台治疗结算"), SourceCombat->ApplyHealing(Healing).IsSuccess());
    TestEqual(TEXT("来源和目标同一权威治疗只投影一次"), Requests, 14);
    TestEqual(TEXT("本地主机治疗请求保留权威事件ID"), LastRequest.RequestId, Healing.EventId);
    Service->UnbindDevelopmentMagicWorld();
    TestFalse(TEXT("解绑清除自有确认反馈句柄"), Service->DevelopmentHealingFeedbackHandle.IsValid());
    TestFalse(TEXT("解绑撤销来源的本地权威事件订阅"),
        SourceCombat->OnCombatEvent.IsAlreadyBound(Service.Get(), &UDivineBeastsPresentationClientSubsystem::HandleDevelopmentAuthorityHealingFeedback));
    TestFalse(TEXT("解绑撤销目标的本地权威事件订阅"),
        TargetCombat->OnCombatEvent.IsAlreadyBound(Service.Get(), &UDivineBeastsPresentationClientSubsystem::HandleDevelopmentAuthorityHealingFeedback));
    Event.EventId = FGuid::NewGuid(); Feedback->DispatchConfirmedFeedback(Event);
    TestEqual(TEXT("解绑后的真实总线通知不播放"), Requests, 14);
    Event.EventId = FGuid::NewGuid(); Service->HandleDevelopmentHealingFeedback(Event);
    TestEqual(TEXT("解绑后的迟到直接回调不播放"), Requests, 14);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
