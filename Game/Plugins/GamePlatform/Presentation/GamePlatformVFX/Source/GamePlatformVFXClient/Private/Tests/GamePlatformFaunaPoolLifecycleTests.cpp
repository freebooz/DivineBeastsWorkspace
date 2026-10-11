// 平台真实UE自动化：生产Registry停止未激活的Niagara原生池租出组件，验证归还、复用和销毁。
// 夹具使用已编译的平台基础系统与真实预览World；不复制Registry、不伪造组件或池，不代表GPU运动验收。
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformFaunaPoolLifecycleTest,
    "GamePlatform.VFX.Fauna.NativePoolPreActivationLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGamePlatformFaunaPoolLifecycleTest::RunTest(const FString&)
{
    check(IsInGameThread());
    // 测试夹具显式加载真实已编译资产；不经项目物种目录，也不改变生产资源加载机制。
    const TStrongObjectPtr<UNiagaraSystem> System(LoadObject<UNiagaraSystem>(nullptr,
        TEXT("/GamePlatformVFX/Environment/Fauna/Niagara/NS_GP_Fauna_Base.NS_GP_Fauna_Base")));
    if (!TestNotNull(TEXT("平台Fauna基础系统必须实际存在"), System.Get())) return false;
    System->WaitForCompilationComplete(true, false);
    if (!TestTrue(TEXT("真实系统必须含有效且可运行的编译数据"), System->IsValid() && System->IsReadyToRun()) ||
        !TestTrue(TEXT("基础系统必须允许至少一个原生池空闲组件"), System->MaxPoolSize > 0)) return false;

    IConsoleVariable* PoolEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("FX.NiagaraComponentPool.Enable"));
    if (!TestNotNull(TEXT("真实Niagara池开关必须存在"), PoolEnabled)) return false;
    const int32 PreviousPoolEnabled = PoolEnabled->GetInt();

    // 真实FX预览World避免启动项目Gameplay/LocalPlayer；原生World初始化委托创建真实Niagara管理器。
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(true);
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(
        EWorldType::EditorPreview, false, NAME_None, nullptr, false, ERHIFeatureLevel::Num, &Init));
    FGamePlatformVFXInstanceRegistry Registry;
    ON_SCOPE_EXIT
    {
        Registry.Reset();
        if (World.Get()) World->DestroyWorld(false);
        PoolEnabled->SetWithCurrentPriority(PreviousPoolEnabled);
    };
    if (!TestNotNull(TEXT("真实测试World必须创建成功"), World.Get())) return false;
    FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World.Get());
    if (!TestNotNull(TEXT("World初始化必须创建真实Niagara管理器"), WorldManager) ||
        !TestNotNull(TEXT("真实世界原生池必须存在"), WorldManager->GetComponentPool())) return false;
    PoolEnabled->SetWithCurrentPriority(1);
    if (!TestTrue(TEXT("真实原生池开关已开启"), UNiagaraComponentPool::Enabled())) return false;

    const auto SpawnUnactivated = [&](ENCPoolMethod PoolMethod)
    {
        return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World.Get(), System.Get(), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector,
            true, false, PoolMethod, false);
    };

    UNiagaraComponent* First = SpawnUnactivated(ENCPoolMethod::AutoRelease);
    if (!TestNotNull(TEXT("必须实际租出Niagara组件"), First)) return false;
    const auto FirstHandle = Registry.Reserve(World.Get());
    if (!TestTrue(TEXT("实际组件绑定生产Registry"), Registry.AttachComponent(FirstHandle, First, true))) return false;
    TestFalse(TEXT("Spawn禁止自动激活"), First->IsActive());
    TestEqual(TEXT("已租出但未激活的真实池方式"), First->PoolingMethod, ENCPoolMethod::AutoRelease);
    TestTrue(TEXT("生产Stop接受激活前失败清理"), Registry.Stop(FirstHandle));
    if (!TestTrue(TEXT("归池后组件仍有效"), IsValid(First))) return false;
    if (!TestEqual(TEXT("未激活Stop必须实际归入FreeInPool"), First->PoolingMethod, ENCPoolMethod::FreeInPool)) return false;
    TestFalse(TEXT("旧Handle已从生产Registry移除"), Registry.OwnsHandle(FirstHandle));
    TestFalse(TEXT("旧Component反向索引已清理"), Registry.FindByComponent(First).IsValid());
    TestFalse(TEXT("同Handle再次Stop不得重复归池"), Registry.Stop(FirstHandle));

    UNiagaraComponent* Reused = SpawnUnactivated(ENCPoolMethod::AutoRelease);
    if (!TestNotNull(TEXT("原生池可再次租出组件"), Reused)) return false;
    if (!TestTrue(TEXT("真实原生池复用同一个组件对象"), Reused == First)) return false;
    const auto ReusedHandle = Registry.Reserve(World.Get());
    if (!TestTrue(TEXT("复用组件获得新的生产Handle"), Registry.AttachComponent(ReusedHandle, Reused, true))) return false;
    TestFalse(TEXT("旧Handle不能停止已复用组件"), Registry.Stop(FirstHandle));
    TestEqual(TEXT("旧Stop不删除新Owner反向索引"), Registry.FindByComponent(Reused), ReusedHandle);
    TestEqual(TEXT("旧Stop不把新租约重新归池"), Reused->PoolingMethod, ENCPoolMethod::AutoRelease);

    // 若重复归还产生两个空闲条目，再租第二份时会错误得到当前仍租出的同一个对象。
    UNiagaraComponent* Concurrent = SpawnUnactivated(ENCPoolMethod::AutoRelease);
    if (!TestNotNull(TEXT("可同时租出另一组件"), Concurrent)) return false;
    if (!TestTrue(TEXT("原生池没有同一对象的重复空闲条目"), Concurrent != Reused)) return false;
    const auto ConcurrentHandle = Registry.Reserve(World.Get());
    if (!TestTrue(TEXT("并存组件绑定生产Registry"), Registry.AttachComponent(ConcurrentHandle, Concurrent, true))) return false;
    AddInfo(FString::Printf(TEXT("真实原生池复用对象=%s，并存对象=%s"), *Reused->GetPathName(), *Concurrent->GetPathName()));
    TestTrue(TEXT("复用句柄正常停止"), Registry.Stop(ReusedHandle));
    TestTrue(TEXT("并存句柄正常停止"), Registry.Stop(ConcurrentHandle));
    TestEqual(TEXT("停止两份租约后生产账本为空"), Registry.Num(), 0);

    PoolEnabled->SetWithCurrentPriority(0);
    if (!TestFalse(TEXT("真实全局池开关已关闭"), UNiagaraComponentPool::Enabled())) return false;
    for (const ENCPoolMethod Mode : {ENCPoolMethod::AutoRelease, ENCPoolMethod::None})
    {
        UNiagaraComponent* Unpooled = SpawnUnactivated(Mode);
        if (!TestNotNull(TEXT("无池情况下仍创建真实组件"), Unpooled)) return false;
        const TWeakObjectPtr<UNiagaraComponent> UnpooledWeak(Unpooled);
        const auto UnpooledHandle = Registry.Reserve(World.Get());
        // 故意保留请求时bPooled=true，证明销毁以真实组件/原生池状态为准。
        if (!TestTrue(TEXT("无池组件绑定生产Registry"), Registry.AttachComponent(UnpooledHandle, Unpooled, true))) return false;
        TestFalse(TEXT("无池组件同样尚未激活"), Unpooled->IsActive());
        if (Mode == ENCPoolMethod::None)
            TestEqual(TEXT("显式None组件保留真实非池方式"), Unpooled->PoolingMethod, ENCPoolMethod::None);
        TestTrue(TEXT("无池Stop正常结束"), Registry.Stop(UnpooledHandle));
        TestFalse(TEXT("无池Stop必须销毁对象，不能只置为不活跃"), UnpooledWeak.IsValid());
        TestFalse(TEXT("销毁后的重复Stop不再处理组件"), Registry.Stop(UnpooledHandle));
    }
    TestEqual(TEXT("所有终态后生产Registry为空"), Registry.Num(), 0);
    return true;
}

#endif
