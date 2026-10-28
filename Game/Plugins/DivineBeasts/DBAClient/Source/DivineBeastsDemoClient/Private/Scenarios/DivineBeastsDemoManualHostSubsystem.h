// 每GI仅协调开发人工服务的独占所有权；不持有业务快照/状态机，不创建第二份平台执行器。
#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#include "DivineBeastsDemoManualHostSubsystem.generated.h"
UCLASS(Transient)
class UDivineBeastsDemoManualHostSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool ShouldCreateSubsystem(UObject* Outer) const override;
    FGamePlatformResult Acquire(const FDivineBeastsDemoRunToken& Token,UWorld& World);
    FGamePlatformResult Release(const FDivineBeastsDemoRunToken& Token);
    bool Owns(const FDivineBeastsDemoRunToken& Token) const {return Owner.IsValid() && Owner==Token;}
private:FDivineBeastsDemoRunToken Owner;TWeakObjectPtr<UWorld> Source;
};

