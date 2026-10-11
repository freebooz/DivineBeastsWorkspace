// 第三层客户端场景锚点：把物种/数量转为项目中立表现请求；不拥有Niagara或资产租约。
// 游戏线程按事件申请/停止；组件路径与注册token隔离场景身份，拒绝候选时保留合法旧绑定。
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DivineBeastsFaunaSceneComponent.generated.h"
class UDivineBeastsPresentationClientSubsystem;

/** 场景群落配置组件：无Tick、无复制、无玩法碰撞，执行仍归唯一平台VFX。 */
UCLASS(ClassGroup=(DivineBeasts),meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsFaunaSceneComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsFaunaSceneComponent();
    /** 空值使用组件完整对象路径，区分同Actor多组件和流送Level实例；显式标识拒绝其他Owner占用。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Fauna",meta=(DisplayName="群落标识"))
    FName HabitatId = NAME_None;
    /** 项目物种选项。扩展物种使用定义和映射，不改变平台GPU算法。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Fauna",meta=(GetOptions="GetFaunaSpeciesOptions",DisplayName="群落类型"))
    FName SpeciesId = TEXT("ButterflyAzure");
    /** 0撤销本组件注册；1..512为请求数量，实际可见数量仍受平台预算约束。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Fauna",meta=(ClampMin="0",ClampMax="512",DisplayName="群落数量"))
    int32 Count = 32;
    /** 确定性初始分布/运动种子；同种/同数量/同位置/同种子保持当前请求。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Fauna",meta=(ClampMin="0",ClampMax="1000000",DisplayName="行为种子"))
    int32 Seed = 137;
    UFUNCTION(BlueprintPure,Category="DivineBeasts|Fauna")
    TArray<FString> GetFaunaSpeciesOptions() const;
    /** 显式应用配置；候选被拒绝时保留旧绑定。true仅表示受理，不代表GPU已播放。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|Fauna")
    bool ApplyFauna(FString& OutError);
    /** 仅撤销本组件持有的注册token；旧组件不能停止后来占用同名ID的请求。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|Fauna")
    void StopFauna();
    /** 纯本地惊扰：世界中心cm，Radius>0，Strength0..1，DurationSeconds0..60；返回受影响群数。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|Fauna")
    int32 DisturbFauna(const FVector& WorldCenter, float Radius, float Strength, float DurationSeconds, FString& OutError);
    /** 当前有效注册的CPU行为快照：LocalPositions为群落局部厘米坐标；不表示GPU可见数量。
     * 固定步快照可能领先渲染插值一个模拟步，不能用于捕获、碰撞、奖励或任何玩法权威。
     * false时清空所有输出并提供错误；场景身份被替换、尚未建立实例或世界退出均不可查询。
     */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|Fauna",meta=(DisplayName="查询群落CPU状态"))
    bool QueryFaunaState(int32& SimulatedCount, int32& PerchedCount, int32& ReservedFlowerSlots,
        bool& bRenderingActive, TArray<FVector>& LocalPositions, FString& OutError) const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> BoundPresentation;
    FName BoundHabitatId = NAME_None;
    FGuid BoundRegistrationToken;
    /** 每次Apply/Stop先更换，防止同步取消回调后外层覆盖内层绑定。 */
    FGuid ApplyMutationToken;
    bool bEndingPlay = false;
};
