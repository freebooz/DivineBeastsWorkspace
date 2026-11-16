#pragma once
#include "Framework/GamePlatformArenaPlayerController.h"
#include "DivineBeastsArenaPlayerController.generated.h"
class UDivineBeastsPlayerControlsComponent;

/** SSLM竞技控制器薄适配；保留平台竞技RPC/断线语义，只转发共同本地操控生命周期。 */
UCLASS()
class DIVINEBEASTSARENARUNTIME_API ADivineBeastsArenaPlayerController : public AGamePlatformArenaPlayerController
{
    GENERATED_BODY()
public:
    ADivineBeastsArenaPlayerController();
    virtual void FlushPressedKeys() override;
    virtual void UpdateRotation(float DeltaTime) override;
    virtual void UpdateCameraManager(float DeltaSeconds) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    virtual void OnRep_PlayerState() override;
    virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
protected:
    virtual void SetupInputComponent() override;
    virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    /** 非复制本地输入组件；服务器不执行鼠标或镜头行为，不实现Experience资格Provider。 */
    UPROPERTY() TObjectPtr<UDivineBeastsPlayerControlsComponent> PlayerControls;
};
