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
    void ClearPawnBinding();
    void ClearAllBindings();

    TWeakObjectPtr<APawn> BoundPawn;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> BoundIdentity;
    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> BoundFlow;
    FDelegateHandle ReadinessHandle;
    FDelegateHandle IdentityHandle;
    FDelegateHandle FlowHandle;
    TSharedPtr<FStreamableHandle> PortraitLoadHandle;
    FSoftObjectPath RequestedPortraitPath;
    uint64 LoadGeneration = 0;
};
