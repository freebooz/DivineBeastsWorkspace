#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DivineBeastsClientTelemetryBootstrapSubsystem.generated.h"

/**
 * UDivineBeastsClientTelemetryBootstrapSubsystem（神兽联盟客户端遥测装配子系统）。
 *
 * 职责：只把项目 Gateway 地址和 Online 的瞬时 Authorization Header 注入平台 Telemetry；
 * 不复制 Buffer/Sampling/Retry，不保存 AccessToken，不让遥测失败改变登录或玩法流程。
 */
UCLASS()
class UDivineBeastsClientTelemetryBootstrapSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool bConfiguredNetworkSink = false;
};
