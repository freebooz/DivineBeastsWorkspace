#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Types/GamePlatformSurfaceTypes.h"
#include "Components/SceneComponent.h"
class UWorld;
/** 模块生命周期内的Editor通知订阅；只租用当前Editor世界风预览，关闭/换图恢复原四项。 */
class FGamePlatformWeatherWindPreview
{
public:
    void Start();
    void Stop();
private:
    void Refresh();
    void Restore();
    void MapOpened(const FString&, bool);
    void PropertyChanged(UObject*, struct FPropertyChangedEvent&);
    void ActorMoved(class AActor* Actor);
    void RootTransformChanged(USceneComponent*, EUpdateTransformFlags, ETeleportType);
    FDelegateHandle MapHandle, PropertyHandle, DeleteHandle, UndoHandle, MoveHandle;
    FDelegateHandle RootTransformHandle;
    TWeakObjectPtr<USceneComponent> PreviewRoot;
    TWeakObjectPtr<UWorld> PreviewWorld;
    FGamePlatformSurfaceWindTransition Before;
    bool bApplying = false;
};
#endif
