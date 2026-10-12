// 表面痕迹独立验收场景Actor；只负责展示五种视觉贴花，不修改玩家账号/世界规则。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GamePlatformSurfaceMarkDemoActor.generated.h"

/** 放在平台独立演示地图中，PIE或-game时向五块测试板提交五种痕迹。 */
UCLASS(Blueprintable)
class GAMEPLATFORMSURFACECLIENT_API AGamePlatformSurfaceMarkDemoActor final : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformSurfaceMarkDemoActor();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark|Demo")
    bool bAutoEmitOnBeginPlay = true;

    /** 测试板沿本Actor X方向等距排列；厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark|Demo", meta=(ClampMin="150.0", Units="cm"))
    float PanelSpacingCm = 360.0f;

    /** 由演示界面按钮或PIE键盘指令手动重播，不接受真实战斗事件。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface|Demo")
    void EmitFiveMarks();

private:
    FTimerHandle DemoTimer;
};