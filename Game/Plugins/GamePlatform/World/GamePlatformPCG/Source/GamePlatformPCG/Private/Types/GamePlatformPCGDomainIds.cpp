#include "Types/GamePlatformPCGDomainIds.h"

const FName FGamePlatformPCGDomainIds::FieldHeight(TEXT("Field.Height"));
const FName FGamePlatformPCGDomainIds::FieldBiomeWeight(TEXT("Field.BiomeWeight"));
const FName FGamePlatformPCGDomainIds::FieldBuildable(TEXT("Field.Buildable"));

const FName FGamePlatformPCGDomainIds::ForestCanopy(TEXT("Forest.Canopy"));
const FName FGamePlatformPCGDomainIds::ForestUnderstory(TEXT("Forest.Understory"));
const FName FGamePlatformPCGDomainIds::ForestFloor(TEXT("Forest.Floor"));
const FName FGamePlatformPCGDomainIds::ForestEdge(TEXT("Forest.Edge"));
const FName FGamePlatformPCGDomainIds::RockScatter(TEXT("Rock.Scatter"));
const FName FGamePlatformPCGDomainIds::RockFormation(TEXT("Rock.Formation"));

const FName FGamePlatformPCGDomainIds::AgriParcel(TEXT("Agri.Parcel"));
const FName FGamePlatformPCGDomainIds::AgriCrop(TEXT("Agri.Crop"));
const FName FGamePlatformPCGDomainIds::AgriOrchard(TEXT("Agri.Orchard"));
const FName FGamePlatformPCGDomainIds::AgriFallow(TEXT("Agri.Fallow"));

const FName FGamePlatformPCGDomainIds::RoadNetwork(TEXT("Road.Network"));
const FName FGamePlatformPCGDomainIds::RoadSurface(TEXT("Road.Surface"));
const FName FGamePlatformPCGDomainIds::RoadShoulder(TEXT("Road.Shoulder"));
const FName FGamePlatformPCGDomainIds::PathTrail(TEXT("Path.Trail"));

const FName FGamePlatformPCGDomainIds::WaterRiver(TEXT("Water.River"));
const FName FGamePlatformPCGDomainIds::WaterLake(TEXT("Water.Lake"));
const FName FGamePlatformPCGDomainIds::WaterBank(TEXT("Water.Bank"));

const FName FGamePlatformPCGDomainIds::BridgeSpan(TEXT("Bridge.Span"));
const FName FGamePlatformPCGDomainIds::GateFarm(TEXT("Gate.Farm"));
const FName FGamePlatformPCGDomainIds::BreakRoad(TEXT("Break.Road"));

const FName FGamePlatformPCGDomainIds::EnclosureFieldFence(TEXT("Encl.FieldFence"));
const FName FGamePlatformPCGDomainIds::EnclosurePaddock(TEXT("Encl.Paddock"));
const FName FGamePlatformPCGDomainIds::EnclosureYardWall(TEXT("Encl.YardWall"));
const FName FGamePlatformPCGDomainIds::EnclosureHedgeRow(TEXT("Encl.HedgeRow"));
const FName FGamePlatformPCGDomainIds::EnclosureRoadGuard(TEXT("Encl.RoadGuard"));
const FName FGamePlatformPCGDomainIds::EnclosureCliffRail(TEXT("Encl.CliffRail"));
const FName FGamePlatformPCGDomainIds::EnclosureDockRail(TEXT("Encl.DockRail"));

const FName FGamePlatformPCGDomainIds::SettlementParcel(TEXT("Settle.Parcel"));
const FName FGamePlatformPCGDomainIds::SettlementBuilding(TEXT("Settle.Building"));
const FName FGamePlatformPCGDomainIds::SettlementYard(TEXT("Settle.Yard"));

const FName FGamePlatformPCGDomainIds::PlayResource(TEXT("Play.Resource"));
const FName FGamePlatformPCGDomainIds::PlayCover(TEXT("Play.Cover"));
const FName FGamePlatformPCGDomainIds::PlayClimb(TEXT("Play.Climb"));
const FName FGamePlatformPCGDomainIds::PlaySpawn(TEXT("Play.Spawn"));
const FName FGamePlatformPCGDomainIds::GameplayExclusion(TEXT("Gameplay.Exclusion"));

const FName FGamePlatformPCGDomainIds::StateHarvest(TEXT("State.Harvest"));
const FName FGamePlatformPCGDomainIds::StateChop(TEXT("State.Chop"));
const FName FGamePlatformPCGDomainIds::StateGate(TEXT("State.Gate"));
const FName FGamePlatformPCGDomainIds::StatePersist(TEXT("State.Persist"));

TConstArrayView<FName> FGamePlatformPCGDomainIds::All()
{
    static const TArray<FName> Values =
    {
        FieldHeight, FieldBiomeWeight, FieldBuildable,
        ForestCanopy, ForestUnderstory, ForestFloor, ForestEdge, RockScatter, RockFormation,
        AgriParcel, AgriCrop, AgriOrchard, AgriFallow,
        RoadNetwork, RoadSurface, RoadShoulder, PathTrail,
        WaterRiver, WaterLake, WaterBank,
        BridgeSpan, GateFarm, BreakRoad,
        EnclosureFieldFence, EnclosurePaddock, EnclosureYardWall, EnclosureHedgeRow,
        EnclosureRoadGuard, EnclosureCliffRail, EnclosureDockRail,
        SettlementParcel, SettlementBuilding, SettlementYard,
        PlayResource, PlayCover, PlayClimb, PlaySpawn, GameplayExclusion,
        StateHarvest, StateChop, StateGate, StatePersist
    };
    return Values;
}

bool FGamePlatformPCGDomainIds::IsKnown(FName DomainId)
{
    return All().Contains(DomainId);
}
