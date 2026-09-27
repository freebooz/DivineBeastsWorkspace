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
    static FName RequestWorld() { return TEXT("DBA.Flow.RequestWorld"); }
    static FName TransferWorld() { return TEXT("DBA.Flow.TransferWorld"); }
    static FName WorldReady() { return TEXT("DBA.Flow.WorldReady"); }
    static FName InWorld() { return TEXT("DBA.Flow.InWorld"); }
    static FName Recovering() { return TEXT("DBA.Flow.Recovering"); }
};
