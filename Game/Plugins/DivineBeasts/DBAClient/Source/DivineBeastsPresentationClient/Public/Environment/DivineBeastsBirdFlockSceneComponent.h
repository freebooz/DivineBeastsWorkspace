// 第三层客户端场景锚点：将策划选择的鸟类/数量转为既有项目中立表现请求。
// 不持有网格、Niagara或资产租约；执行和资源所有权仍属于GamePlatformData/VFX服务。
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DivineBeastsBirdFlockSceneComponent.generated.h"
class UDivineBeastsPresentationClientSubsystem;

/** 拖入客户端场景蓝图即可指定鸟种/数量，始终无Tick、无复制、无玩法碰撞。 */
UCLASS(ClassGroup=(DivineBeasts),meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsBirdFlockSceneComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsBirdFlockSceneComponent();
    /** 空值自动取场景Owner名称；同世界不能有两个锚点使用同一显式标识。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(DisplayName="鸟群标识"))
    FName FlockId=NAME_None;
    /** 项目鸟种选项。扩展新鸟种只需新增定义和映射，不修改平台GPU算法。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(GetOptions="GetBirdSpeciesOptions",DisplayName="鸟类类型"))
    FName SpeciesId=TEXT("Seagull");
    /** 本地GPU粒子数，0关闭并释放该实例；硬上限512，平台环境预算仍可剔除。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(ClampMin="0",ClampMax="512",DisplayName="鸟类数量"))
    int32 BirdCount=32;
    UFUNCTION(BlueprintPure,Category="DivineBeasts|BirdFlock")
    TArray<FString> GetBirdSpeciesOptions() const;
    /** 蓝图改变数量/物种后显式应用；编辑器非PIE只保存配置，不启动生产表现链。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|BirdFlock")
    bool ApplyBirdFlock(FString& OutError);
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|BirdFlock")
    void StopBirdFlock();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> BoundPresentation;
    FName BoundFlockId=NAME_None;
};
