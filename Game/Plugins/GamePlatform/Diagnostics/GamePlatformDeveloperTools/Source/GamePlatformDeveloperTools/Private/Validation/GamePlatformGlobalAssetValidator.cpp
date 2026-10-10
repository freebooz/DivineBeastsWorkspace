#include "Validation/GamePlatformGlobalAssetValidator.h"
#include "Definitions/GamePlatformDefinitionBase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/UnrealType.h"

namespace
{
    bool IsProjectOwnedPackage(const FString& Package)
    {
        return Package.StartsWith(TEXT("/Game/"))
            || Package.StartsWith(TEXT("/GamePlatform"))
            || Package.StartsWith(TEXT("/DBA")) || Package.StartsWith(TEXT("/MobaPresentation/"));
    }

    FGamePlatformValidationResult MakeGlobalAssetResult(
        FName RuleId,
        const FString& Target,
        EGamePlatformValidationStatus Status,
        const FString& Message,
        const FString& Evidence = FString())
    {
        FGamePlatformValidationResult Result;
        Result.RuleId = RuleId;
        Result.Target = Target;
        Result.Status = Status;
        Result.Message = Message;
        Result.Evidence = Evidence;
        return Result;
    }
}

void FGamePlatformGlobalAssetValidator::ValidateProject(
    TArray<FGamePlatformValidationResult>& OutResults)
{
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = RegistryModule.Get();

    TArray<FAssetData> AllAssets;
    Registry.GetAllAssets(AllAssets, true);

    TArray<FAssetData> ProjectAssets;
    ProjectAssets.Reserve(AllAssets.Num());
    for (const FAssetData& Asset : AllAssets)
    {
        if (IsProjectOwnedPackage(Asset.PackageName.ToString()))
        {
            ProjectAssets.Add(Asset);
        }
    }

    ValidateDefinitionsAndStableIds(ProjectAssets, OutResults);
    ValidatePrimaryAssetsChunksAndReferences(ProjectAssets, OutResults);
}

