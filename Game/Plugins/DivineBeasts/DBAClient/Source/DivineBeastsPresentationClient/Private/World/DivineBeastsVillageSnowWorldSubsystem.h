// 神兽联盟项目第三层客户端世界地形积雪适配。
// Landscape无法在Cook后通过编辑器SetMaterial路径安全换材质：此处只写已烘焙的中性地形MID参数。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "TimerManager.h" // 开发审核截图一次性定时器；普通天气只用已有事件
#include "DivineBeastsVillageSnowWorldSubsystem.generated.h"

class ALandscapeProxy;
class AActor;
class UMeshComponent;
class UMaterialInterface;
class UGamePlatformWeatherClientWorldSubsystem;
struct FGamePlatformWeatherState;
struct FGamePlatformResult;

/**
 * 新手村客户端雪纹理桥：不拥有天气事实、不生成地形、不改变服务器材质或碰撞。
 * 地图唯一真源仍在DBAWorldPack_Village，地图持有不引用VFX/Surface的中性Landscape母材质；
 * 本类仅客户端在实际世界BeginPlay时订阅WeatherClient，并通过GamePlatformData异步租约
 * 加载平台纹理，按天气变化写入该地图已有的Landscape动态材质实例（MID）。
 * 不使用Tick、每帧扫描地图、同步LoadObject或Landscape EditorOnly API。
 */
UCLASS()
class UDivineBeastsVillageSnowWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 仅适用于Game/PIE非Dedicated世界；具体Village身份在OnWorldBeginPlay再核实。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** 供UE自动化测试：仅识别真正的Village地图（允许PIE前缀），不把登录/竞技误识别为村庄。 */
    static bool IsVillageWorldPackage(const FString& PackageName);

    /** 供UE自动化测试：无效天气值拒绝；合法值转成0..1的只读材质视觉雪量。 */
    static bool MakeClampedVisualState(const FGamePlatformWeatherState& State,
        float& OutSnowAmount, float& OutWetness);

private:
    /** 客户端一次世界生命周期内最多持有一个异步资源租约；无服务时等待下一次天气事件再尝试。 */
    void TryRequestSnowTextures();

    /** 只有签发身份、代次、所属世界均匹配时，异步完成才允许修改地形纹理。 */
    void HandleTexturesReady(int64 ExpectedGeneration,
        const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result);

    /** 由现有GamePlatformWeatherClient通知；只修改材质值，不提交天气命令。 */
    void OnVisualWeatherChanged(const FGamePlatformWeatherState& State);

    /** 处理流送后生成的Landscape代理；仅本世界游戏线程，无全局持续Tick。 */
    void OnLandscapeActorSpawned(AActor* Actor);
    /** 只有真实Landscape MID已应用数值及高清贴图且回读一致，才返回成功。 */
    bool ApplyToLandscape(ALandscapeProxy* Landscape);

    /** 平台Data世界租约异步加载第三层雪苔MI；失败保留原始岩石材质而不阻断世界。 */
    void TryRequestMossMaterial();
    void HandleMossMaterialLoaded(int64 ExpectedGeneration,
        const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result);

    /** 仅替换明确带MossSnowComponentTag的网格；保存旧材质以在世界关闭时还原。 */
    void ApplyMossMaterialToActor(AActor* Actor);
    /** 原世界退出后先移除委托、释放租约，防止跨地图晚回调和动态材质泄漏。 */
#if !UE_BUILD_SHIPPING
    /**
     * 开发专用、只读的客户端权威天气回归采样。必须显式传入DBAVillageSnowTraceWeather。
     * 每跨越显著雪量或天气类型变化时核验MID标量；不影响天气/网络/着色器生产路径。
     */
    void TraceWeatherVisualForReview(const FGamePlatformWeatherState& State);
    /**
     * 非Shipping显式截图请求：只有真实游戏视口、HeavySnow和正确MID才执行。
     * 截图隐藏UMG（bShowUI=false），仅留作程序化视觉证据，不能替代人工验收。
     */
    void CaptureWeatherReviewScreenshot();
#endif

    void ReleaseResources();

    /** 一次材质替换的恢复位置；只保存弱组件，不延长世界Actor生命周期。 */
    struct FMossMaterialSlotBinding
    {
        TWeakObjectPtr<UMeshComponent> Component;
        int32 MaterialSlot = INDEX_NONE;
        int32 RetainedMaterialIndex = INDEX_NONE;
    };

    /** 保存旧材质强引用直至本世界结束，避免GC后无法安全回退。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> RetainedMossOriginalMaterials;

    TArray<FMossMaterialSlotBinding> MossMaterialSlotBindings;
    FGamePlatformDataLease MossMaterialLease;
    TWeakObjectPtr<UMaterialInterface> LoadedMossMaterial;
    int64 MossRequestGeneration = 0;
    bool bMossMaterialReady = false;

    TWeakObjectPtr<UGamePlatformWeatherClientWorldSubsystem> WeatherClient;
    FDelegateHandle WeatherChangedHandle;
    FDelegateHandle ActorSpawnedHandle;
    FGamePlatformDataLease TextureLease;
    /** 已真正写入并回读过高清雪纹理的Landscape弱标识；天气过渡不重复设置Texture参数。 */
    TSet<TWeakObjectPtr<ALandscapeProxy>> TexturesAppliedLandscapes;
    int64 RequestGeneration = 0;
    float CurrentSnowAmount = 0.f;
    float CurrentWetness = 0.f;
    bool bVillageWorld = false;
    bool bClosing = false;
    bool bTexturesReady = false;
    bool bLoggedMissingMaterial = false;
#if !UE_BUILD_SHIPPING
    bool bReviewWeatherTrace = false;
    float LastTracedSnowAmount = -1.f;
    int32 LastTracedWeatherType = -1;
    bool bReviewScreenshotEnabled = false;
    bool bReviewScreenshotQueued = false;
    FTimerHandle ReviewScreenshotTimer;
#endif
};
