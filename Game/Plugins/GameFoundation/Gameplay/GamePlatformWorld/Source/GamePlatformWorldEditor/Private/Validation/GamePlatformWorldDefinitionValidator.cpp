#include "Validation/GamePlatformWorldDefinitionValidator.h"
#include "Definitions/GamePlatformWorldDefinition.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Validation/GamePlatformDefinitionValidation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 一次编辑器验证独占缓存及强引用；链式迭代避免深父树递归爆栈，不保存到进程全局。 */
class FWorldRegionGraphValidation
{
public:
    FWorldRegionGraphValidation(IAssetRegistry& InRegistry, UGamePlatformDefinitionBase& InCandidate)
        : Registry(InRegistry), Candidate(InCandidate) {}

    FGamePlatformResult Validate(const FPrimaryAssetId& Start)
    {
        FPrimaryAssetId Current = Start;
        TSet<FPrimaryAssetId> Chain;
        while (Current.IsValid())
        {
            if (Complete.Contains(Current)) { break; }
            if (Chain.Contains(Current))
            { return FGamePlatformResult::Failure(TEXT("RegionParentCycle"), TEXT("区域父链存在循环，不能确定区域层级。")); }
            if (Chain.Num() >= 128 || (!Resolved.Contains(Current) && Resolved.Num() >= 4096))
            { return FGamePlatformResult::Failure(TEXT("RegionParentGraphLimit"), TEXT("区域父图超过128层或4096个唯一节点安全上限。")); }
            Chain.Add(Current);
            UGamePlatformRegionDefinition* Region = nullptr;
            if (auto* Cached = Resolved.Find(Current)) { Region = *Cached; }
            else
            {
                FSoftObjectPath Source;
                const auto Unique = ResolveUniqueGamePlatformDefinitionSource(Current, Source, &Candidate);
                if (!Unique.IsSuccess()) { return Unique; }
                // 当前编辑对象可能尚未保存；其余对象必须来自已解析的唯一源，而非猜测资产路径。
                UObject* Object = Source == FSoftObjectPath(&Candidate) ? &Candidate : Registry.GetAssetByObjectPath(Source).GetAsset();
                Region = Cast<UGamePlatformRegionDefinition>(Object);
                if (!Region || Region->GetPrimaryAssetId() != Current)
                { return FGamePlatformResult::Failure(TEXT("RegionDefinitionTypeMismatch"), TEXT("区域列表或父身份未解析到同身份的真实RegionDefinition。")); }
                Pins.Emplace(Region);
                const auto Fields = Region->ValidateDefinition();
                if (!Fields.IsSuccess()) { return Fields; }
                Resolved.Add(Current, Region);
            }
            Current = Region->ParentRegionId.IsValid() ? FPrimaryAssetId(
                UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*Region->ParentRegionId.ToString())) : FPrimaryAssetId();
        }
        for (const auto& Id : Chain) { Complete.Add(Id); }
        return FGamePlatformResult::Success();
    }
private:
    IAssetRegistry& Registry;
    UGamePlatformDefinitionBase& Candidate;
    TMap<FPrimaryAssetId, UGamePlatformRegionDefinition*> Resolved;
    TArray<TStrongObjectPtr<UGamePlatformRegionDefinition>> Pins;
    TSet<FPrimaryAssetId> Complete;
};

FGamePlatformResult ValidateSavedWorldMap(const UGamePlatformWorldDefinition& Definition, IAssetRegistry& Registry)
{
    const FSoftObjectPath Path = Definition.MapIdentity.ToSoftObjectPath();
    if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
    { return FGamePlatformResult::Failure(TEXT("WorldMapMissing"), TEXT("世界定义引用的地图包不存在或尚未保存，不能作为可烘焙地图。")); }
    const FAssetData MapAsset = Registry.GetAssetByObjectPath(Path);
    if (!MapAsset.IsValid())
    { return FGamePlatformResult::Failure(TEXT("WorldMapObjectMissing"), TEXT("地图包存在，但指定顶层对象不在源资产注册表中。")); }
    if (MapAsset.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
    { return FGamePlatformResult::Failure(TEXT("WorldMapTypeMismatch"), TEXT("地图软引用解析到非UWorld源资产，不能把其他资源当作世界。")); }
    // 仅编辑器验证加载对象，不把打开地图/BeginPlay当成验证，不创建游戏实例或玩家。
    TStrongObjectPtr<UWorld> Map(Cast<UWorld>(MapAsset.GetAsset()));
    if (!Map.IsValid() || FSoftObjectPath(Map.Get()) != Path)
    { return FGamePlatformResult::Failure(TEXT("WorldMapLoadFailed"), TEXT("地图对象无法真实加载或身份发生偏移；先修复引用与重定向再验证。")); }
    return FGamePlatformResult::Success();
}

bool UGamePlatformWorldDefinitionValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject,
    FDataValidationContext&) const
{
    return InObject && (InObject->IsA<UGamePlatformWorldDefinition>() || InObject->IsA<UGamePlatformRegionDefinition>());
}

EDataValidationResult UGamePlatformWorldDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData&,
    UObject* InAsset, FDataValidationContext&)
{
    check(IsInGameThread());
    auto Fail = [this, InAsset](const FGamePlatformResult& Result)
    {
        AssetFails(InAsset, FText::FromString(Result.Code.ToString() + TEXT(": ") + Result.Message));
        return EDataValidationResult::Invalid;
    };
    auto* Root = Cast<UGamePlatformDefinitionBase>(InAsset);
    if (!Root) { return EDataValidationResult::NotValidated; }
    const auto Fields = Root->ValidateDefinition();
    if (!Fields.IsSuccess()) { return Fail(Fields); }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    Registry.WaitForCompletion();
    FSoftObjectPath UniqueSource;
    const auto Unique = ResolveUniqueGamePlatformDefinitionSource(Root->GetPrimaryAssetId(), UniqueSource, Root);
    if (!Unique.IsSuccess()) { return Fail(Unique); }
    FWorldRegionGraphValidation Regions(Registry, *Root);
    if (const auto* World = Cast<UGamePlatformWorldDefinition>(Root))
    {
        const auto MapResult = ValidateSavedWorldMap(*World, Registry);
        if (!MapResult.IsSuccess()) { return Fail(MapResult); }
        for (const auto& Id : World->Regions)
        {
            const auto Result = Regions.Validate(Id);
            if (!Result.IsSuccess()) { return Fail(Result); }
        }
    }
    else if (Root->IsA<UGamePlatformRegionDefinition>())
    {
        const auto Result = Regions.Validate(Root->GetPrimaryAssetId());
        if (!Result.IsSuccess()) { return Fail(Result); }
    }
    else { return EDataValidationResult::NotValidated; }
    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
