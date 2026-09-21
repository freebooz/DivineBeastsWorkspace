#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DBAGameInstance.generated.h"

class UDBAFoundationCoordinator;

/** 项目游戏实例入口。仅持有本实例协调器，不在此实现通用加载器或第二套流程执行器。 */
UCLASS()
class DIVINEBEASTSARENA_API UDBAGameInstance final : public UGameInstance
{
    GENERATED_BODY()
public:
    /** 游戏线程实例初始化；不假定地图或资产已就绪。 */
    virtual void Init() override;
    /** 游戏线程幂等关闭，先撤销本实例开发装配再关闭引擎实例。 */
    virtual void Shutdown() override;
    /** HUD 的只读值快照；不返回协调器或资产的裸指针。 */
    FString GetFoundationDiagnostics() const;
private:
    UPROPERTY(Transient)
    TObjectPtr<UDBAFoundationCoordinator> FoundationCoordinator;
};
