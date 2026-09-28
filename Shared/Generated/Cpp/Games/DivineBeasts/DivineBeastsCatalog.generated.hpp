// Code generated from Shared/Contracts/Games/DivineBeasts and Shared/Docs. DO NOT EDIT.
#pragma once
#include <array>
#include <string_view>
namespace DivineBeasts::Contracts {
inline constexpr const char* GameId = "divinebeasts";
inline constexpr const char* ProjectId = "DivineBeastsArena";
inline constexpr const char* ContractVersion = "2.0.0";
inline constexpr int CatalogVersion = 1;
inline constexpr const char* GeneratedRevision = "b62b6b25a11c115ff8a20c206defa7d9d24171e878b17c76f875740677253b2a";
inline constexpr const char* ClientServerMinimumContractVersion = "2.0.0";
inline constexpr const char* ClientServerMaximumExclusiveContractVersion = "3.0.0";
inline constexpr const char* ServerBackendMinimumContractVersion = "2.0.0";
inline constexpr const char* ServerBackendMaximumExclusiveContractVersion = "3.0.0";
inline constexpr std::string_view GameServerRoleOpenWorld = "GameServer.Role.OpenWorld";
inline constexpr std::string_view GameServerRoleVillage = "GameServer.Role.Village";
inline constexpr std::string_view GameServerRoleMainArena = "GameServer.Role.MainArena";
inline constexpr std::string_view ExperienceLobbyMain = "Experience.Lobby.Main";
inline constexpr std::string_view ExperienceOpenWorldMain = "Experience.OpenWorld.Main";
inline constexpr std::string_view ExperienceVillageMain = "Experience.Village.Main";
inline constexpr std::string_view ExperienceVillageTutorial = "Experience.Village.Tutorial";
inline constexpr std::string_view ExperienceVillageTraining = "Experience.Village.Training";
inline constexpr std::string_view ExperienceMainArenaMain = "Experience.MainArena.Main";
inline constexpr std::string_view ExperienceOpenWorldHub = "Experience.OpenWorld.Hub";
inline constexpr std::string_view ArenaModeDuel1v1 = "Arena.Mode.Duel1v1";
inline constexpr std::string_view ArenaModeTeam2v2 = "Arena.Mode.Team2v2";
inline constexpr std::string_view ArenaModeTeam3v3 = "Arena.Mode.Team3v3";
inline constexpr std::string_view ArenaModeTeam4v4 = "Arena.Mode.Team4v4";
inline constexpr std::string_view ArenaModeTeam5v5 = "Arena.Mode.Team5v5";
inline constexpr std::string_view HeroZodiacRat = "Hero.Zodiac.Rat";
inline constexpr std::string_view HeroZodiacOx = "Hero.Zodiac.Ox";
inline constexpr std::string_view HeroZodiacTiger = "Hero.Zodiac.Tiger";
inline constexpr std::string_view HeroZodiacRabbit = "Hero.Zodiac.Rabbit";
inline constexpr std::string_view HeroZodiacDragon = "Hero.Zodiac.Dragon";
inline constexpr std::string_view HeroZodiacSnake = "Hero.Zodiac.Snake";
inline constexpr std::string_view HeroZodiacHorse = "Hero.Zodiac.Horse";
inline constexpr std::string_view HeroZodiacGoat = "Hero.Zodiac.Goat";
inline constexpr std::string_view HeroZodiacMonkey = "Hero.Zodiac.Monkey";
inline constexpr std::string_view HeroZodiacRooster = "Hero.Zodiac.Rooster";
inline constexpr std::string_view HeroZodiacDog = "Hero.Zodiac.Dog";
inline constexpr std::string_view HeroZodiacBoar = "Hero.Zodiac.Boar";
inline constexpr std::array<const char*, 3> ServerRoles = {{"GameServer.Role.OpenWorld", "GameServer.Role.Village", "GameServer.Role.MainArena"}};
inline constexpr std::array<const char*, 7> ExperienceIds = {{"Experience.Lobby.Main", "Experience.OpenWorld.Main", "Experience.Village.Main", "Experience.Village.Tutorial", "Experience.Village.Training", "Experience.MainArena.Main", "Experience.OpenWorld.Hub"}};
inline constexpr std::array<const char*, 5> ArenaModeIds = {{"Arena.Mode.Duel1v1", "Arena.Mode.Team2v2", "Arena.Mode.Team3v3", "Arena.Mode.Team4v4", "Arena.Mode.Team5v5"}};
inline constexpr std::array<const char*, 12> HeroDefinitionIds = {{"Hero.Zodiac.Rat", "Hero.Zodiac.Ox", "Hero.Zodiac.Tiger", "Hero.Zodiac.Rabbit", "Hero.Zodiac.Dragon", "Hero.Zodiac.Snake", "Hero.Zodiac.Horse", "Hero.Zodiac.Goat", "Hero.Zodiac.Monkey", "Hero.Zodiac.Rooster", "Hero.Zodiac.Dog", "Hero.Zodiac.Boar"}};
struct ExperienceServerRoleMapping { const char* ExperienceId; const char* ServerRoleId; };
inline constexpr std::array<ExperienceServerRoleMapping, 7> ExperienceMappings = {{ExperienceServerRoleMapping{"Experience.OpenWorld.Hub", "GameServer.Role.OpenWorld"}, ExperienceServerRoleMapping{"Experience.OpenWorld.Main", "GameServer.Role.OpenWorld"}, ExperienceServerRoleMapping{"Experience.Lobby.Main", "GameServer.Role.OpenWorld"}, ExperienceServerRoleMapping{"Experience.Village.Main", "GameServer.Role.Village"}, ExperienceServerRoleMapping{"Experience.Village.Tutorial", "GameServer.Role.Village"}, ExperienceServerRoleMapping{"Experience.Village.Training", "GameServer.Role.Village"}, ExperienceServerRoleMapping{"Experience.MainArena.Main", "GameServer.Role.MainArena"}}};
inline constexpr const auto& ExperienceServerRoleMappings = ExperienceMappings;
struct ArenaModeContextMapping { const char* ArenaModeId; const char* ServerRoleId; const char* ExperienceId; };
inline constexpr std::array<ArenaModeContextMapping, 5> ArenaModeMappings = {{ArenaModeContextMapping{"Arena.Mode.Duel1v1", "GameServer.Role.MainArena", "Experience.MainArena.Main"}, ArenaModeContextMapping{"Arena.Mode.Team2v2", "GameServer.Role.MainArena", "Experience.MainArena.Main"}, ArenaModeContextMapping{"Arena.Mode.Team3v3", "GameServer.Role.MainArena", "Experience.MainArena.Main"}, ArenaModeContextMapping{"Arena.Mode.Team4v4", "GameServer.Role.MainArena", "Experience.MainArena.Main"}, ArenaModeContextMapping{"Arena.Mode.Team5v5", "GameServer.Role.MainArena", "Experience.MainArena.Main"}}};
}
