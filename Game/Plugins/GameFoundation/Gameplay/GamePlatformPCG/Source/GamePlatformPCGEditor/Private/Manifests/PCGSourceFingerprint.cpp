#include "Manifests/PCGSourceFingerprint.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/Archive.h"
#include "UObject/Package.h"

FString GamePlatformPCGEditor::HashRecords(TArray<FString> Records)
{
    Records.Sort();
    FString Canonical;
    for (const FString& Record : Records)
    {
        Canonical += FString::Printf(TEXT("%d:"), Record.Len()) + Record + TEXT("\n");
    }
    const FTCHARToUTF8 Bytes(*Canonical);
    uint8 Digest[FSHA1::DigestSize];
    FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Digest);
    return TEXT("sha1-pcg-source-v1:") + BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
}

namespace
{
bool AppendFile(const FString& Filename, const FString& Identity, TArray<FString>& Records, FString& Error)
{
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Filename));
    if (!Reader || Reader->TotalSize() < 0)
    {
        Error = TEXT("来源依赖不可读取：") + Filename;
        return false;
    }
    const int64 Size = Reader->TotalSize();
    FSHA1 Hash;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(1024 * 1024);
    int64 Remaining = Size;
    while (Remaining > 0)
    {
        const uint32 Count = static_cast<uint32>(FMath::Min<int64>(Remaining, Buffer.Num()));
        Reader->Serialize(Buffer.GetData(), Count);
        if (Reader->IsError()) { Error = TEXT("依赖字节读取失败：") + Filename; return false; }
        Hash.Update(Buffer.GetData(), Count);
        Remaining -= Count;
    }
    Hash.Final();
    uint8 Digest[FSHA1::DigestSize];
    Hash.GetHash(Digest);
    Records.Add(Identity + FString::Printf(TEXT("|%lld|sha1:"), Size) + BytesToHex(Digest, UE_ARRAY_COUNT(Digest)));
    return true;
}
}

bool GamePlatformPCGEditor::CalculateSourceFingerprint(const UGamePlatformPCGProfileDefinition& Profile,
    FString& Fingerprint, TArray<FString>& Dependencies, FString& Error)
{
    check(IsInGameThread());
    Fingerprint.Reset(); Dependencies.Reset(); Error.Reset();
    TArray<FString> Records{TEXT("engine:") + FEngineVersion::Current().ToString(), TEXT("generator:GamePlatformPCGEditor/0.1.0")};
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FName> Pending{Profile.GetOutermost()->GetFName(),
        FName(*Profile.GraphReference.ToSoftObjectPath().GetLongPackageName()),
        FName(*Profile.OutputMesh.ToSoftObjectPath().GetLongPackageName())};
    TSet<FName> Visited;
    while (!Pending.IsEmpty())
    {
        const FName PackageName = Pending.Pop(EAllowShrinking::No);
        if (Visited.Contains(PackageName)) { continue; }
        Visited.Add(PackageName);
        if (Visited.Num() > 4096) { Error = TEXT("来源依赖超出工具4096包审查上限"); return false; }
        const FString PackageText = PackageName.ToString();
        if (PackageText.StartsWith(TEXT("/Script/")))
        {
            Records.Add(TEXT("native-package:") + PackageText);
            continue;
        }
        if (UPackage* Loaded = FindPackage(nullptr, *PackageText); Loaded && Loaded->IsDirty())
        {
            Error = TEXT("依赖有未保存编辑，拒绝用旧磁盘字节生成指纹：") + PackageText;
            return false;
        }
        FString Filename;
        if (!FPackageName::DoesPackageExist(PackageText, &Filename) || !AppendFile(Filename, PackageText, Records, Error))
        {
            if (Error.IsEmpty()) { Error = TEXT("来源包缺失：") + PackageText; }
            return false;
        }
        const FString Stem = FPaths::ChangeExtension(Filename, TEXT(""));
        for (const TCHAR* Extension : {TEXT("uexp"), TEXT("ubulk"), TEXT("uptnl"), TEXT("m.ubulk")})
        {
            const FString Sidecar = Stem + TEXT(".") + Extension;
            if (IFileManager::Get().FileExists(*Sidecar) && !AppendFile(Sidecar, PackageText + TEXT(".") + Extension, Records, Error))
            { return false; }
        }
        Registry.ScanFilesSynchronous({Filename}, true);
        TArray<FName> Children;
        if (!Registry.GetDependencies(PackageName, Children, UE::AssetRegistry::EDependencyCategory::Package))
        {
            Error = TEXT("无法取得完整包依赖，拒绝省略：") + PackageText;
            return false;
        }
        Pending.Append(Children);
    }
    if (!AppendFile(FPaths::EngineDir() / TEXT("Build/Build.version"), TEXT("engine:Build.version"), Records, Error)) { return false; }
    for (const TCHAR* PluginName : {TEXT("PCG"), TEXT("GamePlatformPCG")})
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
        if (!Plugin.IsValid()) { Error = TEXT("来源插件未发现：") + FString(PluginName); return false; }
        if (!AppendFile(Plugin->GetDescriptorFileName(), FString(PluginName) + TEXT("/.uplugin"), Records, Error)) { return false; }
        TArray<FString> SourceFiles;
        const FString SourceRoot = Plugin->GetBaseDir() / TEXT("Source");
        IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*"), true, false);
        if (SourceFiles.IsEmpty()) { Error = TEXT("缺少源码，当前来源算法不支持二进制插件：") + FString(PluginName); return false; }
        for (const FString& SourceFile : SourceFiles)
        {
            const FString Extension = FPaths::GetExtension(SourceFile);
            if (Extension != TEXT("h") && Extension != TEXT("cpp") && Extension != TEXT("cs")) { continue; }
            FString Relative = SourceFile;
            FPaths::MakePathRelativeTo(Relative, *(Plugin->GetBaseDir() + TEXT("/")));
            FPaths::NormalizeFilename(Relative);
            if (!AppendFile(SourceFile, FString(PluginName) + TEXT("/") + Relative, Records, Error)) { return false; }
        }
    }
    Records.Sort();
    Fingerprint = HashRecords(Records);
    Dependencies = MoveTemp(Records);
    return true;
}
