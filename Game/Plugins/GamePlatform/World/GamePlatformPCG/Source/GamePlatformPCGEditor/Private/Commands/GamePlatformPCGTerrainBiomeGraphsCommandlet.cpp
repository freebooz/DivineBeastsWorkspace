// GamePlatformPCGEditor：UE5.8原生GetLandscapeLayerWeight→SurfaceSampler→FilterGraph。
// 源于已批准平台Graph范式；所有Graph参数、层名由项目命令行提供。
// 只创建新的PCGGraph uasset，不触碰既有Gold模板、地图、PCGComponent或服务器权威。
#include "Commands/GamePlatformPCGTerrainBiomeGraphsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Elements/PCGAttributeFilter.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Elements/PCGTypedGetter.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainPCGBiome, Log, All);

UGamePlatformPCGTerrainBiomeGraphsCommandlet::UGamePlatformPCGTerrainBiomeGraphsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

namespace
{
struct FLayerGraphSpec
{
    FString BiomeName;
    FName LayerId;
    FString PackageName;
};

bool SafeToken(const FString& Value)
{
    if(Value.IsEmpty() || Value.Len()>54) return false;
    for(const TCHAR Character : Value)
    {
        if(!FChar::IsAlnum(Character) && Character != TEXT('_')) return false;
    }
    return true;
}

bool ValidateGraph(const UPCGGraph* Graph, const FName ExpectedLayer,
                   const float Threshold, FString& Error)
{
    if (!IsValid(Graph) || !Graph->bLandscapeUsesMetadata ||
        Graph->GetNodes().Num()!=3 || Graph->GetAllEdges().Num()!=5)
    {
        Error=TEXT("原生PCGGraph缺少权重元数据、三节点或五条GraphEdge。");
        return false;
    }
    int32 LandscapeCount=0, SamplerCount=0, FilterCount=0;
    for(const UPCGNode* Node : Graph->GetNodes())
    {
        if(!IsValid(Node)) return false;
        if(const UPCGGetLandscapeSettings* Landscape =
            Cast<const UPCGGetLandscapeSettings>(Node->GetSettings()))
        {
            ++LandscapeCount;
            if(!Landscape->SamplingProperties.bGetLayerWeights || Landscape->bUnbounded)
            {
                Error=TEXT("必须从有界Landscape采样真实图层权重。");
                return false;
            }
        }
        else if(const UPCGSurfaceSamplerSettings* Sampler =
            Cast<const UPCGSurfaceSamplerSettings>(Node->GetSettings()))
        {
            ++SamplerCount;
            if(Sampler->PointsPerSquaredMeter<=0.f ||
               Sampler->PointsPerSquaredMeter>.005f)
            {
                Error=TEXT("地形PCG采样密度超出平台预算。");
                return false;
            }
        }
        else if(const UPCGAttributeFilteringSettings* Filter =
            Cast<const UPCGAttributeFilteringSettings>(Node->GetSettings()))
        {
            ++FilterCount;
            if(Filter->TargetAttribute.GetAttributeName()!=ExpectedLayer ||
               Filter->Operator!=EPCGAttributeFilterOperator::GreaterOrEqual ||
               !Filter->bUseConstantThreshold ||
               Filter->AttributeTypes.Type!=EPCGMetadataTypes::Float ||
               !FMath::IsNearlyEqual(Filter->AttributeTypes.FloatValue,Threshold,.0001f))
            {
                Error=TEXT("Biome过滤器与指定LS_地貌权重/阈值不匹配。");
                return false;
            }
        }
        else
        {
            Error=TEXT("Graph包含未经批准的PCG节点；禁止覆盖。");
            return false;
        }
    }
    if(LandscapeCount!=1 || SamplerCount!=1 || FilterCount!=1)
    {
        Error=TEXT("地形PCG输入图节点种类不完整。");
        return false;
    }
    return true;
}
} // anonymous

