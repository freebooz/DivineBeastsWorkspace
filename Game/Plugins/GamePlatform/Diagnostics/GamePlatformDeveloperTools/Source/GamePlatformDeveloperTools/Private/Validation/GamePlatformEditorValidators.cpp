// 平台Editor逐资产验证入口：只读已加载资产及配置，报告命名/内容/身份等规则结果，不修改或保存资产。
#include "Validation/GamePlatformEditorValidators.h"
#include "Definitions/GamePlatformDefinitionBase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Settings/GamePlatformValidationSettings.h"
#include "UObject/UnrealType.h"
#include "Validation/GamePlatformValidationService.h"

namespace
{
    bool IsProjectAsset(const FAssetData& AssetData)
    {
        const FString Package = AssetData.PackageName.ToString();
        if (Package.StartsWith(TEXT("/Engine/")) || Package.StartsWith(TEXT("/Script/")))
        {
            return false;
        }

        // 平台插件与项目插件使用自身Mount Point；第三方插件不强制套用项目命名规范。
        return Package.StartsWith(TEXT("/Game/"))
            || Package.StartsWith(TEXT("/GamePlatform"))
            || Package.StartsWith(TEXT("/DBA"));
    }

    FString ExportPropertyValue(UObject* Object, FProperty* Property)
    {
        if (!Object || !Property)
        {
            return FString();
        }

        FString Value;
        const void* Address = Property->ContainerPtrToValuePtr<void>(Object);
        Property->ExportTextItem_Direct(Value, Address, nullptr, Object, PPF_None);
        Value.TrimStartAndEndInline();
        Value.RemoveFromStart(TEXT("\""));
        Value.RemoveFromEnd(TEXT("\""));
        return Value;
    }

    bool IsStableIdProperty(FName Name)
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

    bool HasValidStableIdCharacters(const FString& Value)
    {
        if (Value.IsEmpty())
        {
            return false;
        }

        for (TCHAR Ch : Value)
        {
            if (!(FChar::IsAlnum(Ch)
                || Ch == TEXT('.')
                || Ch == TEXT('_')
                || Ch == TEXT('-')
                || Ch == TEXT(':')
                || Ch == TEXT('/')))
            {
                return false;
            }
        }
        return true;
    }

    bool IsAllowlisted(FName RuleId, const FAssetData& AssetData)
    {
        return FGamePlatformValidationService::IsTargetAllowlisted(
            RuleId,
            AssetData.PackageName.ToString());
    }

    bool IsForbiddenServerDependency(const FString& PackagePath, const FString& ClassName)
    {
        bool bClientPath = false;
        const UGamePlatformValidationSettings* Settings = GetDefault<UGamePlatformValidationSettings>();
        for (const FString& Token : Settings->ForbiddenServerDependencyPathTokens)
        {
            if (!Token.IsEmpty() && PackagePath.Contains(Token))
            {
                bClientPath = true;
                break;
            }
        }

        const bool bPresentationClass =
            ClassName.Contains(TEXT("Niagara"))
            || ClassName.Contains(TEXT("Widget"))
            || ClassName.Contains(TEXT("Sound"))
            || ClassName.Contains(TEXT("LevelSequence"));

        return bClientPath || bPresentationClass;
    }
}

bool UGamePlatformNamingValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    return InObject != nullptr && IsProjectAsset(InAssetData);
}

EDataValidationResult UGamePlatformNamingValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    // UBlueprint资产类型不能代表技能/效果领域；使用已加载对象的真实ParentClass祖先与统一配置解析。
    const FString Prefix = GetDefault<UGamePlatformValidationSettings>()->ResolveAssetPrefix(InAsset);
    if (!Prefix.IsEmpty() && !InAssetData.AssetName.ToString().StartsWith(Prefix))
    {
        if (IsAllowlisted(TEXT("GP.Naming"), InAssetData))
        {
            AssetWarning(
                InAsset,
                FText::FromString(TEXT("GP.Naming：命名不合规，但命中有效Allowlist（临时豁免）。")));
            AssetPasses(InAsset);
            return EDataValidationResult::Valid;
        }

        AssetFails(
            InAsset,
            FText::FromString(FString::Printf(
                TEXT("GP.Naming：%s 应使用前缀 %s。"),
                *InAssetData.AssetName.ToString(),
                *Prefix)));
        return EDataValidationResult::Invalid;
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}

bool UGamePlatformContentValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    return InObject != nullptr && IsProjectAsset(InAssetData);
}

