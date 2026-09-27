#include "Validation/GamePlatformGlobalAssetValidator.h"

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
            || Package.StartsWith(TEXT("/DBA"));
    }

    bool IsDefinitionAsset(const FAssetData& Asset)
    {
        return Asset.AssetClassPath.GetAssetName().ToString().Contains(TEXT("Definition"))
            || Asset.AssetName.ToString().Contains(TEXT("Definition"));
    }

    bool IsGlobalAssetStableIdProperty(FName Name)
    {
        static const TSet<FName> Names =
        {
            TEXT("DefinitionId"),
            TEXT("ArenaModeId"),
            TEXT("ServerRole"),
            TEXT("ExperienceId"),
            TEXT("ItemDefinitionId"),
            TEXT("EntitlementId"),
            TEXT("ProgressionTrackId"),
            TEXT("QuestId"),
            TEXT("EquipmentSlotId")
        };
        return Names.Contains(Name);
    }

    FString ExportGlobalAssetPropertyValue(UObject* Object, FProperty* Property)
    {
        FString Value;
        if (!Object || !Property)
        {
            return Value;
        }

        const void* Address = Property->ContainerPtrToValuePtr<void>(Object);
        Property->ExportTextItem_Direct(Value, Address, nullptr, Object, PPF_None);
        Value.TrimStartAndEndInline();
        Value.RemoveFromStart(TEXT("\""));
        Value.RemoveFromEnd(TEXT("\""));
        return Value;
    }

    FString NormalizeRequirementToken(FString Value)
    {
        Value.TrimStartAndEndInline();
        Value.RemoveFromStart(TEXT("\""));
        Value.RemoveFromEnd(TEXT("\""));
        Value.RemoveFromStart(TEXT("'"));
        Value.RemoveFromEnd(TEXT("'"));
        Value.RemoveFromStart(TEXT("("));
        Value.RemoveFromEnd(TEXT(")"));
        Value.TrimStartAndEndInline();
        return Value;
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
    TMap<FString, FString> StableIdOwners;
    TMap<FString, FString> DefinitionIdToAsset;
    TMap<FString, TArray<FString>> DefinitionEdges;
    TArray<FString> Failures;

    for (const FAssetData& Asset : ProjectAssets)
    {
        if (!IsDefinitionAsset(Asset))
        {
            continue;
        }

        UObject* Object = Asset.GetAsset();
        if (!Object)
        {
            Failures.Add(FString::Printf(
                TEXT("%s：Definition资产无法加载。"),
                *Asset.PackageName.ToString()));
            continue;
        }

        FString DefinitionId;
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (!IsGlobalAssetStableIdProperty(Property->GetFName()))
            {
                continue;
            }

            const FString Value = ExportGlobalAssetPropertyValue(Object, Property);
            if (Value.IsEmpty())
            {
                continue;
            }

            const FString Key = Property->GetName() + TEXT("=") + Value;
            if (const FString* ExistingOwner = StableIdOwners.Find(Key))
            {
                Failures.Add(FString::Printf(
                    TEXT("重复Stable ID：%s，资产=%s 与 %s。"),
                    *Key,
                    **ExistingOwner,
                    *Asset.PackageName.ToString()));
            }
            else
            {
                StableIdOwners.Add(Key, Asset.PackageName.ToString());
            }

            if (Property->GetFName() == TEXT("DefinitionId"))
            {
                DefinitionId = Value;
            }
        }

        if (DefinitionId.IsEmpty())
        {
            Failures.Add(FString::Printf(
                TEXT("%s：缺少DefinitionId。"),
                *Asset.PackageName.ToString()));
            continue;
        }

        DefinitionIdToAsset.Add(DefinitionId, Asset.PackageName.ToString());

        const FPrimaryAssetId PrimaryId = Object->GetPrimaryAssetId();
        if (!PrimaryId.IsValid())
        {
            Failures.Add(FString::Printf(
                TEXT("%s：Definition未返回有效PrimaryAssetId。"),
                *Asset.PackageName.ToString()));
        }

        FProperty* RequiredProperty =
            Object->GetClass()->FindPropertyByName(TEXT("RequiredDefinitions"));
        FArrayProperty* RequiredArray = CastField<FArrayProperty>(RequiredProperty);
        if (!RequiredArray)
        {
            continue;
        }

        void* ArrayAddress = RequiredArray->ContainerPtrToValuePtr<void>(Object);
        FScriptArrayHelper Helper(RequiredArray, ArrayAddress);

        TArray<FString>& Edges = DefinitionEdges.FindOrAdd(DefinitionId);
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            FString RequiredText;
            RequiredArray->Inner->ExportTextItem_Direct(
                RequiredText,
                Helper.GetRawPtr(Index),
                nullptr,
                Object,
                PPF_None);

            RequiredText = NormalizeRequirementToken(RequiredText);
            if (!RequiredText.IsEmpty())
            {
                Edges.AddUnique(RequiredText);
            }
        }
    }

    for (const TPair<FString, TArray<FString>>& Pair : DefinitionEdges)
    {
        for (const FString& RequiredId : Pair.Value)
        {
            if (!DefinitionIdToAsset.Contains(RequiredId))
            {
                // 路径型RequiredDefinitions由Asset Registry引用检查负责；
                // 这里只对明确的稳定ID形式判缺失，避免把SoftObjectPath误报成ID。
                if (!RequiredId.Contains(TEXT("/"))
                    && !RequiredId.Contains(TEXT("."))
                    && !RequiredId.Contains(TEXT(":")))
                {
                    Failures.Add(FString::Printf(
                        TEXT("%s -> %s：RequiredDefinitions缺失。"),
                        *Pair.Key,
                        *RequiredId));
                }
            }
        }
    }

    TMap<FString, uint8> VisitState;
    TArray<FString> Stack;
    FString CycleEvidence;

    TFunction<bool(const FString&)> Visit = [&](const FString& Id)
    {
        VisitState.FindOrAdd(Id) = 1;
        Stack.Add(Id);

        for (const FString& Next : DefinitionEdges.FindRef(Id))
        {
            if (!DefinitionIdToAsset.Contains(Next))
            {
                continue;
            }

            const uint8 State = VisitState.FindRef(Next);
            if (State == 0)
            {
                if (Visit(Next))
                {
                    return true;
                }
            }
            else if (State == 1)
            {
                const int32 Start = Stack.Find(Next);
                TArray<FString> Cycle;
                if (Start != INDEX_NONE)
                {
                    for (int32 Index = Start; Index < Stack.Num(); ++Index)
                    {
                        Cycle.Add(Stack[Index]);
                    }
                }
                Cycle.Add(Next);
                CycleEvidence = FString::Join(Cycle, TEXT(" -> "));
                return true;
            }
        }

        Stack.Pop();
        VisitState.FindOrAdd(Id) = 2;
        return false;
    };

    for (const TPair<FString, FString>& Pair : DefinitionIdToAsset)
    {
        if (VisitState.FindRef(Pair.Key) == 0 && Visit(Pair.Key))
        {
            Failures.Add(TEXT("Definition依赖循环：") + CycleEvidence);
            break;
        }
    }

    OutResults.Add(MakeGlobalAssetResult(
        TEXT("GP.Definition"),
        TEXT("DefinitionGraph"),
        Failures.IsEmpty()
            ? EGamePlatformValidationStatus::Passed
            : EGamePlatformValidationStatus::Failed,
        Failures.IsEmpty()
            ? TEXT("DefinitionId/PrimaryAssetId/RequiredDefinitions/循环依赖聚合检查通过。")
            : TEXT("Definition聚合检查发现问题。"),
        FString::Join(Failures, TEXT("; "))));

    OutResults.Add(MakeGlobalAssetResult(
        TEXT("GP.StableId"),
        TEXT("StableIdIndex"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("Stable ID"));
        })
            ? EGamePlatformValidationStatus::Failed
            : EGamePlatformValidationStatus::Passed,
        TEXT("稳定ID重复索引检查完成。"),
        FString::Printf(TEXT("indexed=%d"), StableIdOwners.Num())));
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
