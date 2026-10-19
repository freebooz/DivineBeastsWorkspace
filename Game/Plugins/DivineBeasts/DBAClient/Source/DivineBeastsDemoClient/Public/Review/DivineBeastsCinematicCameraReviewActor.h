// SSLM独立开发镜头演示Actor：编辑器蓝图可配置现有摄像机/配置，禁止修改Gameplay和生产关卡。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DivineBeastsCinematicCameraReviewActor.generated.h"

class ACineCameraActor;
class APlayerController;
class UGamePlatformCinematicCameraProfile;
class UGamePlatformCinematicCameraSubsystem;
class UDivineBeastsCameraReviewWidget;

/**
 * 开发客户端电影镜头演示入口。四个镜头与四份Profile由正式编辑器资产引用，
 * 游戏线程按本地玩家调用GamePlatformCamera服务，使用键盘操作，不创建每帧Tick。
 * 仅允许放入/Game/Development/PluginDemos开发地图，绝不作为权威Gameplay Actor。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API ADivineBeastsCinematicCameraReviewActor : public AActor
{
    GENERATED_BODY()
public:
    ADivineBeastsCinematicCameraReviewActor();

    /** 24mm广角实体及其独立开发Profile。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<ACineCameraActor> Camera24mm;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<UGamePlatformCinematicCameraProfile> Profile24mm;

    /** 35mm默认电影蓝图实体及其Profile。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<ACineCameraActor> Camera35mm;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<UGamePlatformCinematicCameraProfile> Profile35mm;

    /** 50mm中焦展示机位及Profile。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<ACineCameraActor> Camera50mm;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<UGamePlatformCinematicCameraProfile> Profile50mm;

    /** 85mm长焦特写机位及Profile。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<ACineCameraActor> Camera85mm;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SSLM|Camera Review")
    TObjectPtr<UGamePlatformCinematicCameraProfile> Profile85mm;

    /** 0/1/2/3分别切换24/35/50/85mm；失败保持当前镜头。 */
    UFUNCTION(BlueprintCallable, Category="SSLM|Camera Review")
    bool SelectLens(int32 Index);

    /** 对当前展示镜头调光圈，正方向减小f值、虚化背景；不改DataAsset。 */
    UFUNCTION(BlueprintCallable, Category="SSLM|Camera Review")
    void AdjustAperture(bool bOpen);

    /** 对焦点每次调整50厘米，正方向聚焦远方；不改DataAsset。 */
    UFUNCTION(BlueprintCallable, Category="SSLM|Camera Review")
    void AdjustFocus(bool bFarther);

    /** 恢复当前镜头的焦距、光圈、焦点距离。 */
    UFUNCTION(BlueprintCallable, Category="SSLM|Camera Review")
    void ResetLens();

    /** 返回最近一次成功切换的镜头序号，尚未激活时返回-1。 */
    UFUNCTION(BlueprintPure, Category="SSLM|Camera Review")
    int32 GetActiveLensIndex() const { return ActiveIndex; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void SetupLocalInput();
    void RunAutoCycleStep();
    void Select24();
    void Select35();
    void Select50();
    void Select85();
    void OpenAperture();
    void CloseAperture();
    void FocusNear();
    void FocusFar();
    ACineCameraActor* ResolveCamera(int32 Index) const;
    UGamePlatformCinematicCameraProfile* ResolveProfile(int32 Index) const;
    UGamePlatformCinematicCameraSubsystem* ResolveService() const;
    void PrintCameraStatus(const FString& Message) const;

    /** 只在开发世界存在的本地UMG控件，随本Actor清理。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsCameraReviewWidget> ControlsWidget;
    bool bPreviousMouseCursor = false;
    TWeakObjectPtr<APlayerController> BoundController;
    FTimerHandle InitTimer;
    FTimerHandle AutoCycleTimer;
    int32 InitAttempts = 0;
    int32 AutoCycleSteps = 0;
    int32 ActiveIndex = -1;
    bool bInputBound = false;
};