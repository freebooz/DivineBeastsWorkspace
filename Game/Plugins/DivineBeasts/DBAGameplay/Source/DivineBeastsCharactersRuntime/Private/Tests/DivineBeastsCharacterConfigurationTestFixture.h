// Editor物理重叠回归专用Pawn：只提供真实Capsule委托的同步监听者，不制造Definition/Data/Ready成功。
// UHT与C++均使用锁定引擎支持的WITH_EDITORONLY_DATA条件；Server/Shipping不定义本反射类型。
#pragma once
#include "Characters/DivineBeastsCharacter.h"
#include "Components/CapsuleComponent.h"
#include "DivineBeastsCharacterConfigurationTestFixture.generated.h"

#if WITH_EDITORONLY_DATA
/** 测试拥有一次Overlap回调并在EndPlay解绑；没有生产实例、资产挂载或网络命令。 */
UCLASS(Transient, NotBlueprintable)
class ADivineBeastsCharacterConfigurationTestPawn : public ADivineBeastsCharacter
{
    GENERATED_BODY()
public:
    /** GT安装一次真实ComponentBeginOverlap监听，目标Actor由夹具拥有；回调可以同步配置后继或关闭组件。 */
    void ArmConfigurationOverlap(AActor* ExpectedActor, TFunction<void()> Action)
    {
        DisarmConfigurationOverlap();
        ExpectedOverlapActor = ExpectedActor; OverlapAction = MoveTemp(Action); OverlapNotifications = 0;
        GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleConfigurationOverlap);
    }
    void DisarmConfigurationOverlap()
    {
        GetCapsuleComponent()->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleConfigurationOverlap);
        OverlapAction = nullptr; ExpectedOverlapActor.Reset();
    }
    int32 GetConfigurationOverlapNotifications() const { return OverlapNotifications; }
protected:
    void EndPlay(const EEndPlayReason::Type Reason) override
    { DisarmConfigurationOverlap(); Super::EndPlay(Reason); }
private:
    /** 仅由UE真实碰撞通知调用；取走回调后再执行，避免递归碰撞再次触发或关闭销毁正在执行的函数。 */
    UFUNCTION()
    void HandleConfigurationOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
    {
        (void)Component; (void)OtherComponent; (void)OtherBodyIndex; (void)bFromSweep; (void)SweepResult;
        if (OtherActor != ExpectedOverlapActor.Get() || !OverlapAction) { return; }
        ++OverlapNotifications; auto Action = MoveTemp(OverlapAction); Action();
    }
    TWeakObjectPtr<AActor> ExpectedOverlapActor;
    TFunction<void()> OverlapAction;
    int32 OverlapNotifications = 0;
};
#endif
