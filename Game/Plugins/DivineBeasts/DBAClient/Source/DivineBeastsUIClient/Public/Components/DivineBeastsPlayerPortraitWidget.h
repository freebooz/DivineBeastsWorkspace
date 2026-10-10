// 项目层客户端玩家肖像；角色身份与流程摘要事件提供只读事实，普通纹理句柄只属于当前Widget生命周期。
#pragma once

#include "Components/GamePlatformPortraitWidget.h"
#include "DivineBeastsPlayerPortraitWidget.generated.h"

class APawn;
class UDivineBeastsCharacterComponent;
class UDivineBeastsApplicationFlowSubsystem;
struct FStreamableHandle;
struct FDivineBeastsFlowViewState;

/** 项目玩家肖像适配；名称来自匹配当前角色身份的流程摘要，无等级真源时明确隐藏等级。 */
UCLASS(Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPlayerPortraitWidget : public UGamePlatformPortraitWidget
{
    GENERATED_BODY()
public:
    /** 游戏线程更换本地Pawn；撤销旧身份事件和纹理请求，空值清空，不发网络请求。 */
    void BindToPawn(APawn* Pawn);
    /** 仅核心目录认可的英雄可映射到所属英雄包；未知身份返回无效路径，禁止任意拼接资源名。 */
    static FSoftObjectPath ResolvePortraitPath(FName HeroDefinitionId);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void BeginDestroy() override;

private:
    void HandleReadinessChanged(bool bReady);
    void HandleIdentityChanged();
    void HandleFlowChanged(const FDivineBeastsFlowViewState& State);
    UFUNCTION()
    void HandlePawnDestroyed(AActor* DestroyedActor);
    /** 重新核对Hero/CharacterId之后替换完整快照；异步完成只允许当前代次渲染。 */
    void RefreshPortrait();
    /** 只更新已由Monolith创建的命名控件；此处不创建视觉资产或Widget树。 */
    void RenderPortrait();
    /** 始终解绑与取消自有请求；只有正常页面退出/换Pawn才发布空快照，GC销毁禁止蓝图事件及Widget操作。 */
    void ClearPawnBinding(bool bPublishEmptySnapshot = true);
    /** 流程事件同样在GC销毁时原生撤销；参数只控制最后的界面发布，不跳过资源释放。 */
    void ClearAllBindings(bool bPublishEmptySnapshot = true);

    TWeakObjectPtr<APawn> BoundPawn;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> BoundIdentity;
    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> BoundFlow;
    FDelegateHandle ReadinessHandle;
    FDelegateHandle IdentityHandle;
    FDelegateHandle FlowHandle;
    TSharedPtr<FStreamableHandle> PortraitLoadHandle;
    FSoftObjectPath RequestedPortraitPath;
    uint64 LoadGeneration = 0;
    /** BeginDestroy首先置位，覆盖Super销毁可能重入NativeDestruct的情况；销毁后任何回调不得渲染。 */
    bool bDestroyingNativeResources = false;
};
