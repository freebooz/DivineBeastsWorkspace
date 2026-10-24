// 第三层只读死亡适配：不新增死亡、复活或Actor销毁规则。
#pragma once
#include "Components/GamePlatformMeshDissolveComponent.h"
#include "DivineBeastsDeathDissolveComponent.generated.h"
class UGamePlatformCombatComponent;

/** 项目装配向下继承平台通用渲染组件；无MOBA专属语义时不制造空中间类型。 */
UCLASS(Transient)
class UDivineBeastsDeathDissolveComponent final : public UGamePlatformMeshDissolveComponent
{
    GENERATED_BODY()
public:
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    /** 现有异步外观作者完成后调用；不发起加载，不扫描世界。 */
    static void NotifyVisualsChanged(AActor* Actor);
private:
    TWeakObjectPtr<UGamePlatformCombatComponent> Combat;
    FDelegateHandle Subscription;
    void HandleStateChanged();
};
