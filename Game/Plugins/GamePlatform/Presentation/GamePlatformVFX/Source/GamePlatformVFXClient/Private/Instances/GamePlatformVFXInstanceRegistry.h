// 本文件属于GamePlatform平台层GamePlatformVFX客户端，持有世界完整实例句柄与组件反向索引；无权威玩法或资产加载职责。
// 中文接口/所有权/完成与取消语义见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "Instances/GamePlatformVFXInstanceRecord.h"

class UNiagaraComponent;
class UGamePlatformVFXDefinition;
class UWorld;

/** Handle -> 活动实例的唯一运行时索引，并维护Component反向索引避免完成回调线性扫描。 */
class FGamePlatformVFXInstanceRegistry
{
public:
    FGamePlatformVFXHandle Reserve(UWorld* World = nullptr);
    bool SetDefinition(const FGamePlatformVFXHandle& Handle, UGamePlatformVFXDefinition* Definition);
    bool AttachComponent(const FGamePlatformVFXHandle& Handle, UNiagaraComponent* Component, bool bPooled);
    bool AddChild(const FGamePlatformVFXHandle& Parent, const FGamePlatformVFXHandle& Child);
    bool Stop(
        const FGamePlatformVFXHandle& Handle,
        bool bStopComponent = true,
        bool bStopChildren = true);
    bool IsActive(const FGamePlatformVFXHandle& Handle) const;
    /** 游戏线程核对完整Id/Generation/World账本身份；组件已自然失活仍持有，直到Stop移除。 */
    bool OwnsHandle(const FGamePlatformVFXHandle& Handle) const;
    bool IsActiveId(const FGuid& Id) const;
    UNiagaraComponent* GetComponent(const FGamePlatformVFXHandle& Handle) const;
    FGamePlatformVFXHandle FindByComponent(const UNiagaraComponent* Component) const;
    TArray<FGamePlatformVFXHandle> GetChildren(const FGamePlatformVFXHandle& Handle) const;
    void Reset();
    int32 Num() const { return Records.Num(); }

private:
    TMap<FGuid, FGamePlatformVFXInstanceRecord> Records;
    TMap<const UNiagaraComponent*, FGamePlatformVFXHandle> ComponentHandles;
};
