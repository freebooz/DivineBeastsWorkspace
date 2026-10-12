// 第三层世界生态路线Actor：仅在客户端/开发Editor放置，组合Spline和有限批次鸟群。
 // 平台只接收中立USplineComponent弱引用，不反向硬依赖神兽联盟具体地图。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DivineBeastsBirdFlockRouteActor.generated.h"

class USceneComponent;
class USplineComponent;
class UDivineBeastsBirdFlockBatchComponent;

/** 可放置在Village/OpenWorld客户端表现层的鸟群路径Actor；无碰撞、无复制、无每帧Tick。 */
UCLASS(Blueprintable)
class DIVINEBEASTSPRESENTATIONCLIENT_API ADivineBeastsBirdFlockRouteActor : public AActor
{
    GENERATED_BODY()
public:
    ADivineBeastsBirdFlockRouteActor();

    /** 模型路径锚点；虚幻样条本身拥有曲线关键点及长度。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BirdFlock|Route")
    TObjectPtr<USplineComponent> FlightSpline;

    /** 群体生成只委托项目客户端正式表现Service，不创建逐鸟Actor。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BirdFlock|Route")
    TObjectPtr<UDivineBeastsBirdFlockBatchComponent> FlockBatches;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BirdFlock|Route")
    TObjectPtr<USceneComponent> SceneRoot;

    /** 显式启动，调用时本地玩家和正式表现服务必须就绪。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|BirdFlock|Route")
    bool StartRouteBirds(FString& OutError);

    /** 仅撤销本路线Actor所有已受理鸟群请求。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|BirdFlock|Route")
    void StopRouteBirds();
};
