// GamePlatformSurfaceEditor：Gaea兼容原生数据生成UE Landscape审核地图。
// 平台只读取受控外部R16/R8和通用LayerInfo/材质路径，绝不认识神兽联盟地貌身份。
// 所有Import仅在显式命令和开发地图下执行；高度、权重及资产校验失败立即拒绝。
#include "Commands/GamePlatformSurfaceTerrainReviewMapCommandlet.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/Level.h"
#include "GameFramework/PlayerStart.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceTerrainImport, Log, All);

UGamePlatformSurfaceTerrainReviewMapCommandlet::UGamePlatformSurfaceTerrainReviewMapCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

namespace
{
    constexpr int32 Samples=1009;
    constexpr int32 Quads=Samples-1;
    constexpr int32 NumSubsections=2;
    constexpr int32 SubsectionSizeQuads=63;
    constexpr int32 ExpectedComponents=8;
    constexpr int32 ExpectedLayerCount=10;
    constexpr int64 ExpectedVertices=int64(Samples)*Samples;

    bool LoadSizeChecked(const FString& Filename, int64 ExpectedBytes,
                         TArray<uint8>& Out, FString& OutError)
    {
        if (!FPaths::FileExists(Filename) ||
            !FFileHelper::LoadFileToArray(Out, *Filename) ||
            Out.Num() != ExpectedBytes)
        {
            OutError=FString::Printf(
                TEXT("Landscape源数据缺失/长度不符，要求%lld字节：%s"),
                static_cast<long long>(ExpectedBytes), *Filename);
            return false;
        }
        return true;
    }
    bool IsSafeLayerName(const FString& Name)
    {
        if (Name.IsEmpty() || Name.Len()>48) return false;
        for(const TCHAR Ch : Name)
        {
            if(!FChar::IsAlnum(Ch) && Ch != TEXT('_')) return false;
        }
        return true;
    }
}

