// 每GI仅协调开发人工服务的独占所有权；不持有业务快照/状态机，不创建第二份平台执行器。
#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#if WITH_EDITOR
#include "Manual/GamePlatformEditorResources.h"
#endif
#include "DivineBeastsDemoManualHostSubsystem.generated.h"
UCLASS(Transient)
class UDivineBeastsDemoManualHostSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool ShouldCreateSubsystem(UObject* Outer) const override;
    FGamePlatformResult Acquire(const FDivineBeastsDemoRunToken& Token,UWorld& World);

    /** 仅精确Editor功能走显式真实PIE路径；普通Acquire资格保持原样。 */
    /** 新来源的只读资格检查；恢复页可通过但不能取得/抢占旧资源。 */
    FGamePlatformResult ValidateEditorFixtureSource(const FDivineBeastsDemoRunToken& Token,UWorld& World,FName Feature)const;
    FGamePlatformResult AcquireEditorFixture(const FDivineBeastsDemoRunToken& Token,UWorld& World,FName Feature);
    static bool IsEditorFeature(FName Feature);

    bool OwnsEditorFixture(const FDivineBeastsDemoRunToken& Token)const;
#if WITH_EDITOR
    /** 仅复制原Token已拥有的完整租约，用于World退出后转交同一Editor所有者。 */
    FGamePlatformEditorResourceLease CopyEditorLease(const FDivineBeastsDemoRunToken& Token)const{return Owns(Token)?EditorLease:FGamePlatformEditorResourceLease{};}
#endif
    FGamePlatformResult Release(const FDivineBeastsDemoRunToken& Token);

    bool Owns(const FDivineBeastsDemoRunToken& Token) const {return Owner.IsValid() && Owner==Token;}

private:FDivineBeastsDemoRunToken Owner;TWeakObjectPtr<UWorld> Source;
#if WITH_EDITOR
    FGamePlatformEditorResourceLease EditorLease;
#endif

};
