#include "Definitions/DivineBeastsArenaEntranceDefinition.h"
#include "Identity/DivineBeastsProjectCatalog.h"

bool UDivineBeastsArenaEntranceDefinition::IsConfigured() const
{
    FName Role, Experience;
    return LogicalId.IsValid() &&
        FDivineBeastsProjectCatalog::TryGetArenaContextForMode(ArenaModeId,Role,Experience) &&
        Role==TEXT("GameServer.Role.MainArena");
}
