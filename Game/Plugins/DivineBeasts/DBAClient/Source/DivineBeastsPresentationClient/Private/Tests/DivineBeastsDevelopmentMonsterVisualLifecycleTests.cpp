// 项目客户端开发石灵原生回归：真实保存的BP/UIFont、World/EngineContext和ASC/Combat，检查只读生命装配与同步重入。
// 同步Load仅作为本测试资源前置，friend仅驱动资源就绪/历史通知；不伪造Data完成、租约、伤害结果或资产。
// 本文件不验证生产Data异步加载/跨世界租约、联机复制、GPU文字渲染或人工视觉质量；主执行器另做集成验证。
#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsDevelopmentMonsterVisualWorldSubsystem.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "String/LexFromString.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Types/GamePlatformCombatSpec.h"
#include "UObject/StrongObjectPtr.h"

/** friend夹具独占瞬态世界及装配服务，伤害只能从真实SourceCombat.ApplyDamage进入。 */
struct FDivineBeastsDevelopmentMonsterVisualTestFixture
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UClass> VisualClass;
    TStrongObjectPtr<UFont> LabelFont;
    TStrongObjectPtr<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem> Service;
    ADivineBeastsDevelopmentMonster* Monster = nullptr;
    AActor* Source = nullptr;
    UGamePlatformCombatComponent* SourceCombat = nullptr;
    FDelegateHandle TestSpawnHandle;
    FDelegateHandle TestTransformHandle;
    TStrongObjectPtr<USceneComponent> TransformSource;
    bool bWorldContextCreated = false;

    /** 前置失败明确停止：真实资源缺失不能被当作装配行为的RED，也不能以替身补足。 */
    bool Create(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("须从真实开发配置和命令行进入"),
            ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess()) ||
            !Test.TestNotNull(TEXT("Engine可注册真实WorldContext"), GEngine)) return false;
        VisualClass.Reset(LoadClass<AActor>(nullptr,
            TEXT("/DBAContentPack_Common/Development/Monsters/BP_DBA_AncientStoneSpiritVisual.BP_DBA_AncientStoneSpiritVisual_C")));
        LabelFont.Reset(LoadObject<UFont>(nullptr,
            TEXT("/DBAUIPack_Core/UI/Fonts/Development/F_DBA_DevelopmentMonsterLabel.F_DBA_DevelopmentMonsterLabel")));
        if (!Test.TestNotNull(TEXT("已保存装饰Blueprint真实加载"), VisualClass.Get()) ||
            !Test.TestNotNull(TEXT("已保存中文UIFont真实加载"), LabelFont.Get()) ||
            !Test.TestTrue(TEXT("TextRender使用真实离线字形字体"), LabelFont->FontCacheType == EFontCacheType::Offline)) return false;
        bool bGlyphsValid = true;
        // RemapChar验证不是缺字回退；GetCharSize查询实际UIFont数据，不替代真实屏幕文字验收。
        for (const TCHAR Character : FString(TEXT("远古石灵生命已击败等待重生")))
        {
            const TCHAR Mapped = LabelFont->RemapChar(Character);
            float Width = 0.0f;
            float Height = 0.0f;
            LabelFont->GetCharSize(Character, Width, Height);
            bGlyphsValid &= Test.TestTrue(FString::Printf(TEXT("中文字符U+%04X有真实映射及尺寸"), static_cast<uint32>(Character)),
                Mapped != UFont::NULLCHARACTER && LabelFont->Characters.IsValidIndex(static_cast<int32>(Mapped)) && Width > 0.0f && Height > 0.0f);
        }
        if (!bGlyphsValid) return false;
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("真实瞬态Game世界"), World.Get())) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
        bWorldContextCreated = true;
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Monster = World->SpawnActor<ADivineBeastsDevelopmentMonster>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("真实开发怪物"), Monster)) return false;
        Monster->DispatchBeginPlay();
        if (!Test.TestTrue(TEXT("真实唯一Combat生命初始化"), Monster->AuthorityInitializeDevelopmentMonster())) return false;
        Source = World->SpawnActor<AActor>();
        if (!Test.TestNotNull(TEXT("独立真实伤害来源"), Source)) return false;
        auto* SourceASC = NewObject<UGamePlatformAbilitySystemComponent>(Source);
        SourceCombat = NewObject<UGamePlatformCombatComponent>(Source);
        Source->AddInstanceComponent(SourceASC);
        Source->AddInstanceComponent(SourceCombat);
        SourceASC->RegisterComponent();
        SourceCombat->RegisterComponent();
        if (!Test.TestTrue(TEXT("来源ASC真实绑定"), SourceASC->BindAbilityActorInfo(Source, Source))) return false;
        Source->DispatchBeginPlay();
        if (!Test.TestTrue(TEXT("来源Combat真实进入BeginPlay"), SourceCombat->HasBegunPlay() && SourceCombat->GetCombatAttributeSet())) return false;
        Service.Reset(NewObject<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem>(World.Get()));
        Service->LifetimeToken = FGuid::NewGuid();
        // 无GameInstance/Data测试替身；不调用生产AcquireResources，不制造可释放的假Lease。
        Service->bReady = false;
        Service->ObserveActor(Monster);
        return Test.TestEqual(TEXT("资源就绪前尚无装饰Actor"), CountOwnedDecorations(), 0);
    }

    /** 仅驱动本测试的就绪边界；资源已经由真实Load持有，生产仍须经唯一Data服务预热。 */
    void Display()
    {
        Service->bReady = true;
        Service->ApplyLife(Monster, Monster->GetLifeState());
    }

    /** 历史真实快照重投；不写Monster/ASC，不伪造新生命或CombatResult。 */
    void DeliverHistorical(const FDivineBeastsDevelopmentMonsterLifeState& State)
    {
        Service->ApplyLife(Monster, State);
    }

    /** 正代次真实Combat结算；唯一Formula属于平台，夹具只提供确定的输入量。 */
    FGamePlatformCombatResult Damage(float Magnitude) const
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid();
        Spec.Source = Source;
        Spec.Target = Monster;
        Spec.Magnitude = Magnitude;
        Spec.DamageType = EGamePlatformDamageType::Magic;
        Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
        Spec.TargetAvatarGeneration = Monster->GetCombatAvatarGeneration();
        return SourceCombat->ApplyDamage(Spec);
    }

    /** 检查世界中所有自有装饰而不只看Map，能发现同步Spawn丢失引用的第二个Actor。 */
    int32 CountOwnedDecorations() const
    {
        int32 Count = 0;
        if (!World.IsValid() || !VisualClass.IsValid()) return Count;
        for (TActorIterator<AActor> It(World.Get()); It; ++It)
            if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->IsA(VisualClass.Get()) && It->GetOwner() == Monster) ++Count;
        return Count;
    }

    /** 只读当前装配记录；没有Actor必须由用例显式失败，不能返回默认标签冒充装配。 */
    AActor* GetDecoration() const
    {
        const auto* Visual = Service->Visuals.Find(Monster);
        return Visual ? Visual->Actor.Get() : nullptr;
    }

    /** 从真实运行时Blueprint组件查找中立槽，测试本身不SetFont或SetText。 */
    UTextRenderComponent* GetLabel(const TCHAR* Tag) const
    {
        AActor* Decoration = GetDecoration();
        if (!IsValid(Decoration)) return nullptr;
        TArray<UTextRenderComponent*> Labels;
        Decoration->GetComponents(Labels);
        for (auto* Label : Labels) if (Label && Label->ComponentHasTag(FName(Tag))) return Label;
        return nullptr;
    }

    float GetDisplayedHealth() const
    {
        const auto* Visual = Service->Visuals.Find(Monster);
        return Visual ? Visual->PreviousHealth : -1.0f;
    }

    int32 GetObservationCount() const { return Service->Visuals.Num(); }

    /** 原生TimerManager无公开计数器；捕获其真实ListTimers总数，含尚未Tick的Pending Timer。
     * 只读诊断而非私有Timer容器/假句柄；缺日志就报前置失败，不能以0假冒无泄漏。
     */
    int32 CountWorldTimers() const
    {
        struct FTimerLogCapture final : FOutputDevice
        {
            int32 Total = -1;
            virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
            {
                const FString Line(Message);
                const int32 Marker = Line.Find(TEXT(" Total Timers -------"));
                if (Marker == INDEX_NONE) return;
                const int32 Start = Line.Find(TEXT("------- "));
                if (Start != INDEX_NONE) LexTryParseString(Total, *Line.Mid(Start + 8, Marker - Start - 8));
            }
        } Capture;
        if (!GLog || !World.IsValid()) return -1;
        // UE5.8由专用日志线程分发；设备添加会自动重放旧日志，必须同步排空后重置本次采样。
        // ListTimers结束后同样等待真实日志送达，不能在异步发送前移除设备并把缺日志记成0。
        GLog->FlushThreadedLogs();
        GLog->AddOutputDevice(&Capture);
        GLog->FlushThreadedLogs();
        Capture.Total = -1;
        World->GetTimerManager().ListTimers();
        GLog->FlushThreadedLogs();
        GLog->RemoveOutputDevice(&Capture);
        return Capture.Total;
    }

    /** 先去本测试回调再结束装配；清理漏Actor/Timer仅在断言之后，不能掩盖被测泄漏。 */
    ~FDivineBeastsDevelopmentMonsterVisualTestFixture()
    {
        if (!World.IsValid()) return;
        World->RemoveOnActorSpawnedHandler(TestSpawnHandle);
        if (TransformSource.IsValid()) TransformSource->TransformUpdated.Remove(TestTransformHandle);
        if (Service.IsValid())
        {
            Service->Deinitialize();
            World->GetTimerManager().ClearAllTimersForObject(Service.Get());
        }
        TArray<AActor*> Leaked;
        for (TActorIterator<AActor> It(World.Get()); It; ++It)
            if (VisualClass.IsValid() && It->IsA(VisualClass.Get()) && It->GetOwner() == Monster) Leaked.Add(*It);
        for (auto* Actor : Leaked) if (IsValid(Actor)) Actor->Destroy();
        if (IsValid(Monster)) Monster->Destroy();
        if (IsValid(Source)) Source->Destroy();
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        if (bWorldContextCreated && GEngine) GEngine->DestroyWorldContext(World.Get());
    }
};

