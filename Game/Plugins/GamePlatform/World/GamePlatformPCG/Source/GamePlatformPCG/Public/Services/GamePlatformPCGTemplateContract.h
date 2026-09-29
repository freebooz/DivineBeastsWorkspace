#pragma once

#include "CoreMinimal.h"

class UGamePlatformPCGProfileDefinition;
class UPCGGraph;

/** 1.0 Template Contract（模板合同）稳定ID；这里只登记合同，不伪造任何.uasset模板。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGTemplateIds
{
    static const FName Base;
    static const FName ScatterSurface;
    static const FName BiomeGenerator;
    static const FName LinearDresser;
    static const FName Enclosure;
    static const FName EnclosureClosed;
    static const FName Connector;
    static const FName GateInsert;
    static const FName ParcelFill;
    static const FName CropField;
    static const FName AssemblySpawn;
    static const FName InterfaceBand;
    static const FName RailingAttached;

    static TConstArrayView<FName> All();
    static bool IsKnown(FName TemplateId);
};

/** 1.0 Foundation Subgraph（基础公共子图）稳定ID；子图属于平台创作合同，不是新的Primitive（原语）。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGSubgraphIds
{
    static const FName ProjectOnLandscape;
    static const FName PriorityCarve;
    static const FName ApplySpawnPolicy;
    static const FName AssignMeshSet;
    static const FName FitPostsToSpline;
    static const FName BreakByIntersection;
    static const FName WriteClosedExclude;

    static TConstArrayView<FName> All();
    static bool IsKnown(FName SubgraphId);
};

/**
 * FGamePlatformPCGTemplateContract（PCG模板合同）。
 * 当前只开放M0/M1批准节点；HiGen/GPU/未知节点继续失败关闭。
 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGTemplateContract
{
    static bool IsApprovedSettingsClass(const UClass* SettingsClass);
    static bool IsTemplateHeaderValid(const UGamePlatformPCGProfileDefinition& Profile);
};