int32 UGamePlatformPCGTerrainBiomeGraphsCommandlet::Main(const FString& Params)
{
    check(IsInGameThread());
    FString Root,Prefix,Map;
    if (!FParse::Value(*Params,TEXT("GraphRoot="),Root) ||
        !FParse::Value(*Params,TEXT("BiomeMap="),Map))
    {
        UE_LOG(LogTerrainPCGBiome, Error,
               TEXT("必须显式指定GraphRoot与BiomeMap，例如Forest:LS_Grass,Rock:LS_Rock。"));
        return 1;
    }
    if (!FParse::Value(*Params,TEXT("GraphPrefix="),Prefix))
        Prefix=TEXT("PCG_Terrain_");
    Root.RemoveFromEnd(TEXT("/"));
    const bool bValidateOnly=FParse::Param(*Params,TEXT("ValidateOnly"));
    float Threshold=.18f;
    FParse::Value(*Params,TEXT("WeightThreshold="),Threshold);
    if(!FMath::IsFinite(Threshold) || Threshold<.01f || Threshold>.95f ||
       !Root.StartsWith(TEXT("/")) ||
       !Root.Contains(TEXT("/Terrain/PCG/Graphs")) ||
       Root.StartsWith(TEXT("/Engine/")) ||
       Root.StartsWith(TEXT("/Script/")) ||
       !SafeToken(Prefix))
    {
        UE_LOG(LogTerrainPCGBiome, Error,
               TEXT("GraphRoot/Prefix/阈值不满足项目隔离与合法范围。"));
        return 1;
    }
    TArray<FString> Inputs;
    Map.ParseIntoArray(Inputs,TEXT(","),true);
    if(Inputs.IsEmpty() || Inputs.Num()>12)
    {
        UE_LOG(LogTerrainPCGBiome,Error,TEXT("BiomeMap只能包含1到12条真实图层映射。"));
        return 1;
    }
    TArray<FLayerGraphSpec> Specs;
    TSet<FString> Unique;
    for(const FString& Pair : Inputs)
    {
        FString Biome,Layer;
        if(!Pair.Split(TEXT(":"),&Biome,&Layer) ||
           !SafeToken(Biome) || !SafeToken(Layer) ||
           Unique.Contains(Biome))
        {
            UE_LOG(LogTerrainPCGBiome,Error,TEXT("Biome映射格式、重复或名称非法：%s"),*Pair);
            return 1;
        }
        Unique.Add(Biome);
        const FString Name=Prefix+Biome+TEXT("WeightFiltered");
        const FString Package=Root+TEXT("/")+Name;
        if(!SafeToken(Name) || !FPackageName::IsValidLongPackageName(Package))
            return 1;
        Specs.Add({Biome,FName(*Layer),Package});
    }
    // 任何目标被占用且不是当前合同对应的Graph，整个批次立即失败，不覆盖。
    for(const FLayerGraphSpec& Spec:Specs)
    {
        if(!FPackageName::DoesPackageExist(Spec.PackageName))
        {
            if(bValidateOnly)
            {
                UE_LOG(LogTerrainPCGBiome,Error,
                       TEXT("权重PCGGraph尚不存在：%s"),*Spec.PackageName);
                return 1;
            }
            continue;
        }
        const FString ObjectPath=Spec.PackageName+TEXT(".")+
            FPackageName::GetLongPackageAssetName(Spec.PackageName);
        UPCGGraph* Current=LoadObject<UPCGGraph>(nullptr,*ObjectPath);
        FString Error;
        if(!ValidateGraph(Current,Spec.LayerId,Threshold,Error))
        {
            UE_LOG(LogTerrainPCGBiome,Error,
                   TEXT("既有PCGGraph与地貌层合同不一致，拒绝覆盖：%s %s"),
                   *ObjectPath,*Error);
            return 1;
        }
    }

    int32 Created=0;
    for(const FLayerGraphSpec& Spec:Specs)
    {
        if(FPackageName::DoesPackageExist(Spec.PackageName))
        {
            UE_LOG(LogTerrainPCGBiome,Display,TEXT("TERRAIN_BIOME_FILTER_GRAPH_EXISTS %s"),
                   *Spec.PackageName);
            continue;
        }
        UPackage* Package=CreatePackage(*Spec.PackageName);
        if(!IsValid(Package)) return 1;
        const FName Name(*FPackageName::GetLongPackageAssetName(Spec.PackageName));
        UPCGGraph* Graph=NewObject<UPCGGraph>(
            Package,Name,RF_Public|RF_Standalone|RF_Transactional);
        if(!IsValid(Graph) || !IsValid(Graph->GetInputNode()) ||
           !IsValid(Graph->GetOutputNode()))
        {
            UE_LOG(LogTerrainPCGBiome,Error,TEXT("UE未创建有效原生PCGGraph：%s"),*Spec.PackageName);
            return 1;
        }
        Graph->bLandscapeUsesMetadata=true;
        UPCGGetLandscapeSettings* LandscapeSettings=nullptr;
        UPCGSurfaceSamplerSettings* SamplerSettings=nullptr;
        UPCGAttributeFilteringSettings* FilterSettings=nullptr;
        UPCGNode* Landscape=Graph->AddNodeOfType(LandscapeSettings);
        UPCGNode* Sampler=Graph->AddNodeOfType(SamplerSettings);
        UPCGNode* Filter=Graph->AddNodeOfType(FilterSettings);
        if(!IsValid(Landscape) || !IsValid(Sampler) || !IsValid(Filter) ||
           !IsValid(LandscapeSettings) || !IsValid(SamplerSettings) ||
           !IsValid(FilterSettings)) return 1;
        LandscapeSettings->bUnbounded=false;
        LandscapeSettings->SamplingProperties.bGetLayerWeights=true;
        LandscapeSettings->SamplingProperties.bGetHeightOnly=false;
        SamplerSettings->PointsPerSquaredMeter=.004f;
        SamplerSettings->bApplyDensityToPoints=true;
        FilterSettings->TargetAttribute.SetAttributeName(Spec.LayerId);
        FilterSettings->Operator=EPCGAttributeFilterOperator::GreaterOrEqual;
        FilterSettings->bUseConstantThreshold=true;
        FilterSettings->AttributeTypes.Type=EPCGMetadataTypes::Float;
        FilterSettings->AttributeTypes.FloatValue=Threshold;
        FilterSettings->bWarnOnDataMissingAttribute=true;

        struct FLink{UPCGNode* A;FName APin;UPCGNode* B;FName BPin;};
        const FLink Links[]={
            {Graph->GetInputNode(),TEXT("In"),Landscape,TEXT("BoundingShape")},
            {Landscape,TEXT("Out"),Sampler,TEXT("Surface")},
            {Graph->GetInputNode(),TEXT("In"),Sampler,TEXT("Bounding Shape")},
            {Sampler,TEXT("Out"),Filter,TEXT("In")},
            {Filter,TEXT("InsideFilter"),Graph->GetOutputNode(),TEXT("Out")}
        };
        for(const FLink& L:Links)
        {
            if(Graph->AddEdge(L.A,L.APin,L.B,L.BPin)!=L.B)
            {
                UE_LOG(LogTerrainPCGBiome,Error,TEXT("原生PCGGraph连接失败：%s"),*Spec.PackageName);
                return 1;
            }
        }
        FString Err;
        if(!ValidateGraph(Graph,Spec.LayerId,Threshold,Err))
        {
            UE_LOG(LogTerrainPCGBiome,Error,TEXT("新Graph原生回读不符：%s %s"),
                   *Spec.PackageName,*Err);
            return 1;
        }
        FAssetRegistryModule::AssetCreated(Graph);
        Package->MarkPackageDirty();
        const FString Filename=FPackageName::LongPackageNameToFilename(
            Spec.PackageName,FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags=RF_Public|RF_Standalone;
        Args.SaveFlags=SAVE_NoError;
        if(!UPackage::SavePackage(Package,Graph,*Filename,Args))
        {
            UE_LOG(LogTerrainPCGBiome,Error,
                   TEXT("真正UPCGGraph保存失败：%s"),*Spec.PackageName);
            return 1;
        }
        UE_LOG(LogTerrainPCGBiome,Display,
            TEXT("TERRAIN_BIOME_FILTER_GRAPH_CREATED graph=%s weight=%s threshold=%0.3f edges=5"),
            *Spec.PackageName,*Spec.LayerId.ToString(),Threshold);
        ++Created;
    }
    UE_LOG(LogTerrainPCGBiome,Display,
        TEXT("TERRAIN_BIOME_FILTER_GRAPHS_PASS count=%d created=%d mode=%s"),
        Specs.Num(),Created,bValidateOnly?TEXT("verify"):TEXT("apply"));
    return 0;
}
