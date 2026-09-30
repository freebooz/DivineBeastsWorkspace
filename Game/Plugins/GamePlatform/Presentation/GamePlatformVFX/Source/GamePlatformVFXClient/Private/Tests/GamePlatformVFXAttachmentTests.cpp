// F09：延迟请求只能保存弱目标；目标回收和跨世界目标在执行前必须拒绝。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Definitions/GamePlatformVFXAttachedDefinition.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXAttachmentTest, "GamePlatform.VFX.Attachment.WeakAndWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXAttachmentTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> First(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UWorld> Second(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UGamePlatformVFXAttachedDefinition> Definition(NewObject<UGamePlatformVFXAttachedDefinition>());
    FGamePlatformVFXSpawnContext Spawn;
    auto* Actor=First->SpawnActor<AActor>();
    auto* Component=NewObject<USceneComponent>(Actor); Component->RegisterComponent();
    Spawn.AttachComponent=Component;
    TestTrue(TEXT("本世界活目标允许附着"), FGamePlatformVFXNiagaraExecutor::IsAttachmentValid(*First, *Definition, Spawn));
    TestFalse(TEXT("跨世界组件必须拒绝"), FGamePlatformVFXNiagaraExecutor::IsAttachmentValid(*Second, *Definition, Spawn));
    Component->DestroyComponent();
    Actor->Destroy();
    CollectGarbage(RF_NoFlags);
    TestFalse(TEXT("保存的延迟上下文不会保活已销毁组件"), Spawn.AttachComponent.IsValid());
    TestFalse(TEXT("回收后附着失败，不能落地播放"), FGamePlatformVFXNiagaraExecutor::IsAttachmentValid(*First, *Definition, Spawn));
    First->DestroyWorld(false); Second->DestroyWorld(false);
    return true;
}
#endif