int32 UGamePlatformSurfaceTerrainReviewMapCommandlet::Main(const FString& Params)
{
    check(IsInGameThread());
    const bool bVerify=FParse::Param(*Params,TEXT("ValidateOnly"));
    FString MapPackage, RawRoot, LayerRoot, LayerIds, LayerAssetPrefix, LayerNamePrefix, MaterialPath;
    if (!FParse::Value(*Params,TEXT("MapPackage="),MapPackage) ||
        !FParse::Value(*Params,TEXT("RawRoot="),RawRoot) ||
        !FParse::Value(*Params,TEXT("LayerRoot="),LayerRoot) ||
        !FParse::Value(*Params,TEXT("LayerIds="),LayerIds) ||
        !FParse::Value(*Params,TEXT("MaterialPath="),MaterialPath))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,
            TEXT("参数缺失，必须显式提供MapPackage/RawRoot/LayerRoot/LayerIds/MaterialPath。"));
        return 1;
    }
    if (!FParse::Value(*Params,TEXT("LayerAssetPrefix="),LayerAssetPrefix))
        LayerAssetPrefix=TEXT("LI_");
    if (!FParse::Value(*Params,TEXT("LayerNamePrefix="),LayerNamePrefix))
        LayerNamePrefix=TEXT("LS_");

    RawRoot=FPaths::ConvertRelativePathToFull(RawRoot);
    LayerRoot.RemoveFromEnd(TEXT("/"));
    if (!MapPackage.StartsWith(TEXT("/Game/Development/Terrain/Maps/")) ||
        !FPackageName::IsValidLongPackageName(MapPackage) ||
        !FPackageName::IsValidLongPackageName(LayerRoot+TEXT("/_SafeCheck")) ||
        MaterialPath.IsEmpty() ||
        Samples-1 != ExpectedComponents*NumSubsections*SubsectionSizeQuads)
    {
        UE_LOG(LogSurfaceTerrainImport,Error,
            TEXT("拒绝修改正式地图、非法资源路径或不兼容Landscape组件网格。"));
        return 1;
    }
    const FString Filename=FPackageName::LongPackageNameToFilename(
        MapPackage,FPackageName::GetMapPackageExtension());
    if (bVerify)
    {
        if (!FPackageName::DoesPackageExist(MapPackage) || !GEditor)
        {
            UE_LOG(LogSurfaceTerrainImport,Error,TEXT("验证模式缺少已保存UWorld或Editor上下文。"));
            return 1;
        }
        UWorld* World=UEditorLoadingAndSavingUtils::LoadMap(Filename);
        if(!IsValid(World))
        {
            UE_LOG(LogSurfaceTerrainImport,Error,TEXT("真实审查地图回读失败：%s"),*MapPackage);
            return 1;
        }
        int32 Landscapes=0;
        for(AActor* Actor : World->PersistentLevel->Actors)
        {
            if (IsValid(Cast<ALandscape>(Actor))) ++Landscapes;
        }
        if(Landscapes!=1)
        {
            UE_LOG(LogSurfaceTerrainImport,Error,
                   TEXT("审查地图Landscape数量不是1：%d"),Landscapes);
            return 1;
        }
        UE_LOG(LogSurfaceTerrainImport,Display,
               TEXT("TERRAIN_LANDSCAPE_REVIEW_UE_VERIFY_PASS map=%s landscape=1"),
               *MapPackage);
        return 0;
    }
    if(FPackageName::DoesPackageExist(MapPackage) ||
       IFileManager::Get().FileExists(*Filename))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("目标技术地图已存在，严禁覆盖：%s"),*MapPackage);
        return 1;
    }

    TArray<FString> Names;
    LayerIds.ParseIntoArray(Names,TEXT(","),true);
    if(Names.Num()!=ExpectedLayerCount)
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("初版要求恰好10个已批准权重层。"));
        return 1;
    }
    UMaterialInterface* Master=LoadObject<UMaterialInterface>(nullptr,*MaterialPath);
    if (!IsValid(Master))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("真实UE材质未被加载：%s"),*MaterialPath);
        return 1;
    }
    // 先读取全部原始数据和LayerInfo并校验逐像素总权重，未通过前不创建UWorld。
    TArray<uint8> HeightBytes;
    FString Error;
    if (!LoadSizeChecked(FPaths::Combine(RawRoot,TEXT("Height_1009_LE.r16")),
                         ExpectedVertices*2,HeightBytes,Error))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("%s"),*Error);
        return 1;
    }
    TMap<FGuid,TArray<uint16>> HeightData;
    TArray<uint16>& RawHeights=HeightData.FindOrAdd(FGuid());
    RawHeights.SetNumUninitialized(ExpectedVertices);
    TArray<FLandscapeImportLayerInfo> Layers;
    Layers.Reserve(Names.Num());
    TSet<FString> Unique;
    TArray<TArray<uint8>> Sources;
    Sources.Reserve(Names.Num());
    for(const FString& Raw : Names)
    {
        const FString Name=Raw.TrimStartAndEnd();
        if(!IsSafeLayerName(Name) || Unique.Contains(Name))
        {
            UE_LOG(LogSurfaceTerrainImport,Error,TEXT("不允许重复/非法地表层：%s"),*Name);
            return 1;
        }
        Unique.Add(Name);
        const FString AssetName=LayerAssetPrefix+Name;
        const FString PackageName=LayerRoot+TEXT("/")+AssetName;
        const FString ObjectPath=PackageName+TEXT(".")+AssetName;
        ULandscapeLayerInfoObject* Info=LoadObject<ULandscapeLayerInfoObject>(nullptr,*ObjectPath);
        const FName LayerId(*(LayerNamePrefix+Name));
        if(!IsValid(Info) || Info->GetLayerName()!=LayerId ||
           Info->GetBlendMethod()!=ELandscapeTargetLayerBlendMethod::FinalWeightBlending)
        {
            UE_LOG(LogSurfaceTerrainImport,Error,
                   TEXT("地形LayerInfo资产缺失/名称/混合方式不符：%s"),*ObjectPath);
            return 1;
        }
        TArray<uint8> Bytes;
        if(!LoadSizeChecked(FPaths::Combine(RawRoot,TEXT("Weights"),
                            LayerNamePrefix+Name+TEXT(".r8")),
                            ExpectedVertices,Bytes,Error))
        {
            UE_LOG(LogSurfaceTerrainImport,Error,TEXT("%s"),*Error);
            return 1;
        }
        Sources.Add(MoveTemp(Bytes));
        FLandscapeImportLayerInfo Item(LayerId);
        Item.LayerInfo=Info;
        Layers.Add(MoveTemp(Item));
    }
    // Landscape中的+Y与PNG顶部行相反，因此这里明确翻转源行；
    // 不依赖Gaea导入UI中的默认FlipYAxis选择，防止PCG与材质位置错位。
    const uint16* InHeights=reinterpret_cast<const uint16*>(HeightBytes.GetData());
    const int32 Side=Samples;
    for(int32 Y=0;Y<Side;++Y)
    {
        const int32 SrcY=Side-1-Y;
        for(int32 X=0;X<Side;++X)
        {
            const int32 DestIndex=Y*Side+X;
            const int32 SrcIndex=SrcY*Side+X;
            RawHeights[DestIndex]=InHeights[SrcIndex];
            int32 Total=0;
            for(int32 LayerIndex=0;LayerIndex<Layers.Num();++LayerIndex)
            {
                Total+=Sources[LayerIndex][SrcIndex];
            }
            if(Total!=255)
            {
                UE_LOG(LogSurfaceTerrainImport,Error,
                    TEXT("权重总量不为255 at(%d,%d), sum=%d"),X,Y,Total);
                return 1;
            }
        }
    }
    for(int32 I=0;I<Layers.Num();++I)
    {
        Layers[I].LayerData.SetNumUninitialized(ExpectedVertices);
        for(int32 Y=0;Y<Side;++Y)
        {
            FMemory::Memcpy(Layers[I].LayerData.GetData()+int64(Y)*Side,
                            Sources[I].GetData()+int64(Side-1-Y)*Side,Side);
        }
    }
    TMap<FGuid,TArray<FLandscapeImportLayerInfo>> MaterialLayers;
    MaterialLayers.Add(FGuid(),MoveTemp(Layers));

    if(!GEditor)
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("无真正GEditor上下文，拒绝伪造UMAP。"));
        return 1;
    }
    UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if(!IsValid(World) || !IsValid(World->PersistentLevel))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("UE5.8未能创建真实Editor World。"));
        return 1;
    }
    ALandscape* Landscape=World->SpawnActor<ALandscape>(
        FVector(-50400,-50400,5000),FRotator::ZeroRotator);
    if(!IsValid(Landscape))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("未能生成真实ALandscape。"));
        return 1;
    }
    Landscape->SetActorScale3D(FVector(100,100,30000.0/512.0));
    Landscape->LandscapeMaterial=Master;
    Landscape->bUseDynamicMaterialInstance=true;
    Landscape->StaticLightingLOD=0;
    Landscape->Import(FGuid::NewGuid(),0,0,Side-1,Side-1,
                      NumSubsections,SubsectionSizeQuads,HeightData,TEXT(""),
                      MaterialLayers,ELandscapeImportAlphamapType::Additive,
                      TArrayView<const FLandscapeLayer>());
    ULandscapeInfo* Info=Landscape->GetLandscapeInfo();
    if (!IsValid(Info) || Landscape->LandscapeComponents.IsEmpty())
    {
        UE_LOG(LogSurfaceTerrainImport,Error,TEXT("ALandscape::Import未生成组件。"));
        return 1;
    }
    Info->UpdateLayerInfoMap(Landscape);
    for (const FString& Name : Names)
    {
        const FString AssetName=LayerAssetPrefix+Name;
        const FString ObjectPath=LayerRoot+TEXT("/")+AssetName+TEXT(".")+AssetName;
        if(ULandscapeLayerInfoObject* Layer=LoadObject<ULandscapeLayerInfoObject>(nullptr,*ObjectPath))
        {
            Landscape->AddTargetLayer(Layer->GetLayerName(),FLandscapeTargetLayerSettings(Layer));
        }
    }
    Landscape->SetActorLabel(TEXT("TerrainCalibration_HeightR16_Weight10"));

    World->SpawnActor<ADirectionalLight>(FVector(0,0,1500),FRotator(-40,30,0));
    World->SpawnActor<ASkyLight>(FVector(0,0,1400),FRotator::ZeroRotator);
    World->SpawnActor<APlayerStart>(FVector(-15000,-12000,12000),FRotator::ZeroRotator);
    if(!UEditorLoadingAndSavingUtils::SaveMap(World,MapPackage))
    {
        UE_LOG(LogSurfaceTerrainImport,Error,
               TEXT("真实Landscape地图保存失败：%s"),*MapPackage);
        return 1;
    }
    UE_LOG(LogSurfaceTerrainImport,Display,
        TEXT("TERRAIN_LANDSCAPE_REVIEW_UE_SAVED map=%s landscape_components=%d "
             "base_layers=%d source_kind=calibration_only_not_gaea"),
        *MapPackage,Landscape->LandscapeComponents.Num(),Names.Num());
    return 0;
}