// 真UIFont中文字形有效仍不足以显示：服务必须把同一Font绑定两个真实槽，并从NPC事件组合名称/当前生命。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterVisualFontProjectionTest,
    "DivineBeasts.Presentation.DevelopmentMonster.Regression.RealFontNameAndLifeProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterVisualFontProjectionTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentMonsterVisualTestFixture Fixture;
    if (!Fixture.Create(*this)) return false;
    Fixture.Display();
    auto* NameLabel = Fixture.GetLabel(TEXT("DBA.Monster.NameLabel"));
    auto* LifeLabel = Fixture.GetLabel(TEXT("DBA.Monster.LifeLabel"));
    if (!TestNotNull(TEXT("真实BP名称槽"), NameLabel) || !TestNotNull(TEXT("真实BP生命槽"), LifeLabel)) return false;
    TestTrue(TEXT("名称槽绑定实际UIFont对象"), NameLabel->Font.Get() == Fixture.LabelFont.Get());
    TestTrue(TEXT("生命槽绑定同一实际UIFont对象"), LifeLabel->Font.Get() == Fixture.LabelFont.Get());
    TestEqual(TEXT("怪物公开名称为中文且无额外JSON引号"), Fixture.Monster->GetMonsterDisplayName().ToString(), FString(TEXT("远古石灵")));
    TestEqual(TEXT("中立名称槽由怪物只读名称装配"), NameLabel->Text.ToString(), Fixture.Monster->GetMonsterDisplayName().ToString());
    TestEqual(TEXT("初始生命完整投影"), LifeLabel->Text.ToString(), FString(TEXT("生命 300 / 300")));
    TestTrue(TEXT("受伤来自真实Combat.ApplyDamage"), Fixture.Damage(30.0f).IsSuccess());
    TestEqual(TEXT("实际权威生命为270"), Fixture.Monster->GetLifeState().Health, 270.0f);
    TestEqual(TEXT("事件实时显示当前生命"), LifeLabel->Text.ToString(), FString(TEXT("生命 270 / 300")));
    return true;
}

