// 平台Editor迁移桥：只做无覆盖的前置校验及原生AssetTools调用；依赖规划、审核和回退归调用方。
#include "Migration/GamePlatformAssetMigrationLibrary.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"

bool UGamePlatformAssetMigrationLibrary::CopyPackagesWithReferences(
    const TMap<FString, FString>& PackageDestinations, const FString& DestinationRoot, FString& OutError)
{
    OutError.Reset();
    FString RootFilename;
    if (!IsInGameThread() || PackageDestinations.IsEmpty() ||
        DestinationRoot.StartsWith(TEXT("/Engine/")) || DestinationRoot.StartsWith(TEXT("/Script/")) ||
        !FPackageName::IsValidLongPackageName(DestinationRoot) ||
        !FPackageName::TryConvertLongPackageNameToFilename(DestinationRoot, RootFilename))
    {
        OutError = TEXT("迁移要求游戏线程、非空完整映射和已挂载的项目目标目录。");
        return false;
    }
    const FString RootPrefix = DestinationRoot.EndsWith(TEXT("/")) ? DestinationRoot : DestinationRoot + TEXT("/");
    TSet<FString> Destinations;
    for (const auto& Pair : PackageDestinations)
    {
        // 先验证整批再复制，避免晚发现名称冲突而覆盖原有内容；路径和包名统一由引擎判断。
        if (!FPackageName::IsValidLongPackageName(Pair.Key) || !FPackageName::DoesPackageExist(Pair.Key) ||
            !FPackageName::IsValidLongPackageName(Pair.Value) || !Pair.Value.StartsWith(RootPrefix) ||
            FPackageName::DoesPackageExist(Pair.Value) || Destinations.Contains(Pair.Value.ToLower()))
        {
            OutError = TEXT("源包缺失、目标越界/已存在或目标名称冲突：") + Pair.Key + TEXT(" -> ") + Pair.Value;
            return false;
        }
        Destinations.Add(Pair.Value.ToLower());
    }
    // 使用唯一原生重映射实现，避免逐个DuplicateAsset留下指向外部源插件的引用。
    if (!IAssetTools::Get().AdvancedCopyPackages(PackageDestinations, true, false))
    {
        OutError = TEXT("引擎AdvancedCopyPackages失败；保留部分产物和日志供调用方审计。");
        return false;
    }
    return true;
}
