#if WITH_EDITOR
#include "Editor/GamePlatformWeatherWindPreview.h"
#include "API/GamePlatformWeatherWindSource.h"
#include "API/GamePlatformSurfaceWindPreview.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/WindDirectionalSource.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

void FGamePlatformWeatherWindPreview::Start()
{
    MapHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FGamePlatformWeatherWindPreview::MapOpened);
    PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FGamePlatformWeatherWindPreview::PropertyChanged);
    DeleteHandle = FEditorDelegates::OnDeleteActorsEnd.AddRaw(this, &FGamePlatformWeatherWindPreview::Refresh);
    UndoHandle = FEditorDelegates::PostUndoRedo.AddRaw(this, &FGamePlatformWeatherWindPreview::Refresh);
    if (GEngine) MoveHandle = GEngine->OnActorMoved().AddRaw(this, &FGamePlatformWeatherWindPreview::ActorMoved);
    Refresh();
}
void FGamePlatformWeatherWindPreview::Stop()
{
    FEditorDelegates::OnMapOpened.Remove(MapHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
    FEditorDelegates::OnDeleteActorsEnd.Remove(DeleteHandle);
    FEditorDelegates::PostUndoRedo.Remove(UndoHandle);
    if (GEngine) GEngine->OnActorMoved().Remove(MoveHandle);
    Restore();
}
void FGamePlatformWeatherWindPreview::Restore()
{
    if (auto* Root = PreviewRoot.Get()) Root->TransformUpdated.Remove(RootTransformHandle);
    PreviewRoot.Reset(); RootTransformHandle.Reset();
    if (auto* World = PreviewWorld.Get(); IsValid(World) && !World->bIsTearingDown)
    { FString Error; GamePlatformSurfaceWindPreview::Apply(*World, Before, Error); }
    PreviewWorld.Reset();
}
void FGamePlatformWeatherWindPreview::MapOpened(const FString&, bool) { Refresh(); }
void FGamePlatformWeatherWindPreview::ActorMoved(AActor* Actor)
{
    if (IsValid(Actor) && Actor->IsA<AWindDirectionalSource>() && IsValid(Actor->GetWorld()) &&
        Actor->GetWorld()->WorldType == EWorldType::Editor) Refresh();
}
void FGamePlatformWeatherWindPreview::RootTransformChanged(USceneComponent* Component, EUpdateTransformFlags, ETeleportType)
{
    if (IsValid(Component) && IsValid(Component->GetWorld()) && Component->GetWorld()->WorldType == EWorldType::Editor) Refresh();
}
void FGamePlatformWeatherWindPreview::PropertyChanged(UObject* Object, FPropertyChangedEvent&)
{
    const auto* Wind = Cast<AWindDirectionalSource>(Object);
    const auto* Component = Cast<UWindDirectionalSourceComponent>(Object);
    const auto* World = Wind ? Wind->GetWorld() : (Component ? Component->GetWorld() : nullptr);
    if (IsValid(World) && World->WorldType == EWorldType::Editor) Refresh();
}
void FGamePlatformWeatherWindPreview::Refresh()
{
    if (bApplying || !GEditor || IsRunningCommandlet()) return;
    TGuardValue<bool> Guard(bApplying, true);
    auto* World = GEditor->GetEditorWorldContext().World();
    if (!IsValid(World) || World->WorldType != EWorldType::Editor || World->bIsTearingDown) return;
    if (PreviewWorld.Get() != World) Restore();
    FGamePlatformWeatherState State; AWindDirectionalSource* Source = nullptr; FString Error;
    const auto Result = GamePlatformWeatherWindSource::Read(*World, State, Source, Error);
    if (Result != EGamePlatformSceneWindResult::Applied)
    {
        Restore();
        if (Result == EGamePlatformSceneWindResult::Invalid) UE_LOG(LogTemp, Warning, TEXT("EditorWindPreviewRejected: %s"), *Error);
        return;
    }
    if (!PreviewWorld.IsValid())
    {
        if (!GamePlatformSurfaceWindPreview::Read(*World, Before, Error)) return;
        PreviewWorld = World;
    }
    if (PreviewRoot.Get() != Source->GetRootComponent())
    {
        if (auto* Root = PreviewRoot.Get()) Root->TransformUpdated.Remove(RootTransformHandle);
        PreviewRoot = Source->GetRootComponent();
        RootTransformHandle = PreviewRoot->TransformUpdated.AddRaw(this, &FGamePlatformWeatherWindPreview::RootTransformChanged);
    }
    FGamePlatformSurfaceWindTransition Wind;
    Wind.From = Wind.To = FVector4(State.WindYawDegrees, State.WindIntensity, State.WindGustStrength, State.WindGustFrequencyHz);
    if (!GamePlatformSurfaceWindPreview::Apply(*World, Wind, Error))
        UE_LOG(LogTemp, Warning, TEXT("EditorWindPreviewFailed: %s"), *Error);
}
#endif
