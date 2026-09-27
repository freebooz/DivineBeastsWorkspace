#pragma once

#include "CoreMinimal.h"

/** FDivineBeastsFlowNodes（神兽联盟项目流程节点ID）。 */
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsFlowNodes
{
    static FName Boot() { return TEXT("DBA.Flow.Boot"); }
    static FName Initialize() { return TEXT("DBA.Flow.Initialize"); }
    static FName Authentication() { return TEXT("DBA.Flow.Authentication"); }
    static FName LoadProfile() { return TEXT("DBA.Flow.LoadProfile"); }
    static FName LoadRoster() { return TEXT("DBA.Flow.LoadRoster"); }
    static FName CharacterEntry() { return TEXT("DBA.Flow.CharacterEntry"); }
    static FName CreateCharacter() { return TEXT("DBA.Flow.CreateCharacter"); }
    static FName ValidateSelection() { return TEXT("DBA.Flow.ValidateSelection"); }
    static FName ResolveExperience() { return TEXT("DBA.Flow.ResolveExperience"); }
    static FName RequestWorld() { return TEXT("DBA.Flow.RequestWorld"); }
    static FName TransferWorld() { return TEXT("DBA.Flow.TransferWorld"); }
    static FName WorldReady() { return TEXT("DBA.Flow.WorldReady"); }
    static FName InWorld() { return TEXT("DBA.Flow.InWorld"); }
    static FName Recovering() { return TEXT("DBA.Flow.Recovering"); }
};

/**
 * FDivineBeastsFlowExecutors（神兽联盟流程执行器标识）。
 * ExecutorId只用于把数据驱动FlowDefinition映射到当前GameInstance注册的节点工厂；
 * 它不等于NodeId，也不通过字符串反射创建任意UObject。
 */
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsFlowExecutors
{
    static FName Boot() { return TEXT("DBA.Flow.Executor.Boot"); }
    static FName Initialize() { return TEXT("DBA.Flow.Executor.Initialize"); }
    static FName Authentication() { return TEXT("DBA.Flow.Executor.Authentication"); }
    static FName LoadProfile() { return TEXT("DBA.Flow.Executor.LoadProfile"); }
    static FName LoadRoster() { return TEXT("DBA.Flow.Executor.LoadRoster"); }
    static FName CharacterEntry() { return TEXT("DBA.Flow.Executor.CharacterEntry"); }
    static FName CreateCharacter() { return TEXT("DBA.Flow.Executor.CreateCharacter"); }
    static FName ValidateSelection() { return TEXT("DBA.Flow.Executor.ValidateSelection"); }
    static FName ResolveExperience() { return TEXT("DBA.Flow.Executor.ResolveExperience"); }
    static FName RequestWorld() { return TEXT("DBA.Flow.Executor.RequestWorld"); }
    static FName TransferWorld() { return TEXT("DBA.Flow.Executor.TransferWorld"); }
    static FName WorldReady() { return TEXT("DBA.Flow.Executor.WorldReady"); }
    static FName InWorld() { return TEXT("DBA.Flow.Executor.InWorld"); }
    static FName Recovering() { return TEXT("DBA.Flow.Executor.Recovering"); }
};

/** FDivineBeastsFlowOutcomes（神兽联盟流程具名结果）。 */
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsFlowOutcomes
{
    static FName CreateCharacter() { return TEXT("CreateCharacter"); }
    static FName SelectCharacter() { return TEXT("SelectCharacter"); }
    static FName RequestWorld() { return TEXT("RequestWorld"); }
    static FName Recover() { return TEXT("Recover"); }
    static FName Logout() { return TEXT("Logout"); }
};
