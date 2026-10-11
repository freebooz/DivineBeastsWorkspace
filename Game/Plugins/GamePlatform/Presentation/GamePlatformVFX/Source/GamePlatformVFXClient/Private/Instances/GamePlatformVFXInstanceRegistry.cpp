// 本文件属于GamePlatform平台层GamePlatformVFX客户端，落实实例账本所有权；自然失活和资源清理分别判定。
// 中文接口/所有权/完成与取消语义见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
#include "NiagaraWorldManager.h"
// 把UWorld转为UObject弱引用需要完整继承定义，不能依赖Unity/PCH间接包含。
#include "Engine/World.h"

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::Reserve(UWorld* World)
{
    FGamePlatformVFXHandle Handle;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = 1;
    Handle.World = World;

    FGamePlatformVFXInstanceRecord& Record = Records.Add(Handle.Id);
    Record.Handle = Handle;
    Record.State = EGamePlatformVFXInstanceState::Pending;
    return Handle;
}

bool FGamePlatformVFXInstanceRegistry::SetDefinition(
    const FGamePlatformVFXHandle& Handle,
    UGamePlatformVFXDefinition* Definition)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !IsValid(Definition))
    {
        return false;
    }
    Record->Definition = Definition;
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AttachComponent(
    const FGamePlatformVFXHandle& Handle,
    UNiagaraComponent* Component,
    bool bPooled)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !IsValid(Component))
    {
        return false;
    }

    if (UNiagaraComponent* Existing = Record->Component.Get())
    {
        ComponentHandles.Remove(Existing);
    }

    Record->Component = Component;
    Record->bPooled = bPooled;
    Record->State = EGamePlatformVFXInstanceState::Active;
    ComponentHandles.Add(Component, Handle);
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AddChild(
    const FGamePlatformVFXHandle& Parent,
    const FGamePlatformVFXHandle& Child)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Parent.Id);
    if (!Record || Record->Handle != Parent || !Child.IsValid())
    {
        return false;
    }

    Record->Children.AddUnique(Child);
    Record->State = EGamePlatformVFXInstanceState::Active;
    return true;
}

bool FGamePlatformVFXInstanceRegistry::Stop(
    const FGamePlatformVFXHandle& Handle,
    const bool bStopComponent,
    const bool bStopChildren)
{
    const FGamePlatformVFXInstanceRecord* Existing = Records.Find(Handle.Id);
    if (!Existing || Existing->Handle != Handle)
    {
        return false;
    }

    FGamePlatformVFXInstanceRecord Record = *Existing;
    // 调用方可把Records中的Handle直接传入；删除后只读取已复制的完整身份。
    Records.Remove(Record.Handle.Id);

    if (UNiagaraComponent* Component = Record.Component.Get())
    {
        const FGamePlatformVFXHandle* OwnerBeforeStop = ComponentHandles.Find(Component);
        if (OwnerBeforeStop && *OwnerBeforeStop == Record.Handle)
        {
            // 暂留反向索引作为旧Owner凭证；同句柄重入已被上方Records删除阻断。
            // 完成观察者可能同步新建/再停止另一Owner，不能先删索引后猜测组件仍归本次Stop。
            if (bStopComponent) Component->DeactivateImmediate();

            // Component在这里只作索引键；回调可能已销毁对象，后续解引用必须重新取弱引用。
            const FGamePlatformVFXHandle* OwnerAfterStop = ComponentHandles.Find(Component);
            if (OwnerAfterStop && *OwnerAfterStop == Record.Handle)
            {
                ComponentHandles.Remove(Component);
                if (UNiagaraComponent* Remaining = Record.Component.Get();
                    bStopComponent && Remaining && !Remaining->IsActive())
                {
                    // AutoRelease的预激活失败不会触发OnSystemComplete；按真实池状态补归还。
                    // 正常完成已变FreeInPool时必须跳过，池禁用时实际None也不能残留。
                    if (Remaining->PoolingMethod == ENCPoolMethod::AutoRelease)
                    {
                        UWorld* ComponentWorld = Remaining->GetWorld();
                        FNiagaraWorldManager* WorldManager = IsValid(ComponentWorld)
                            ? FNiagaraWorldManager::Get(ComponentWorld) : nullptr;
                        UNiagaraComponentPool* NativePool = WorldManager ? WorldManager->GetComponentPool() : nullptr;
                        if (NativePool)
                        {
                            NativePool->ReclaimWorldParticleSystem(Remaining);
                        }
                        else
                        {
                            // 世界Niagara管理器已退出，没有可归还的原生池；终止仍存活的旧组件。
                            Remaining->PoolingMethod = ENCPoolMethod::None;
                            Remaining->DestroyComponent();
                        }
                    }
                    else if (Remaining->PoolingMethod == ENCPoolMethod::None)
                    {
                        Remaining->DestroyComponent();
                    }
                }
            }
        }
    }

    if (bStopChildren)
    {
        for (const FGamePlatformVFXHandle& Child : Record.Children)
        {
            Stop(Child);
        }
    }

    return true;
}

bool FGamePlatformVFXInstanceRegistry::IsActive(
    const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle)
    {
        return false;
    }

    if (Record->State == EGamePlatformVFXInstanceState::Pending)
    {
        return true;
    }

    if (const UNiagaraComponent* Component = Record->Component.Get())
    {
        return Component->IsActive();
    }

    // 只有Composite父实例允许“Active但无Niagara Component”；普通实例若组件已失效应立即视为不活动。
    if (Record->State == EGamePlatformVFXInstanceState::Active)
    {
        if (const UGamePlatformVFXDefinition* Definition = Record->Definition.Get())
        {
            return Definition->GetBehavior() == EGamePlatformVFXBehavior::Composite;
        }
    }

    return false;
}

bool FGamePlatformVFXInstanceRegistry::IsActiveId(const FGuid& Id) const
{
    if (const FGamePlatformVFXInstanceRecord* Record = Records.Find(Id))
    {
        return IsActive(Record->Handle);
    }
    return false;
}

bool FGamePlatformVFXInstanceRegistry::OwnsHandle(const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Handle.IsValid() && Record && Record->Handle == Handle;
}

UNiagaraComponent* FGamePlatformVFXInstanceRegistry::GetComponent(
    const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle ? Record->Component.Get() : nullptr;
}

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::FindByComponent(
    const UNiagaraComponent* Component) const
{
    if (!IsValid(Component))
    {
        return {};
    }

    if (const FGamePlatformVFXHandle* Handle = ComponentHandles.Find(Component))
    {
        return *Handle;
    }
    return {};
}

TArray<FGamePlatformVFXHandle> FGamePlatformVFXInstanceRegistry::GetChildren(
    const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle
        ? Record->Children
        : TArray<FGamePlatformVFXHandle>();
}

void FGamePlatformVFXInstanceRegistry::Reset()
{
    TArray<FGuid> Keys;
    Records.GetKeys(Keys);
    for (const FGuid& Id : Keys)
    {
        if (const FGamePlatformVFXInstanceRecord* Record = Records.Find(Id))
        {
            Stop(Record->Handle);
        }
    }
    ComponentHandles.Reset();
    Records.Reset();
}
