// 本回归隔离验证已准备作用域的来源查询与真实Touch签发；显式搭建准备前提，不冒充Data加载或实机验收。
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Misc/ScopeExit.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformInputSemanticOwnershipTest,
    "GamePlatform.Input.SemanticOwnership.MappedAndTouch", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformInputSemanticOwnershipTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); auto* World=Instance->GetWorld();
    ON_SCOPE_EXIT {Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
    World->InitializeActorsForPlay(FURL());
    TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    Viewport->Init(GEngine->GetWorldContextFromWorldChecked(World),Instance.Get(),false);
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));Player->ViewportClient=Viewport.Get();
    ON_SCOPE_EXIT {Player->ViewportClient=nullptr;};
    auto* Controller=World->SpawnActor<APlayerController>();Controller->SetPlayer(Player.Get());
    TStrongObjectPtr<UGamePlatformInputLocalPlayerSubsystem> Input(NewObject<UGamePlatformInputLocalPlayerSubsystem>(Player.Get()));
    Input->Scope=MakeUnique<FGamePlatformInputScope>();
    ON_SCOPE_EXIT {Input->Deinitialize();};
    TStrongObjectPtr<UGamePlatformInputProfileDefinition> Profile(NewObject<UGamePlatformInputProfileDefinition>(Player.Get()));
    Input->Scope->Profile=Profile.Get();Input->Scope->bProfilePrepared=true;
    const auto Move=GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move);
    const auto Look=GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookDelta);
    TStrongObjectPtr<UInputAction> MoveAction(NewObject<UInputAction>(Player.Get()));MoveAction->ValueType=EInputActionValueType::Axis2D;
    TStrongObjectPtr<UInputAction> LookAction(NewObject<UInputAction>(Player.Get()));LookAction->ValueType=EInputActionValueType::Axis2D;
    for (const auto& Pair : {TPair<FGameplayTag,UInputAction*>(Move,MoveAction.Get()),TPair<FGameplayTag,UInputAction*>(Look,LookAction.Get())})
    {
        FGamePlatformCompiledInputAction Action;Action.Slot=Input->Scope->CompiledActions.Num();Action.SemanticId.Tag=Pair.Key;
        Action.Action=Pair.Value;Action.ValueType=EInputActionValueType::Axis2D;
        Input->Scope->SlotByTag.Add(Pair.Key,Action.Slot);Input->Scope->CompiledActions.Add(Action);
    }
    TStrongObjectPtr<UEnhancedInputComponent> Receiver(NewObject<UEnhancedInputComponent>(Controller));
    FGamePlatformResult Result;const auto Binding=Input->BindInputReceiver(*Receiver,Result);
    if(!TestTrue(*FString::Printf(TEXT("真实原生Receiver签发成功 (%s)"),*Result.Code.ToString()),Result.IsSuccess()&&Binding.IsValid()))return false;
    TStrongObjectPtr<UInputMappingContext> Context(NewObject<UInputMappingContext>(Player.Get()));Context->MapKey(MoveAction.Get(),EKeys::W);
    Input->Scope->Contexts.Add(TEXT("Fixture"),Context.Get());
    FInputContextRecord ContextRecord;ContextRecord.Owner=Player.Get();ContextRecord.ContextName=TEXT("Fixture");
    const FGuid ContextId=FGuid::NewGuid();Input->Scope->ContextLeases.Add(ContextId,ContextRecord);
    ON_SCOPE_EXIT {Input->Scope->ContextLeases.Remove(ContextId);};
    TestTrue(TEXT("活跃映射和真实Receiver共同拥有Move"),Input->GetSemanticInputOwnership(Move).bMappedReceiver);
    TestFalse(TEXT("Profile有Look但没有活跃映射不得关闭原生Look"),Input->GetSemanticInputOwnership(Look).IsOwned());
    FGamePlatformInputSemanticId LookId;LookId.Tag=Look;
    const auto Touch=Input->BeginTouchInputBySemantic(0,LookId,Player.Get(),Result);
    if(!TestTrue(TEXT("真实平台BeginTouchInputBySemantic成功"),Result.IsSuccess()&&Touch.IsValid()))return false;
    TestTrue(TEXT("真实Touch来源独立于最近设备提示"),Input->GetSemanticInputOwnership(Look).bTouchReceiver);
    Input->EndTouchInput(Touch);
    TestFalse(TEXT("Touch结束后不能继续绕过鼠标拖动"),Input->GetSemanticInputOwnership(Look).IsOwned());
    Input->UnbindInputReceiver(Binding);
    TestFalse(TEXT("没有当前Receiver时不得接管原生Move"),Input->GetSemanticInputOwnership(Move).IsOwned());
    return true;
}
#endif
