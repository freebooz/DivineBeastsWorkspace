// 网络天气对象：每个已激活服务器世界恰好一个；不复制Niagara粒子或客户端材质状态。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "GamePlatformWeatherReplicator.generated.h"

/** 只复制单一快照；拒绝客户端发起的天气修改，迟加入者由初始Actor复制获得最新版。 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMWEATHERRUNTIME_API AGamePlatformWeatherReplicator final : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformWeatherReplicator();
    const FGamePlatformWeatherSnapshot& GetSnapshot() const { return Snapshot; }
    /** 只允许当前世界的权威天气子系统调用，变更会主动刷新网络复制。 */
    bool PublishAuthoritativeSnapshot(const FGamePlatformWeatherSnapshot& InSnapshot);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_WeatherSnapshot)
    FGamePlatformWeatherSnapshot Snapshot;
    UFUNCTION()
    void OnRep_WeatherSnapshot();
    void NotifyWorldService() const;
};
