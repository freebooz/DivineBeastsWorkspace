// SSLM新手村真实水体声源组件：不在游戏层实例化播放器，依托现有GamePlatformSFX语义和LocalPlayer世界租约。
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "DivineBeastsWaterAudioAnchorComponent.generated.h"

class AActor;
class UDivineBeastsPresentationClientSubsystem;

/**
 * 只加到真实湖泊/瀑布/溪流Actor，且Owner必须携带SSLM.WaterSource标签。
 * 声源位置取本SceneComponent的真实世界变换；由关卡编辑器将组件锚定在水体对象上。
 * 没有确认的源Actor/Tag时安全静音，不使用猜测的世界坐标，也不逐帧Tick。
 */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent, DisplayName="SSLM Water Audio Anchor"))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsWaterAudioAnchorComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UDivineBeastsWaterAudioAnchorComponent();

    /** 仅支持既有目录中原生Looping的两种水流声音。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Water")
    FName WaterLoopClipId = TEXT("Ambience.WaterStreamA");

    /** 真实水体场景Actor必须显式拥有此标签；无标签不得发声。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Water")
    FName RequiredSourceTag = TEXT("SSLM.WaterSource");

    /** 
     * 仅在Client/PIE的本地玩家就绪事件调用：将客户端专属组件临时绑定到真实水体Actor上的通用SceneComponent。
     * 通用水源Actor/标记点可参与Server Cook；禁止把本ClientOnly组件序列化进共享地图。
     * 同一真实标记点重入返回既有组件，冲突的Clip配置拒绝叠加。
     */
    UFUNCTION(BlueprintCallable, Category="SSLM|Audio|Water")
    static UDivineBeastsWaterAudioAnchorComponent* EnsureRuntimeAnchor(
        AActor* VerifiedWaterActor, USceneComponent* RealSoundPoint, FName ClipId);

    /** 首次本地玩家/内容包激活后可由关卡蓝图主动重试，成功表示平台已接受播放请求而非扬声器验收。 */
    UFUNCTION(BlueprintCallable, Category="SSLM|Audio|Water")
    bool RefreshWaterAudio();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool BindLocalPresentation();
    void HandleContentPackChanged(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State, const FString& Error);

    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> BoundPresentation;
    TWeakObjectPtr<UWorld> BoundWorld;
    FDelegateHandle PackChangedHandle;
    FName OwnedAnchorId = NAME_None;
};