void FGamePlatformGlobalAssetValidator::ValidateDefinitionsAndStableIds(
    const TArray<FAssetData>& ProjectAssets,
    TArray<FGamePlatformValidationResult>& OutResults)
{
    // 编辑器游戏线程一次获取继承集合，筛选元数据后才加载对象；类/资产重命名不改变定义资格。
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    const FTopLevelAssetPath BasePath = UGamePlatformDefinitionBase::StaticClass()->GetClassPathName();
    TSet<FTopLevelAssetPath> DefinitionClasses;
    Registry.GetDerivedClassNames({ BasePath }, {}, DefinitionClasses);
    DefinitionClasses.Add(BasePath);
    TMap<FString, FString> DefinitionIdToAsset;
    TMap<FString, TArray<FString>> DefinitionEdges;
    TArray<FString> Failures;
    bool bHasDuplicateIdentity = false;
    for (const FAssetData& Asset : ProjectAssets)
    {
        if (!DefinitionClasses.Contains(Asset.AssetClassPath)) continue;
        const auto* Definition = Cast<UGamePlatformDefinitionBase>(Asset.GetAsset());
        if (!Definition)
        {
            Failures.Add(FString::Printf(TEXT("%s：定义类元数据与实际对象不一致或无法加载。"), *Asset.PackageName.ToString()));
            continue;
        }
        const FGamePlatformResult Validation = Definition->ValidateDefinition();
        const FPrimaryAssetId PrimaryId = Definition->GetPrimaryAssetId();
        if (!Validation.IsSuccess() || !PrimaryId.IsValid())
        {
            Failures.Add(FString::Printf(TEXT("%s：定义身份/版本/字段无效：%s。"), *Asset.PackageName.ToString(), *Validation.Message));
            continue;
        }
        // 当前平台真源是LogicalId→PrimaryAssetId；其他字段中的Role/Item等引用并不是其唯一所有者。
        const FString Id = PrimaryId.ToString();
        if (const FString* Previous = DefinitionIdToAsset.Find(Id))
        {
            bHasDuplicateIdentity = true;
            Failures.Add(FString::Printf(TEXT("重复Stable ID：%s，资产=%s 与 %s。"), *Id, **Previous, *Asset.PackageName.ToString()));
            continue; // 已经明确失败，不能覆盖首项或按扫描顺序选择合法定义。
        }
        DefinitionIdToAsset.Add(Id, Asset.PackageName.ToString());
        auto& Edges = DefinitionEdges.FindOrAdd(Id);
        for (const FPrimaryAssetId& RequiredId : Definition->RequiredDefinitions) Edges.AddUnique(RequiredId.ToString());
    }
    for (const auto& Pair : DefinitionEdges)
        for (const FString& RequiredId : Pair.Value)
            if (!DefinitionIdToAsset.Contains(RequiredId))
                Failures.Add(FString::Printf(TEXT("%s -> %s：必需定义未在本次项目定义视图中完整通过校验。"), *Pair.Key, *RequiredId));

    // 显式DFS栈按已扫描定义数有界，不以未限定的C++递归处理资产图；每条边只访问一次。
    struct FVisitFrame { FString Id; int32 NextEdgeIndex = 0; };
    TMap<FString, uint8> VisitState;
    FString CycleEvidence;
    for (const auto& Root : DefinitionIdToAsset)
    {
        if (VisitState.FindRef(Root.Key) != 0) continue;
        TArray<FVisitFrame> Stack;
        Stack.Add({ Root.Key, 0 });
        VisitState.Add(Root.Key, 1);
        while (!Stack.IsEmpty() && CycleEvidence.IsEmpty())
        {
            auto& Frame = Stack.Last();
            const auto* Edges = DefinitionEdges.Find(Frame.Id);
            if (!Edges || Frame.NextEdgeIndex >= Edges->Num())
            {
                VisitState.Add(Frame.Id, 2);
                Stack.Pop();
                continue;
            }
            const FString Next = (*Edges)[Frame.NextEdgeIndex++];
            if (!DefinitionIdToAsset.Contains(Next)) continue;
            const uint8 State = VisitState.FindRef(Next);
            if (State == 0)
            {
                VisitState.Add(Next, 1);
                Stack.Add({ Next, 0 });
            }
            else if (State == 1)
            {
                TArray<FString> Cycle;
                bool bInCycle = false;
                for (const auto& Item : Stack)
                { bInCycle |= Item.Id == Next; if (bInCycle) Cycle.Add(Item.Id); }
                Cycle.Add(Next);
                CycleEvidence = FString::Join(Cycle, TEXT(" -> "));
            }
        }
        if (!CycleEvidence.IsEmpty()) { Failures.Add(TEXT("定义依赖循环：") + CycleEvidence); break; }
    }
    OutResults.Add(MakeGlobalAssetResult(TEXT("GP.Definition"), TEXT("DefinitionGraph"),
        Failures.IsEmpty() ? EGamePlatformValidationStatus::Passed : EGamePlatformValidationStatus::Failed,
        Failures.IsEmpty() ? TEXT("真实定义基类/LogicalId/PrimaryAssetId/版本/必需依赖/循环聚合检查通过。") : TEXT("定义聚合检查发现问题。"),
        FString::Join(Failures, TEXT("; "))));
    OutResults.Add(MakeGlobalAssetResult(TEXT("GP.StableId"), TEXT("StableIdIndex"),
        bHasDuplicateIdentity ? EGamePlatformValidationStatus::Failed : EGamePlatformValidationStatus::Passed,
        TEXT("只索引定义自己拥有的主资产身份，不将重复引用误报为多个所有者。"),
        FString::Printf(TEXT("indexed=%d"), DefinitionIdToAsset.Num())));
}
void FGamePlatformGlobalAssetValidator::ValidatePrimaryAssetsChunksAndReferences(
    const TArray<FAssetData>& ProjectAssets,
    TArray<FGamePlatformValidationResult>& OutResults)
{
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = RegistryModule.Get();

    TMap<FString, FString> PrimaryIdOwners;
    TArray<FString> ContentFailures;
    int32 DependencyEdges = 0;

    for (const FAssetData& Asset : ProjectAssets)
    {
        FString PrimaryType;
        FString PrimaryName;
        Asset.GetTagValue(FPrimaryAssetId::PrimaryAssetTypeTag, PrimaryType);
        Asset.GetTagValue(FPrimaryAssetId::PrimaryAssetNameTag, PrimaryName);

        if (!PrimaryType.IsEmpty() && !PrimaryName.IsEmpty())
        {
            const FString PrimaryKey = PrimaryType + TEXT(":") + PrimaryName;
            if (const FString* Existing = PrimaryIdOwners.Find(PrimaryKey))
            {
                ContentFailures.Add(FString::Printf(
                    TEXT("重复PrimaryAssetId：%s，资产=%s 与 %s。"),
                    *PrimaryKey,
                    **Existing,
                    *Asset.PackageName.ToString()));
            }
            else
            {
                PrimaryIdOwners.Add(PrimaryKey, Asset.PackageName.ToString());
            }
        }

        for (const int32 ChunkId : Asset.GetChunkIDs())
        {
            if (ChunkId < 0)
            {
                ContentFailures.Add(FString::Printf(
                    TEXT("%s：非法ChunkId=%d。"),
                    *Asset.PackageName.ToString(),
                    ChunkId));
            }
        }

        TArray<FName> Dependencies;
        Registry.GetDependencies(
            Asset.PackageName,
            Dependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::FDependencyQuery());

        DependencyEdges += Dependencies.Num();

        for (const FName Dependency : Dependencies)
        {
            const FString DependencyText = Dependency.ToString();
            if (!IsProjectOwnedPackage(DependencyText))
            {
                continue;
            }

            if (!Registry.DoesPackageExistOnDisk(Dependency, nullptr, nullptr))
            {
                ContentFailures.Add(FString::Printf(
                    TEXT("%s -> %s：项目资产依赖在磁盘上不存在。"),
                    *Asset.PackageName.ToString(),
                    *DependencyText));
            }
        }
    }

    OutResults.Add(MakeGlobalAssetResult(
        TEXT("GP.Content"),
        TEXT("ProjectAssetRegistry"),
        ContentFailures.IsEmpty()
            ? EGamePlatformValidationStatus::Passed
            : EGamePlatformValidationStatus::Failed,
        ContentFailures.IsEmpty()
            ? TEXT("PrimaryAssetId重复、Chunk基础合法性与项目内断裂依赖检查通过。")
            : TEXT("项目级内容/资产聚合检查发现问题。"),
        ContentFailures.IsEmpty()
            ? FString::Printf(
                TEXT("assets=%d primaryIds=%d dependencyEdges=%d"),
                ProjectAssets.Num(),
                PrimaryIdOwners.Num(),
                DependencyEdges)
            : FString::Join(ContentFailures, TEXT("; "))));
}
