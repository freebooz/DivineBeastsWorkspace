// 第三层客户端场景锚点：把鸟种/数量转为项目中立表现请求；不拥有Niagara或资产租约。
// 游戏线程按事件申请/停止；组件路径与注册token隔离场景身份，拒绝候选时保留合法旧绑定。
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DivineBeastsBirdFlockSceneComponent.generated.h"
class UDivineBeastsPresentationClientSubsystem;

/** 场景鸟群配置组件：无Tick、无复制、无玩法碰撞，执行仍归唯一平台VFX。 */
UCLASS(ClassGroup=(DivineBeasts),meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsBirdFlockSceneComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsBirdFlockSceneComponent();
    /** 空值使用组件完整对象路径，区分同Actor多组件和流送Level实例；显式标识拒绝其他Owner占用。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(DisplayName="鸟群标识"))
    FName FlockId = NAME_None;
    /** 项目鸟种选项。扩展物种使用定义和映射，不改变平台GPU算法。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(GetOptions="GetBirdSpeciesOptions",DisplayName="鸟类类型"))
    FName SpeciesId = TEXT("Seagull");
    /** 0撤销本组件注册；1..512为请求数量，实际可见数量仍受平台预算约束。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BirdFlock",meta=(ClampMin="0",ClampMax="512",DisplayName="鸟类数量"))
    int32 BirdCount = 32;
    UFUNCTION(BlueprintPure,Category="DivineBeasts|BirdFlock")
    TArray<FString> GetBirdSpeciesOptions() const;
    /** 显式应用配置；候选被拒绝时保留旧绑定。true仅表示受理，不代表GPU已播放。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|BirdFlock")
    bool ApplyBirdFlock(FString& OutError);
    /** 仅撤销本组件持有的注册token；旧组件不能停止后来占用同名ID的请求。 */
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
    FName BoundFlockId = NAME_None;
    FGuid BoundRegistrationToken;
    /** 每次Apply/Stop先更换，防止同步取消回调后外层覆盖内层绑定。 */
    FGuid ApplyMutationToken;
    bool bEndingPlay = false;
};
