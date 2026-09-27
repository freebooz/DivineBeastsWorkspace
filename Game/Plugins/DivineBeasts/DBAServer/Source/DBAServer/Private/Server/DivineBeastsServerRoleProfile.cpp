#include "Server/DivineBeastsServerRoleProfile.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    const TSet<FString>& AllowedTopLevelKeys()
    {
        static const TSet<FString> Keys = {
            TEXT("profileVersion"), TEXT("serverRoleId"),
            TEXT("defaultExperienceId"), TEXT("allowedExperienceIds"),
            TEXT("worldPackage"), TEXT("requiredAssets"),
            TEXT("arenaModeIds"), TEXT("instancePolicy"),
            TEXT("readiness")};
        return Keys;
    }

    bool HasOnlyKeys(const TSharedPtr<FJsonObject>& Object, const TSet<FString>& Allowed)
    {
        if (!Object.IsValid())
        {
            return false;
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
        {
            if (!Allowed.Contains(Entry.Key))
            {
                return false;
            }
        }
        return true;
    }

    bool ReadNameArray(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName,
        TArray<FName>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return false;
        }
        OutValues.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty())
            {
                return false;
            }
            OutValues.Add(FName(*Text));
        }
        return true;
    }

    bool ReadStringArray(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName,
        TArray<FSoftObjectPath>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return false;
        }
        OutValues.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) ||
                !FPackageName::IsValidObjectPath(Text))
            {
                return false;
            }
            OutValues.Emplace(Text);
        }
        return true;
    }

    bool HasDuplicates(const TArray<FName>& Values)
    {
        TSet<FName> UniqueValues;
        for (const FName Value : Values)
        {
            if (Value.IsNone() || UniqueValues.Contains(Value))
            {
                return true;
            }
            UniqueValues.Add(Value);
        }
        return false;
    }

    TArray<FName> ExpectedArenaModes()
    {
        return {
            FName(TEXT("Arena.Mode.Duel1v1")),
            FName(TEXT("Arena.Mode.Team2v2")),
            FName(TEXT("Arena.Mode.Team3v3")),
            FName(TEXT("Arena.Mode.Team4v4")),
            FName(TEXT("Arena.Mode.Team5v5"))};
    }
}

bool FDivineBeastsServerRoleProfile::TryLoadForRoleName(
    const FString& RoleName,
    FDivineBeastsServerRoleProfile& OutProfile,
    FString& OutError)
{
    const FName RoleId(*(FString(TEXT("GameServer.Role.")) + RoleName));
    if (!FDivineBeastsProjectCatalog::IsServerRoleId(RoleId))
    {
        OutError = TEXT("启动参数ServerRole不属于Shared正式角色目录。");
        return false;
    }

    FString ProfileRoot = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DBA_SERVER_PROFILE_ROOT"));
    if (ProfileRoot.IsEmpty())
    {
        // 源码工作区中Deploy与Game同级；打包环境可用环境变量指定只读Profile挂载点。
        ProfileRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Deploy/Server"));
    }
    ProfileRoot = FPaths::ConvertRelativePathToFull(ProfileRoot);
    FPaths::NormalizeDirectoryName(ProfileRoot);
    const FString FilePath = FPaths::Combine(
        ProfileRoot,
        RoleName,
        TEXT("server-profile.json"));
    return TryLoadFromFile(FilePath, OutProfile, OutError);
}

