#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Delegates/Delegate.h"
#include "DivineBeastsClientTelemetryBootstrapSubsystem.generated.h"

struct FGamePlatformAuthSnapshot;
struct FDivineBeastsFlowViewState;
class UGamePlatformOnlineClientSubsystem;
class UDivineBeastsApplicationFlowSubsystem;

/**
 * UDivineBeastsClientTelemetryBootstrapSubsystem（神兽联盟客户端遥测装配子系统）。
 *
 * 职责：只把项目 Gateway 地址与 Online 的请求授权能力注入平台 Telemetry；
 * 不读取/复制 Authorization 字符串，不保存 AccessToken，不复制 Buffer/Sampling/Retry，也不让遥测失败改变登录或玩法流程。
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
    void HandleAuthStateChanged(const FGamePlatformAuthSnapshot& Snapshot);
    void HandleFlowViewStateChanged(const FDivineBeastsFlowViewState& ViewState);
    /** 为当前认证代次创建新的NetworkSink；每次账号切换都必须重建，禁止旧会话Retry复用新Token。 */
    bool ConfigureNetworkSinkForAuthenticatedSession();
    /** 切回NullSink并Shutdown旧NetworkSink，用于登出、账号切换和授权上下文失效。 */
    void SwitchToNullSink();

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> OnlineSubsystem;
    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> ApplicationFlowSubsystem;
    FDelegateHandle AuthStateChangedHandle;
    FDelegateHandle FlowViewStateChangedHandle;
    /** 当前客户端遥测会话只用于采样/关联，不是账号ID，也不携带认证权限。 */
    FString TelemetrySessionId;
    /** 当前遥测Sink绑定的Online认证代次；用于拒绝旧账号批次跨账号继续重试。 */
    FGuid TelemetryAuthGeneration;
    /** 客户端Gateway基址只保存非秘密URL；AccessToken始终由Online在每次请求发送前动态注入。 */
    FString GatewayBaseUrl;
    /** 最近一次观察到的应用流程节点；只用于避免同一TransferWorld节点重复触发Travel边界刷新。 */
    FName LastObservedFlowStep = NAME_None;
    bool bConfiguredNetworkSink = false;
};