// 两次真实伤害产生同代次历史快照；旧通知不能把已显示260尾写回270，尤其不能只用代次排旧。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterVisualOldSameGenerationTest,
    "DivineBeasts.Presentation.DevelopmentMonster.Regression.MainVisualOldSameGen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterVisualOldSameGenerationTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentMonsterVisualTestFixture Fixture;
    if (!Fixture.Create(*this)) return false;
    Fixture.Display();
    if (!TestTrue(TEXT("第一条真实伤害"), Fixture.Damage(30.0f).IsSuccess())) return false;
    const auto Older = Fixture.Monster->GetLifeState();
    if (!TestTrue(TEXT("第二条真实伤害"), Fixture.Damage(10.0f).IsSuccess())) return false;
    const auto Current = Fixture.Monster->GetLifeState();
    TestEqual(TEXT("输入确为同一生命代次"), Current.AvatarGeneration, Older.AvatarGeneration);
    TestEqual(TEXT("后继生命为260"), Current.Health, 260.0f);
    Fixture.DeliverHistorical(Older);
    auto* LifeLabel = Fixture.GetLabel(TEXT("DBA.Monster.LifeLabel"));
    if (!TestNotNull(TEXT("历史通知后仍有同一生命槽"), LifeLabel)) return false;
    TestEqual(TEXT("历史通知不能倒写当前文案"), LifeLabel->Text.ToString(), FString(TEXT("生命 260 / 300")));
    TestEqual(TEXT("历史通知不能倒写显示采样"), Fixture.GetDisplayedHealth(), Current.Health);
    TestEqual(TEXT("客户端适配不写权威生命"), Fixture.Monster->GetLifeState().Health, Current.Health);
    return true;
}

