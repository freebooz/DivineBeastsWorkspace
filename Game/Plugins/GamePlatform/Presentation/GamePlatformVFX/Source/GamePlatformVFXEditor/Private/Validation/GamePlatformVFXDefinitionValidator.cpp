#include "Validation/GamePlatformVFXDefinitionValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraEffectType.h"
#include "NiagaraSystem.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelVariable.h"
#include "NiagaraTypes.h"

void FGamePlatformVFXDefinitionValidator::Validate(
    const UGamePlatformVFXDefinition& Definition,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    const FGamePlatformResult DefinitionResult = Definition.ValidateDefinition();
    if (!DefinitionResult.IsSuccess())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.Invalid");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = FText::FromString(DefinitionResult.Message);
    }

    if (Definition.GetDefinitionId().IsNone())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingId");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingDefinitionId", "VFX Definition必须声明稳定DefinitionId。");
    }

    UNiagaraEffectType* ExpectedEffectType = nullptr;
    if (Definition.UsesScalability() && Definition.GetEffectType().IsNull())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingEffectType");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingEffectType", "启用Scalability的VFX Definition必须关联Niagara Effect Type。");
    }
    else if (!Definition.GetEffectType().IsNull())
    {
        ExpectedEffectType = Definition.GetEffectType().LoadSynchronous();
        if (!IsValid(ExpectedEffectType))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.EffectTypeLoadFailed");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "EffectTypeLoadFailed", "VFX Definition配置的Niagara Effect Type无法加载。");
        }
    }

    // 仅在显式启用共享瞬时事件时加载/核验NDC；此检查不代替Niagara读者System的GPU实测。
    if (Definition.UsesSharedBurstChannel())
    {
        UNiagaraDataChannelAsset* ChannelAsset = Definition.GetSharedBurstDataChannel().LoadSynchronous();
        UNiagaraDataChannel* Channel = IsValid(ChannelAsset) ? ChannelAsset->Get() : nullptr;
        // 名称和Niagara类型必须同时匹配；仅校验变量数量会产生无画面的假成功写入。
        // UE5.8的DataChannelVariable继承FNiagaraVariableBase，可以直接读取GetName/GetType。
        const auto HasTypedField = [Channel](FName Name, const FNiagaraTypeDefinition& Expected)
        {
            if (!IsValid(Channel)) return false;
            const FNiagaraTypeDefinition NativeType = FNiagaraDataChannelVariable::ToDataChannelType(Expected);
            for (const FNiagaraDataChannelVariable& Variable : Channel->GetVariables())
            {
                if (Variable.GetName() == Name && Variable.GetType() == NativeType)
                {
                    return true;
                }
            }
            return false;
        };
        const bool bValidSchema = IsValid(Channel) && Channel->IsValid() &&
            HasTypedField(TEXT("Position"), FNiagaraTypeDefinition::GetPositionDef()) &&
            HasTypedField(TEXT("Normal"), FNiagaraTypeDefinition::GetVec3Def()) &&
            HasTypedField(TEXT("Color"), FNiagaraTypeDefinition::GetColorDef()) &&
            HasTypedField(TEXT("Scale"), FNiagaraTypeDefinition::GetFloatDef()) &&
            HasTypedField(TEXT("StyleId"), FNiagaraTypeDefinition::GetIntDef()) &&
            HasTypedField(TEXT("Seed"), FNiagaraTypeDefinition::GetIntDef());
        if (!bValidSchema)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.NDC.InvalidChannelOrSchema");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "InvalidNDCChannel",
                "共享命中NDC必须含Position/Normal/Color/Scale/StyleId/Seed六个名称与类型精确匹配的字段。");
        }
    }

    // 编辑器验证允许同步装载软引用，确保默认、平台和质量变体全部接受同一LWC/Bounds/EffectType约束。
    const auto ValidateSystem = [&Definition, ExpectedEffectType, &OutIssues](
        const TSoftObjectPtr<UNiagaraSystem>& SystemRef,
        const FString& VariantLabel)
    {
        if (SystemRef.IsNull())
        {
            return;
        }

        UNiagaraSystem* System = SystemRef.LoadSynchronous();
        if (!IsValid(System))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.VariantLoadFailed");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "VariantLoadFailed", "VFX Niagara变体无法加载：{0}"),
                FText::FromString(VariantLabel));
            return;
        }

        if (IsValid(ExpectedEffectType) && System->GetEffectType() != ExpectedEffectType)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.EffectTypeMismatch");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "EffectTypeMismatch", "VFX Niagara变体未绑定Definition声明的Effect Type：{0}"),
                FText::FromString(VariantLabel));
        }

        if (Definition.RequiresLargeWorldCoordinates() && !System->SupportsLargeWorldCoordinates())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.LWC");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "MissingLWCVariant", "VFX Niagara变体未启用Large World Coordinates支持：{0}"),
                FText::FromString(VariantLabel));
        }

        if (Definition.RequiresFixedBounds() && !System->bFixedBounds)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.FixedBounds");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "MissingFixedBoundsVariant", "VFX Niagara变体未启用Fixed Bounds：{0}"),
                FText::FromString(VariantLabel));
        }
    };

    ValidateSystem(Definition.GetNiagaraSystem(), TEXT("Default"));
    for (const TPair<FName, TSoftObjectPtr<UNiagaraSystem>>& Pair : Definition.GetPlatformVariants())
    {
        ValidateSystem(Pair.Value, FString::Printf(TEXT("Platform:%s"), *Pair.Key.ToString()));
    }
    for (const TPair<EGamePlatformVFXQualityTier, TSoftObjectPtr<UNiagaraSystem>>& Pair : Definition.GetQualityVariants())
    {
        ValidateSystem(Pair.Value, FString::Printf(TEXT("Quality:%d"), static_cast<int32>(Pair.Key)));
    }

    if (Definition.GetBehavior() != EGamePlatformVFXBehavior::Composite &&
        Definition.GetNiagaraSystem().IsNull())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingNiagara");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingNiagara", "非复合 VFX Definition 必须指定 Niagara System。");
    }

    if (const UGamePlatformVFXCompositeDefinition* Composite =
        Cast<UGamePlatformVFXCompositeDefinition>(&Definition))
    {
        if (Composite->Steps.IsEmpty())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.Empty");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "EmptyComposite", "Composite Definition 至少需要一个子步骤。");
        }
    }
}