EDataValidationResult UGamePlatformContentValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    const FString ClassName = InAssetData.AssetClassPath.GetAssetName().ToString();
    const FString PackageName = InAssetData.PackageName.ToString();

    if (ClassName == TEXT("ObjectRedirector"))
    {
        if (IsAllowlisted(TEXT("GP.Content"), InAssetData))
        {
            AssetWarning(InAsset, FText::FromString(TEXT("GP.Content：Redirector命中有效Allowlist（临时豁免）。")));
            AssetPasses(InAsset);
            return EDataValidationResult::Valid;
        }

        AssetFails(InAsset, FText::FromString(TEXT("GP.Content：发现Redirector（重定向器），Release前必须清理。")));
        return EDataValidationResult::Invalid;
    }

    if (!FPackageName::IsValidLongPackageName(PackageName, false))
    {
        if (IsAllowlisted(TEXT("GP.Content"), InAssetData))
        {
            AssetWarning(InAsset, FText::FromString(TEXT("GP.Content：无效资产路径命中有效Allowlist（临时豁免）。")));
            AssetPasses(InAsset);
            return EDataValidationResult::Valid;
        }

        AssetFails(InAsset, FText::FromString(TEXT("GP.Content：资产长包路径无效。")));
        return EDataValidationResult::Invalid;
    }

    if (PackageName.Contains(TEXT("/Developers/")) || PackageName.Contains(TEXT("/Developer/")))
    {
        AssetWarning(InAsset, FText::FromString(TEXT("GP.Content：Developer内容不得进入Shipping Cook，请由Release Gate检查真实Cook工件。")));
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}

bool UGamePlatformDefinitionValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    return InObject && InObject->IsA<UGamePlatformDefinitionBase>();
}

EDataValidationResult UGamePlatformDefinitionValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
    // 引擎注册的逐资产路径与全局审计共用真实平台类型合同；类/资产名字不能代替LogicalId与版本校验。
    const auto* Definition = Cast<UGamePlatformDefinitionBase>(InAsset);
    const FGamePlatformResult Result = Definition
        ? Definition->ValidateDefinition()
        : FGamePlatformResult::Failure(TEXT("DefinitionTypeMismatch"), TEXT("资产不是平台定义派生。"));
    if (!Result.IsSuccess() || !Definition || !Definition->GetPrimaryAssetId().IsValid())
    {
        if (IsAllowlisted(TEXT("GP.Definition"), InAssetData))
        {
            AssetWarning(InAsset, FText::FromString(TEXT("GP.Definition：真实定义合同失败，命中有效审批豁免；不代表资源已运行。")));
            AssetPasses(InAsset);
            return EDataValidationResult::Valid;
        }
        AssetFails(InAsset, FText::FromString(TEXT("GP.Definition：") + Result.Message));
        return EDataValidationResult::Invalid;
    }
    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
bool UGamePlatformStableIdValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    if (!InObject)
    {
        return false;
    }

    if (InObject->IsA<UGamePlatformPrimaryDataAsset>()) return true;

    for (TFieldIterator<FProperty> It(InObject->GetClass()); It; ++It)
    {
        if (IsStableIdProperty(It->GetFName()))
        {
            return true;
        }
    }
    return false;
}

EDataValidationResult UGamePlatformStableIdValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    // 平台资产身份由LogicalId完整结构/规范主资产ID决定；版本分隔符等不走旧字符串字段猜测。
    if (const auto* Primary = Cast<UGamePlatformPrimaryDataAsset>(InAsset))
    {
        if (!Primary->GetPrimaryAssetId().IsValid())
        {
            // 保留现行规则的限期豁免合同；豁免只改变审计结果，不给Data签发非法主资产身份。
            if (IsAllowlisted(TEXT("GP.StableId"), InAssetData))
            {
                AssetWarning(InAsset, FText::FromString(TEXT("GP.StableId：LogicalId非法，但命中有效Allowlist（临时豁免）。")));
                AssetPasses(InAsset);
                return EDataValidationResult::Valid;
            }
            AssetFails(InAsset, FText::FromString(TEXT("GP.StableId：LogicalId必须形成有效规范主资产身份。")));
            return EDataValidationResult::Invalid;
        }
        AssetPasses(InAsset);
        return EDataValidationResult::Valid;
    }
    for (TFieldIterator<FProperty> It(InAsset->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!IsStableIdProperty(Property->GetFName()))
        {
            continue;
        }

        const FString Value = ExportPropertyValue(InAsset, Property);
        if (!HasValidStableIdCharacters(Value))
        {
            if (IsAllowlisted(TEXT("GP.StableId"), InAssetData))
            {
                AssetWarning(InAsset, FText::FromString(TEXT("GP.StableId：稳定ID非法，但命中有效Allowlist（临时豁免）。")));
                AssetPasses(InAsset);
                return EDataValidationResult::Valid;
            }

            AssetFails(
                InAsset,
                FText::FromString(FString::Printf(
                    TEXT("GP.StableId：%s为空或包含非法字符。"),
                    *Property->GetName())));
            return EDataValidationResult::Invalid;
        }
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}

bool UGamePlatformGameplayTagValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    if (!InObject)
    {
        return false;
    }

    for (TFieldIterator<FStructProperty> It(InObject->GetClass()); It; ++It)
    {
        if (It->Struct == FGameplayTag::StaticStruct() || It->Struct == FGameplayTagContainer::StaticStruct())
        {
            return true;
        }
    }
    return false;
}

EDataValidationResult UGamePlatformGameplayTagValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    for (TFieldIterator<FStructProperty> It(InAsset->GetClass()); It; ++It)
    {
        FStructProperty* Property = *It;

        if (Property->Struct == FGameplayTag::StaticStruct())
        {
            const FGameplayTag* Tag = Property->ContainerPtrToValuePtr<FGameplayTag>(InAsset);
            if (Tag && Tag->IsValid() && FGamePlatformValidationService::IsRemovedLegacyGameplayTag(Tag->ToString()))
            {
                if (IsAllowlisted(TEXT("GP.GameplayTag"), InAssetData))
                {
                    AssetWarning(InAsset, FText::FromString(TEXT("GP.GameplayTag：旧系统Tag命中有效Allowlist（临时豁免）。")));
                    AssetPasses(InAsset);
                    return EDataValidationResult::Valid;
                }

                AssetFails(InAsset, FText::FromString(TEXT("GP.GameplayTag：禁止恢复FiveCamp/Faction/Element/KingSeal旧系统标签。")));
                return EDataValidationResult::Invalid;
            }
        }
        else if (Property->Struct == FGameplayTagContainer::StaticStruct())
        {
            const FGameplayTagContainer* Container = Property->ContainerPtrToValuePtr<FGameplayTagContainer>(InAsset);
            if (!Container)
            {
                continue;
            }

            TArray<FGameplayTag> Tags;
            Container->GetGameplayTagArray(Tags);
            for (const FGameplayTag& Tag : Tags)
            {
                if (Tag.IsValid() && FGamePlatformValidationService::IsRemovedLegacyGameplayTag(Tag.ToString()))
                {
                    if (IsAllowlisted(TEXT("GP.GameplayTag"), InAssetData))
                    {
                        AssetWarning(InAsset, FText::FromString(TEXT("GP.GameplayTag：TagContainer旧标签命中有效Allowlist（临时豁免）。")));
                        AssetPasses(InAsset);
                        return EDataValidationResult::Valid;
                    }

                    AssetFails(InAsset, FText::FromString(TEXT("GP.GameplayTag：TagContainer包含已取消旧系统标签。")));
                    return EDataValidationResult::Invalid;
                }
            }
        }
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}

bool UGamePlatformServerAssetSafetyValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    const FString Package = InAssetData.PackageName.ToString();
    return InObject != nullptr
        && (Package.Contains(TEXT("/Server/")) || Package.Contains(TEXT("/ServerSafe/")));
}

EDataValidationResult UGamePlatformServerAssetSafetyValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = AssetRegistryModule.Get();

    TArray<FName> Dependencies;
    Registry.GetDependencies(
        InAssetData.PackageName,
        Dependencies,
        UE::AssetRegistry::EDependencyCategory::Package,
        UE::AssetRegistry::FDependencyQuery());

    for (const FName DependencyPackage : Dependencies)
    {
        TArray<FAssetData> DependencyAssets;
        Registry.GetAssetsByPackageName(DependencyPackage, DependencyAssets, true);

        for (const FAssetData& DependencyAsset : DependencyAssets)
        {
            const FString Package = DependencyAsset.PackageName.ToString();
            const FString ClassName = DependencyAsset.AssetClassPath.GetAssetName().ToString();

            if (IsForbiddenServerDependency(Package, ClassName))
            {
                if (IsAllowlisted(TEXT("GP.ServerAssetSafety"), InAssetData))
                {
                    AssetWarning(
                        InAsset,
                        FText::FromString(TEXT("GP.ServerAssetSafety：服务器到客户端表现资源依赖命中有效Allowlist（临时豁免）。")));
                    AssetPasses(InAsset);
                    return EDataValidationResult::Valid;
                }

                AssetFails(
                    InAsset,
                    FText::FromString(FString::Printf(
                        TEXT("GP.ServerAssetSafety：%s -> %s（%s）形成服务器到客户端表现资源的硬依赖。"),
                        *InAssetData.PackageName.ToString(),
                        *Package,
                        *ClassName)));
                return EDataValidationResult::Invalid;
            }
        }
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