// 原生SpawnActor通知同步提交真实伤害：创建栅栏须阻止二次Spawn，回栈须重新采样，而非回写Spawn前300。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterVisualSpawnDamageReentryTest,
    "DivineBeasts.Presentation.DevelopmentMonster.Regression.ActorSpawnRealDamageNoLeak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterVisualSpawnDamageReentryTest::RunTest(const FString&)
{
    FDivineBeastsDevelopmentMonsterVisualTestFixture Fixture;
    if (!Fixture.Create(*this)) return false;
    bool bDamageEntered = false;
    Fixture.TestSpawnHandle = Fixture.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
        [this, &Fixture, &bDamageEntered](AActor* Actor)
        {
            if (bDamageEntered || !Actor || !Actor->IsA(Fixture.VisualClass.Get()) || Actor->GetOwner() != Fixture.Monster) return;
            bDamageEntered = true;
            TestTrue(TEXT("真实SpawnActor同步回调中的Combat伤害成功"), Fixture.Damage(30.0f).IsSuccess());
        }));
    Fixture.Display();
    Fixture.World->RemoveOnActorSpawnedHandler(Fixture.TestSpawnHandle);
    Fixture.TestSpawnHandle.Reset();
    TestTrue(TEXT("真实装饰Spawn已触发同步伤害"), bDamageEntered);
    TestEqual(TEXT("同步重入只生成一个自有装饰"), Fixture.CountOwnedDecorations(), 1);
    auto* LifeLabel = Fixture.GetLabel(TEXT("DBA.Monster.LifeLabel"));
    if (!TestNotNull(TEXT("同步Spawn后装配生命槽"), LifeLabel)) return false;
    TestEqual(TEXT("Spawn回栈按当前生命显示"), LifeLabel->Text.ToString(), FString(TEXT("生命 270 / 300")));
    Fixture.Service->Deinitialize();
    TestEqual(TEXT("退出不遗留丢失引用的第二装饰"), Fixture.CountOwnedDecorations(), 0);
    return true;
}

