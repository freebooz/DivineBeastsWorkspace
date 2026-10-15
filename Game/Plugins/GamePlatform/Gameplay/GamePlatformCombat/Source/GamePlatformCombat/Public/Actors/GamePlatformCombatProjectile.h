// 平台共享权威投射物：服务器运动、连续查询与一次结算；客户端只观察复制事实，不运行VFX。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformCombatSpec.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "GamePlatformCombatProjectile.generated.h"
class USphereComponent;
class AGamePlatformCombatProjectile;

/** 最小复制事实，不含伤害数值/技能类/资产路径。来源标签含义由项目适配解释。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatProjectileFacts
{
    GENERATED_BODY()
    /** 每枚弹体唯一发生身份；连发每枚不同，不与同枚的飞行/命中请求混用。 */
    UPROPERTY(BlueprintReadOnly) FGuid OccurrenceId;
    /** 不透明表现分类，空值表示无可选外观；平台不解析具体法术名字。 */
    UPROPERTY(BlueprintReadOnly) FName PresentationStyleId = NAME_None;
    /** 真实授予技能身份，仅供上下文关联，不能由此在客户端授权技能。 */
    UPROPERTY(BlueprintReadOnly) FName AbilityId = NAME_None;
    /** 来源可信化身代次；0表示尚未初始化，不可播放。 */
    UPROPERTY(BlueprintReadOnly) int32 SourceAvatarGeneration = 0;
    /** 已初始化后为true；客户端等待一次复制快照，不猜测生成次序。 */
    UPROPERTY(BlueprintReadOnly) bool bStarted = false;
    /** 最终态：命中/射程结束/来源失效/取消均结束；不可回退到飞行。 */
    UPROPERTY(BlueprintReadOnly) bool bFinished = false;
    /** 真正服务器碰撞时为true，不等于已成功造成伤害。 */
    UPROPERTY(BlueprintReadOnly) bool bImpacted = false;
    UPROPERTY(BlueprintReadOnly) FVector ImpactLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector ImpactNormal = FVector::UpVector;
};
DECLARE_MULTICAST_DELEGATE_TwoParams(FGamePlatformProjectileFactsChanged,
    AGamePlatformCombatProjectile*, const FGamePlatformCombatProjectileFacts&);

/** 直线/螺旋及显式目标追踪Actor；同世界服务器拥有运动与命中，不提供客户端伤害RPC。 */
UCLASS(Blueprintable)
class GAMEPLATFORMCOMBAT_API AGamePlatformCombatProjectile : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformCombatProjectile();
    /** 游戏线程、服务器、一次初始化；Spec来自成功Commit的调用方，复制事实不复制该伤害规格。
     * 无效/过期来源、方向、参数、重复初始化返回false，不扣费、不加载资源、不生成伤害。
     * 调用方拥有生成失败后的销毁与技能取消；平台不决定项目开发配置或技能输入。
     * Spec.Target为空保持原轨迹；非空时冻结目标化身并追踪，死亡/销毁/换化身即终止，不重新寻敌。
     */
    bool AuthorityInitialize(const FGamePlatformCombatSpec& Spec,
        const FGamePlatformCombatProjectileMotion& Motion, const FVector& Direction,
        FName PresentationStyleId = NAME_None);
    /** 可信服务器取消自有弹体；重复调用安全；无命中事实及伤害。 */
    void AuthorityCancel();
    bool IsFlightFinished() const { return Facts.bFinished; }
    const FGamePlatformCombatProjectileFacts& GetFlightFacts() const { return Facts; }
    FGamePlatformProjectileFactsChanged& OnFlightFactsChanged() { return FactsChanged; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    /** 原生球体根，只作为复制运动附着点；权威碰撞在显式World Sweep完成，客户端禁碰撞。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> CollisionRoot;
    UPROPERTY(ReplicatedUsing=OnRep_Facts) FGamePlatformCombatProjectileFacts Facts;
    /** 仅本服务器弹体拥有，不复制源强引用或任意伤害字段。EndPlay撤销。 */
    UPROPERTY(Transient) FGamePlatformCombatSpec CastSpec;
    FGamePlatformCombatProjectileMotion FlightMotion;
    FVector Origin = FVector::ZeroVector, Axis = FVector::ForwardVector;
    FVector RadialY = FVector::RightVector, RadialZ = FVector::UpVector;
    /** 显式追踪仅持弱引用；后续选择变化不能改写已发射弹体，过期目标不转为无目标弹体。 */
    TWeakObjectPtr<AActor> HomingTarget;
    int32 HomingTargetAvatarGeneration = 0;
    bool bHasHomingTarget = false;
    FVector HomingCenter = FVector::ZeroVector;
    float FlightSeconds = 0.0f;
    FGamePlatformProjectileFactsChanged FactsChanged;
    UFUNCTION() void OnRep_Facts();
    /** 确认原来源仍在同世界同代次且存活；每帧和每次结算前重新读取，不能沿用裸指针资格。 */
    bool HasCurrentSource() const;
    /** 每个真实运动子步及结算前检查冻结目标；无追踪请求返回true，不扫描世界。 */
    bool HasCurrentHomingTarget() const;
    /** 以固定速度推进追踪中心并保留螺旋截面；只有服务器物理子步调用。 */
    FVector AdvanceHomingPosition(float StepSeconds);
    FVector PositionAt(float Seconds) const;
    /** 先锁终态，再提交最多一次平台伤害；同步战斗回调重入不能复活/重复结算。 */
    void FinishFlight(const FHitResult* Hit);
};
