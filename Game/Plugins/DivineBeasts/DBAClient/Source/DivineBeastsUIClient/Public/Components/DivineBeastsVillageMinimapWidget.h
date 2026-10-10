// 项目层客户端新手村小地图适配；读取本地Pawn移动事件，拥有普通纹理加载句柄，不参与世界权威或准入。
#pragma once

#include "Components/GamePlatformMinimapWidget.h"
#include "DivineBeastsVillageMinimapWidget.generated.h"

class APawn;
class ACharacter;
struct FStreamableHandle;

/** 消费本地移动事实的小地图。底图由真实新手村俯视捕获生成；Widget不扫描地形、不启用业务Tick。 */
UCLASS(Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsVillageMinimapWidget : public UGamePlatformMinimapWidget
{
    GENERATED_BODY()
public:
    /** 游戏线程绑定当前本地Pawn；空值撤销旧事件、取消加载并隐藏，重绑不会保留旧世界快照。 */
    void BindToPawn(APawn* Pawn);
    /** 世界厘米投影到[0,1]；+X为北/+Y为东，边界中心0、尺寸50400cm；非法输入返回false且不改OutUV。 */
    static bool ProjectVillagePosition(const FVector& PositionCentimeters, FVector2D& OutUV);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void BeginDestroy() override;

private:
    /** 移动组件完成位置更新后刷新；DeltaSeconds等仅描述事件，不决定权威位置。 */
    UFUNCTION()
    void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
    /** 旧角色销毁立即解除所有绑定，不等候下一次UI更新。 */
    UFUNCTION()
    void HandlePawnDestroyed(AActor* DestroyedActor);
    /** 当前纹理已加载才发布完整快照；过期回调由绑定代次和弱引用阻止。 */
    void RefreshProjection();
    /** 原生事件/加载句柄始终释放；正常解绑发布空快照，GC销毁不能访问Widget树或调用蓝图。 */
    void ClearPawnBinding(bool bPublishEmptySnapshot = true);

    TWeakObjectPtr<ACharacter> BoundCharacter;
    TSharedPtr<FStreamableHandle> MapLoadHandle;
    uint64 BindingGeneration = 0;
    /** 只增不减，避免同一Widget重绑相同MapId被平台版本门禁拒绝。 */
    int64 SnapshotRevision = 0;
    /** 防止Super销毁重入NativeDestruct或取消请求期间的回调重新发布界面。 */
    bool bDestroyingNativeResources = false;
};
