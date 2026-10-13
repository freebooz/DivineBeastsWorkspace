// SSLM适配注册实现；所有调用限游戏线程，工厂只由明确项目组合根注入，不扫描任意脚本。
#include "Scenarios/DivineBeastsDemoAdapterRegistry.h"
namespace DivineBeasts::Demo
{
FAdapterRegistry::FAdapterRegistry() : InstanceId(FGuid::NewGuid()) {}
FDivineBeastsDemoAdapterRegistrationHandle FAdapterRegistry::Register(FName Id, FDivineBeastsDemoAdapterFactory Factory)
{
    check(IsInGameThread());
    if (Id.IsNone() || !Factory || Entries.Contains(Id) || Generation == MAX_uint64) return {};
    auto Entry = MakeShared<FEntry>();
    Entry->Handle.InstanceId = InstanceId; Entry->Handle.RegistrationId = FGuid::NewGuid(); Entry->Handle.Generation = ++Generation;
    Entry->Factory = MoveTemp(Factory); Entries.Add(Id, Entry); return Entry->Handle;
}
FName FAdapterRegistry::FindIdentity(const FDivineBeastsDemoAdapterRegistrationHandle& Handle) const
{
    if (!Handle.IsValid() || Handle.InstanceId != InstanceId) return NAME_None;
    for (const auto& Entry : Entries) if (Entry.Value->Handle == Handle) return Entry.Key;
    return NAME_None;
}
FGamePlatformResult FAdapterRegistry::Unregister(const FDivineBeastsDemoAdapterRegistrationHandle& Handle)
{
    check(IsInGameThread());
    const FName Id = FindIdentity(Handle);
    if (Id.IsNone()) return FGamePlatformResult::Failure(FName(TEXT("Demo.InvalidRegistration")), TEXT("适配注册句柄过期、被修改或属于另一实例。"));
    Entries.Remove(Id); return FGamePlatformResult::Success();
}
bool FAdapterRegistry::Contains(FName Id) const { return Entries.Contains(Id); }
TUniquePtr<IDivineBeastsDemoScenarioAdapter> FAdapterRegistry::Create(FName Id, UWorld& World) const
{
    check(IsInGameThread());
    const auto Entry = Entries.FindRef(Id);
    return Entry ? Entry->Factory(World) : nullptr;
}
void FAdapterRegistry::Reset() { Entries.Reset(); }
}