namespace DivineBeastsMonsterVisualTransformTests
{
/** 旧实现可能在Map移除后使用悬空FVisual而崩溃，故独立CrashRegression前缀供主端隔离进程执行。
 * 回调来自真实SceneComponent.TransformUpdated，而非手工广播/伪造伤害；断言前绝不清理漏Timer。
 */
bool Run(FAutomationTestBase& Test, bool bDestroyMonster)
{
    FDivineBeastsDevelopmentMonsterVisualTestFixture Fixture;
    if (!Fixture.Create(Test)) return false;
    Fixture.Display();
    AActor* Decoration = Fixture.GetDecoration();
    if (!Test.TestNotNull(TEXT("真实已装配Actor"), Decoration) ||
        !Test.TestNotNull(TEXT("实际TransformUpdated来源"), Decoration->GetRootComponent())) return false;
    Fixture.TransformSource.Reset(Decoration->GetRootComponent());
    const int32 BeforeTimers = Fixture.CountWorldTimers();
    if (!Test.TestTrue(TEXT("原生TimerManager总数日志可读取"), BeforeTimers >= 0)) return false;
    bool bEntered = false;
    Fixture.TestTransformHandle = Fixture.TransformSource->TransformUpdated.AddLambda(
        [&Fixture, &bEntered, bDestroyMonster](USceneComponent*, EUpdateTransformFlags, ETeleportType)
        {
            if (bEntered) return;
            bEntered = true;
            if (bDestroyMonster) Fixture.Monster->Destroy();
            else Fixture.Service->Deinitialize();
        });
    const auto Result = Fixture.Damage(10.0f);
    Fixture.TransformSource->TransformUpdated.Remove(Fixture.TestTransformHandle);
    Fixture.TestTransformHandle.Reset();
    Test.TestTrue(TEXT("受击经过真实Combat，退出不伪造其结果"), Result.IsSuccess());
    Test.TestTrue(TEXT("原生TransformUpdated确实同步进入退出"), bEntered);
    Test.TestEqual(TEXT("退出回栈不恢复观察记录"), Fixture.GetObservationCount(), 0);
    Test.TestEqual(TEXT("退出回栈不遗留装饰Actor"), Fixture.CountOwnedDecorations(), 0);
    const int32 AfterTimers = Fixture.CountWorldTimers();
    Test.TestTrue(TEXT("退出后真实TimerManager总数日志可读取"), AfterTimers >= 0);
    Test.TestEqual(TEXT("Transform同步退出后不新建或遗留受击Timer"), AfterTimers, BeforeTimers);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterVisualTransformDeinitializeTest,
    "DivineBeasts.Presentation.DevelopmentMonster.CrashRegression.TransformUpdatedDeinitializeNoTimer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterVisualTransformDeinitializeTest::RunTest(const FString&)
{
    return DivineBeastsMonsterVisualTransformTests::Run(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterVisualTransformDestroyTest,
    "DivineBeasts.Presentation.DevelopmentMonster.CrashRegression.TransformUpdatedDestroyNoTimer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterVisualTransformDestroyTest::RunTest(const FString&)
{
    return DivineBeastsMonsterVisualTransformTests::Run(*this, true);
}

#endif
