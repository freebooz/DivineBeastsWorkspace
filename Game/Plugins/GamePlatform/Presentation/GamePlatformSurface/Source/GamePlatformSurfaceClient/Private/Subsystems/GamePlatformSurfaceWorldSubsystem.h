#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlatformSurfaceWorldSubsystem.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/**
 * GamePlatformSurface世界服务。
 *
 * 只在客户端可表现世界中创建；无Tick。状态变化时一次性写入MPC，并在世界销毁时释放绑定和订阅。
 */
UCLASS()
class UGamePlatformSurfaceWorldSubsystem final : public UWorldSubsystem, public IGamePlatformSurfaceService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformSurfaceUpdateResult ApplyEnvironmentState(
        const FGamePlatformSurfaceEnvironmentState& State) override;
    virtual const FGamePlatformSurfaceEnvironmentState& GetEnvironmentState() const override;
    virtual int32 GetRevision() const override;
    virtual FGamePlatformSurfaceUpdateResult RefreshMaterialBinding() override;
    virtual FDelegateHandle AddStateChangedHandler(
        const FGamePlatformSurfaceStateChanged::FDelegate& Handler) override;
    virtual void RemoveStateChangedHandler(FDelegateHandle Handle) override;

private:
    friend class FGamePlatformSurfaceBindingRefreshTest;
    /** 解析配置软引用并绑定当前世界MPC实例；失败不创建伪资产。 */
    bool ResolveMaterialBinding();

    /** 把CurrentState一次性写入已绑定MPC；调用前必须位于游戏线程。 */
    FGamePlatformSurfaceUpdateResult PushCurrentStateToMaterialParameters();

    FGamePlatformSurfaceEnvironmentState CurrentState;
    int32 Revision = 0;
    bool bClosing = false;
    bool bLoggedBindingFailure = false;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialParameterCollection> BoundCollection;

    TWeakObjectPtr<UMaterialParameterCollectionInstance> BoundInstance;
    FGamePlatformSurfaceStateChanged StateChanged;
};
