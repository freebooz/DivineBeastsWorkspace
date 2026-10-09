// F12：失活只结束显示状态，资源/Travel所有权必须等真实栈移除；重复ID不能覆盖旧通知。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Services/GamePlatformNotificationService.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Components/Overlay.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Engine/LocalPlayer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIScreenLifecycleRegressionTest, "GamePlatform.UI.Lifecycle.SuspensionKeepsOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIScreenLifecycleRegressionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract; // 仅此测试分配，无Blueprint文件。
    auto* Player=NewObject<ULocalPlayer>(); auto* Manager=NewObject<UGamePlatformUIManagerSubsystem>(Player);
    auto* A=NewObject<UGamePlatformUIScreen>(); auto* Stack=NewObject<UCommonActivatableWidgetStack>();
    Manager->ScreenStacks.Add(A,Stack); Manager->ActiveScreenLeases.Add(A,FGamePlatformDataLease());
    Manager->TravelPersistentScreens.Add(A); Manager->PauseScreens.Add(A);
    Manager->HandleScreenDeactivated(A);
    TestTrue(TEXT("A被B覆盖仍持资源"), Manager->ActiveScreenLeases.Contains(A));
    TestTrue(TEXT("A被B覆盖仍记Travel策略"), Manager->TravelPersistentScreens.Contains(A));
    TestFalse(TEXT("失活A不继续申请暂停"), Manager->PauseScreens.Contains(A));
    Manager->RemoveScreenOwnership(A);
    TestFalse(TEXT("最终移除A释放资源账本"), Manager->ActiveScreenLeases.Contains(A));
    TestFalse(TEXT("最终移除A释放Travel账本"), Manager->TravelPersistentScreens.Contains(A));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUINotificationDuplicateTest, "GamePlatform.UI.Notification.DuplicateIdKeepsOldWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUINotificationDuplicateTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    auto* Root=NewObject<UGamePlatformUILayerStack>(); Root->NotificationLayer=NewObject<UOverlay>(Root);
    auto* Service=NewObject<UGamePlatformNotificationService>(); Service->SetRootLayout(Root);
    auto* Old=NewObject<UGamePlatformNotificationWidget>(); auto* New=NewObject<UGamePlatformNotificationWidget>();
    FGamePlatformUINotificationRequest Request; Request.RequestId=FGuid::NewGuid(); Request.Channel=TEXT("Test"); Request.LifetimeSeconds=0;
    TestTrue(TEXT("首次挂载允许"), Service->AttachNotificationWidget(Old,Request));
    TestFalse(TEXT("重复ID拒绝"), Service->AttachNotificationWidget(New,Request));
    TestEqual(TEXT("旧条目仍指原Widget"), Service->ActiveNotifications.FindChecked(Request.RequestId).Widget.Get(),Old);
    TestEqual(TEXT("层中没有孤儿通知"), Root->NotificationLayer->GetChildrenCount(),1);
    Service->Clear(); TestEqual(TEXT("Clear移除全部自有通知"), Root->NotificationLayer->GetChildrenCount(),0);
    return true;
}

/** F12：真实CommonUI在B入栈时同步失活A；A的用户事件取消B，返回后不得提交B。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIScreenReentryRegressionTest,
    "GamePlatform.UI.Lifecycle.SynchronousCancelWithdrawsNewWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIScreenReentryRegressionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>());
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Manager(NewObject<UGamePlatformUIManagerSubsystem>(Player.Get()));
    const TStrongObjectPtr<UGamePlatformUILayerStack> Root(NewObject<UGamePlatformUILayerStack>());
    const TStrongObjectPtr<UCommonActivatableWidgetStack> Stack(NewObject<UCommonActivatableWidgetStack>());
    const TStrongObjectPtr<UGamePlatformUIScreen> A(NewObject<UGamePlatformUIScreen>());
    const TStrongObjectPtr<UGamePlatformUIScreen> B(NewObject<UGamePlatformUIScreen>());
    const TStrongObjectPtr<UGamePlatformUIScreenDefinition> Definition(NewObject<UGamePlatformUIScreenDefinition>());
    Definition->ScreenId = TEXT("ReentrantScreen");
    // 仅Transient测试夹具设置可选BindWidget字段，不生成或修改Widget Blueprint资产。
    auto* LayerProperty = FindFProperty<FObjectPropertyBase>(Root->GetClass(), TEXT("ScreenLayer"));
    if (!TestNotNull(TEXT("真实Root层字段存在"), LayerProperty)) return false;
    LayerProperty->SetObjectPropertyValue_InContainer(Root.Get(), Stack.Get());
    Manager->RootLayout = Root.Get();
    Manager->ScreenDefinitions.Add(Definition->ScreenId, Definition.Get());
    FGamePlatformUIAsyncRequest Request;
    Request.RequestId = FGuid::NewGuid(); Request.ScreenId = Definition->ScreenId; Request.Generation = 41;
    FGamePlatformDataLease Lease; Lease.LeaseId = FGuid::NewGuid();
    Manager->PendingRequests.Add(Request.RequestId, Request);
    Manager->PendingLoads.Add(Request.RequestId, Lease);
    // 回退中的请求同样会经历真实CommonUI同步重入；取消必须撤销重试资格且暂留构造租约。
    Manager->PendingDefaultWidgetRetries.Add(Request.RequestId);
    Manager->ConstructingScreenRequests.Add(Request.RequestId);
    UGamePlatformUIManagerSubsystem::FScreenOpenConstruction Construction;
    Construction.Request = Request; Construction.Lease = Lease;
    Construction.Root = Root.Get(); Construction.Stack = Stack.Get(); Construction.Definition = Definition.Get();
    Construction.LayoutGeneration = Manager->RootLayoutGeneration;
    Stack->SetTransitionDuration(0.0f);
    const TSharedRef<SWidget> SlateStack = Stack->TakeWidget();
    Stack->AddWidgetInstance(*A);
    int32 CancelEvents = 0;
    const FDelegateHandle CancelHandle = A->OnDeactivated().AddLambda([&]
    {
        ++CancelEvents;
        Manager->CancelOpen(Request.RequestId);
    });
    Stack->AddWidgetInstance(*B); // 真实容器激活B/失活A的同步调用边界。
    TestEqual(TEXT("A失活用户事件在调用返回前取消B"), CancelEvents, 1);
    TestFalse(TEXT("请求资格已取消"), Manager->PendingRequests.Contains(Request.RequestId));
    TestFalse(TEXT("同步取消清除默认类回退资格"), Manager->PendingDefaultWidgetRetries.Contains(Request.RequestId));
    TestTrue(TEXT("构造返回前仍保留Pending租约"), Manager->PendingLoads.Contains(Request.RequestId));
    TestFalse(TEXT("取消的B不得完成提交"), Manager->CompleteScreenOpen(Construction, B.Get()));
    TestFalse(TEXT("原栈撤回B"), Stack->GetWidgetList().Contains(B.Get()));
    TestFalse(TEXT("原租约账本释放"), Manager->PendingLoads.Contains(Request.RequestId));
    TestFalse(TEXT("B没有Active租约"), Manager->ActiveScreenLeases.Contains(B.Get()));
    TestFalse(TEXT("B没有栈所有权"), Manager->ScreenStacks.Contains(B.Get()));
    TestFalse(TEXT("构造守卫结束"), Manager->ConstructingScreenRequests.Contains(Request.RequestId));
    A->OnDeactivated().Remove(CancelHandle);
    Stack->RemoveWidget(*A);
    (void)SlateStack;
    return true;
}
#endif