bool FDivineBeastsServerRoleProfile::TryLoadFromFile(
    const FString& FilePath,
    FDivineBeastsServerRoleProfile& OutProfile,
    FString& OutError)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
    {
        OutError = TEXT("服务器Profile文件缺失或不可读。");
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) ||
        !HasOnlyKeys(Root, AllowedTopLevelKeys()))
    {
        OutError = TEXT("服务器Profile不是有效对象或包含未批准字段。");
        return false;
    }

    FDivineBeastsServerRoleProfile Candidate;
    double Version = 0;
    FString RoleText;
    FString DefaultExperienceText;
    FString WorldPackageText;
    FString InstancePolicyText;
    if (!Root->TryGetNumberField(TEXT("profileVersion"), Version) ||
        !Root->TryGetStringField(TEXT("serverRoleId"), RoleText) ||
        !Root->TryGetStringField(TEXT("defaultExperienceId"), DefaultExperienceText) ||
        !Root->TryGetStringField(TEXT("worldPackage"), WorldPackageText) ||
        !Root->TryGetStringField(TEXT("instancePolicy"), InstancePolicyText) ||
        !ReadNameArray(Root, TEXT("allowedExperienceIds"), Candidate.AllowedExperienceIds) ||
        !ReadStringArray(Root, TEXT("requiredAssets"), Candidate.RequiredAssets) ||
        !ReadNameArray(Root, TEXT("arenaModeIds"), Candidate.ArenaModeIds))
    {
        OutError = TEXT("服务器Profile缺少必需字段或字段类型不正确。");
        return false;
    }

    const TSharedPtr<FJsonObject>* Readiness = nullptr;
    const TSet<FString> ReadinessKeys = {
        TEXT("requireRequiredAssets"),
        TEXT("requireWorldBeginPlay"),
        TEXT("requireControlPlaneRegistration")};
    if (!Root->TryGetObjectField(TEXT("readiness"), Readiness) ||
        Readiness == nullptr || !Readiness->IsValid() ||
        !HasOnlyKeys(*Readiness, ReadinessKeys))
    {
        OutError = TEXT("服务器Profile就绪策略缺失或包含未知字段。");
        return false;
    }

    if (!FMath::IsNearlyEqual(Version, 1.0))
    {
        OutError = TEXT("不支持的服务器Profile版本。");
        return false;
    }
    Candidate.ProfileVersion = static_cast<int32>(Version);
    Candidate.ServerRoleId = FName(*RoleText);
    Candidate.DefaultExperienceId = FName(*DefaultExperienceText);
    Candidate.WorldPackage = MoveTemp(WorldPackageText);
    Candidate.InstancePolicy = FName(*InstancePolicyText);
    if (!(*Readiness)->TryGetBoolField(TEXT("requireRequiredAssets"), Candidate.bRequireRequiredAssets) ||
        !(*Readiness)->TryGetBoolField(TEXT("requireWorldBeginPlay"), Candidate.bRequireWorldBeginPlay) ||
        !(*Readiness)->TryGetBoolField(TEXT("requireControlPlaneRegistration"), Candidate.bRequireControlPlaneRegistration) ||
        !Candidate.Validate(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("服务器Profile的就绪策略字段无效。");
        }
        return false;
    }

    OutProfile = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool FDivineBeastsServerRoleProfile::Validate(FString& OutError) const
{
    if (ProfileVersion != 1 ||
        !FDivineBeastsProjectCatalog::IsServerRoleId(ServerRoleId) ||
        !FDivineBeastsProjectCatalog::IsExperienceId(DefaultExperienceId) ||
        AllowedExperienceIds.IsEmpty() ||
        HasDuplicates(AllowedExperienceIds) ||
        HasDuplicates(ArenaModeIds) ||
        !WorldPackage.StartsWith(TEXT("/Game/")) ||
        !FPackageName::IsValidLongPackageName(WorldPackage) ||
        RequiredAssets.IsEmpty() || InstancePolicy.IsNone() ||
        !RequiredAssets.ContainsByPredicate(
            [this](const FSoftObjectPath& Path)
            {
                return Path.GetLongPackageName() == WorldPackage;
            }) ||
        !bRequireRequiredAssets || !bRequireWorldBeginPlay ||
        !bRequireControlPlaneRegistration)
    {
        OutError = TEXT("服务器Profile身份、资源或就绪策略未通过结构校验。");
        return false;
    }

    if (!AllowedExperienceIds.Contains(DefaultExperienceId))
    {
        OutError = TEXT("默认体验未包含在角色允许体验集合中。");
        return false;
    }
    for (const FName ExperienceId : AllowedExperienceIds)
    {
        FName MappedRole = NAME_None;
        if (!FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
                ExperienceId, MappedRole) || MappedRole != ServerRoleId)
        {
            OutError = TEXT("Profile体验与Shared目录中的服务器角色映射不一致。");
            return false;
        }
    }

    const bool bIsArena = ServerRoleId == FName(TEXT("GameServer.Role.MainArena"));
    TArray<FName> SortedModes = ArenaModeIds;
    TArray<FName> RequiredModes = ExpectedArenaModes();
    SortedModes.Sort(FNameLexicalLess());
    RequiredModes.Sort(FNameLexicalLess());
    const bool bAllArenaModesMatch = SortedModes.Num() == RequiredModes.Num() &&
        RequiredModes.ContainsByPredicate(
            [&SortedModes](FName Mode) { return SortedModes.Contains(Mode); });
    if ((bIsArena && !bAllArenaModesMatch) || (!bIsArena && !ArenaModeIds.IsEmpty()))
    {
        OutError = TEXT("只有MainArena可声明竞技模式，且必须完整覆盖1v1至5v5目录。");
        return false;
    }

    static const TMap<FName, FName> RequiredPolicyByRole = {
        {FName(TEXT("GameServer.Role.Village")), FName(TEXT("ExperienceInstance"))},
        {FName(TEXT("GameServer.Role.OpenWorld")), FName(TEXT("PersistentShardedWorld"))},
        {FName(TEXT("GameServer.Role.MainArena")), FName(TEXT("PerMatch"))}};
    const FName* RequiredPolicy = RequiredPolicyByRole.Find(ServerRoleId);
    if (RequiredPolicy == nullptr || *RequiredPolicy != InstancePolicy)
    {
        OutError = TEXT("Profile实例策略与服务器角色生命周期不匹配。");
        return false;
    }

    OutError.Reset();
    return true;
}

bool FDivineBeastsServerRoleProfile::FindMissingRequiredAssets(
    TArray<FString>& OutMissingAssets) const
{
    OutMissingAssets.Reset();
    for (const FSoftObjectPath& AssetPath : RequiredAssets)
    {
        const FString PackageName = AssetPath.GetLongPackageName();
        // Ready不只要求烘焙包可见；必要对象必须已加载，避免服务器登记后才发现无法使用。
        if (PackageName.IsEmpty() || !FPackageName::DoesPackageExist(PackageName) ||
            AssetPath.ResolveObject() == nullptr)
        {
            OutMissingAssets.Add(AssetPath.ToString());
        }
    }
    return OutMissingAssets.IsEmpty();
}
