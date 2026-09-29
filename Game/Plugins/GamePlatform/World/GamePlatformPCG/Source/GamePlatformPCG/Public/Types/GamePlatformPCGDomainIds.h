#pragma once

#include "CoreMinimal.h"

/**
 * FGamePlatformPCGDomainIds（PCG领域ID目录）。
 * 领域ID用于模板、Definition和文档之间的稳定语义映射，不等于“一项一个C++类”。
 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGDomainIds
{
    static const FName FieldHeight;
    static const FName FieldBiomeWeight;
    static const FName FieldBuildable;

    static const FName ForestCanopy;
    static const FName ForestUnderstory;
    static const FName ForestFloor;
    static const FName ForestEdge;
    static const FName RockScatter;
    static const FName RockFormation;

    static const FName AgriParcel;
    static const FName AgriCrop;
    static const FName AgriOrchard;
    static const FName AgriFallow;

    static const FName RoadNetwork;
    static const FName RoadSurface;
    static const FName RoadShoulder;
    static const FName PathTrail;

    static const FName WaterRiver;
    static const FName WaterLake;
    static const FName WaterBank;

    static const FName BridgeSpan;
    static const FName GateFarm;
    static const FName BreakRoad;

    static const FName EnclosureFieldFence;
    static const FName EnclosurePaddock;
    static const FName EnclosureYardWall;
    static const FName EnclosureHedgeRow;
    static const FName EnclosureRoadGuard;
    static const FName EnclosureCliffRail;
    static const FName EnclosureDockRail;

    static const FName SettlementParcel;
    static const FName SettlementBuilding;
    static const FName SettlementYard;

    static const FName PlayResource;
    static const FName PlayCover;
    static const FName PlayClimb;
    static const FName PlaySpawn;
    static const FName GameplayExclusion;

    static const FName StateHarvest;
    static const FName StateChop;
    static const FName StateGate;
    static const FName StatePersist;

    static TConstArrayView<FName> All();
    static bool IsKnown(FName DomainId);
};
