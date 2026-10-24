// 平台通用Mesh渲染退场：只消费调用方的生命周期，不读取战斗或项目类型。
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "GamePlatformMeshDissolveComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** 游戏线程表现参数；秒为世界游戏时间。原材质必须实现GP_Dissolve参数合同。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformMeshDissolveSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dissolve",meta=(ClampMin="0"))
    float DelaySeconds=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dissolve",meta=(ClampMin="0.01"))
    float DurationSeconds=1.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dissolve")
    FLinearColor EdgeColor=FLinearColor(.08f,.65f,.42f,1.f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dissolve",meta=(ClampMin="0"))
    float EdgeStrength=1.5f;
};

/** 客户端通用材质操作，不播放Niagara、不加载资产、不修改Actor/碰撞或服务器生命周期。
 * Begin/Reset/Refresh只能在GT调用；同代重复不重播。实例拥有MID与一个结束Timer，EndPlay清理。
 */
UCLASS(ClassGroup=(GamePlatform),Transient,meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMVFXCLIENT_API UGamePlatformMeshDissolveComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGamePlatformMeshDissolveComponent();
    /** 开始本实例代次；false为拒绝或同代重复。初始已结束快照只隐藏，不重播过期动画。 */
    UFUNCTION(BlueprintCallable,Category="VFX|Dissolve")
    bool StartDissolve(int32 InstanceGeneration,const FGamePlatformMeshDissolveSettings& Settings,
        bool bInitiallyCompleted=false);
    /** GT恢复本组件仍拥有的材质和初始可见性，取消旧截止；不覆盖外部替换的材质。 */
    UFUNCTION(BlueprintCallable,Category="VFX|Dissolve")
    void ResetDissolve();
    /** 外观加载完成后由其所有者通知；新Mesh沿用原起点，截止后到达也立即隐藏。 */
    UFUNCTION(BlueprintCallable,Category="VFX|Dissolve")
    void RefreshDissolveMeshes();
    /** 包含完成隐藏状态；必须等Reset或新代次才允许重新开始。 */
    bool IsDissolveActive()const{return bActive;}
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy)override;
private:
    friend class FGamePlatformMeshDissolveLifecycleTest;
    struct FSlot
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        int32 Index=INDEX_NONE;
        TWeakObjectPtr<UMaterialInterface> Original;
        TWeakObjectPtr<UMaterialInstanceDynamic> Dynamic;
    };
    struct FVisibility
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        bool bInitiallyVisible=false;
        bool bHiddenByThis=false;
    };
    TArray<FSlot> Slots;
    TArray<FVisibility> Visibility;
    UPROPERTY(Transient)TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
    FGamePlatformMeshDissolveSettings ActiveSettings;
    FTimerHandle FinishTimer;
    FGuid Operation;
    int32 Generation=0;
    float MaterialStartTime=0;
    bool bActive=false;
    bool bFinished=false;
    bool bClosing=false;
    void FinishDissolve(FGuid ExpectedOperation);
};
