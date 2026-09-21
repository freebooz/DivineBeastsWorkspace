#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "DBAFoundationCoordinator.generated.h"

class UGameInstance;
class UWorld;

/** 本项目开发入口装配，显式启用且非发行构建才运行；不承担平台状态机职责。 */
UCLASS(Transient)
class UDBAFoundationCoordinator final : public UObject
{
    GENERATED_BODY()
public:
    /** 游戏线程；Owner 不能跨实例，协调器 Outer 由游戏实例持有。 */
    void Initialize(UGameInstance& Owner);
    /** 游戏线程撤销轮询与弱上下文；重复调用无副作用。 */
    void Shutdown();
    /** 返回拥有自身存储的诊断文字，关闭后不引用旧世界。 */
    FString GetDiagnostics() const { return Diagnostics; }
private:
    bool Tick(float DeltaSeconds);
    TWeakObjectPtr<UGameInstance> OwnerInstance;
    TWeakObjectPtr<UWorld> ReportedWorld;
    FTSTicker::FDelegateHandle TickerHandle;
    FString RunId;
    FString Diagnostics = TEXT("基础工程开发验证未启用");
};
