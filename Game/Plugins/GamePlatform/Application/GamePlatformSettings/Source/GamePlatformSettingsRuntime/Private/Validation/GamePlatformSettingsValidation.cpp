#include "Validation/GamePlatformSettingsValidation.h"

namespace
{
    bool IsNumericType(const EGamePlatformSettingValueType Type)
    {
        return Type == EGamePlatformSettingValueType::Integer ||
            Type == EGamePlatformSettingValueType::Number;
    }

    int32 CompareNumeric(
        const FGamePlatformSettingValue& A,
        const FGamePlatformSettingValue& B)
    {
        const double Left =
            A.Type == EGamePlatformSettingValueType::Integer
                ? static_cast<double>(A.IntegerValue)
                : A.NumberValue;
        const double Right =
            B.Type == EGamePlatformSettingValueType::Integer
                ? static_cast<double>(B.IntegerValue)
                : B.NumberValue;
        return Left < Right ? -1 : (Left > Right ? 1 : 0);
    }
}

FGamePlatformResult FGamePlatformSettingsValidation::ValidateDescriptor(
    const FGamePlatformSettingDescriptor& Descriptor)
{
    if (Descriptor.SettingId.IsNone() || Descriptor.Category.IsNone())
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorIdentityInvalid"),
            TEXT("SettingId与Category必须是非None稳定标识。"));
    }

    if (Descriptor.SchemaVersion < 1)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorVersionInvalid"),
            TEXT("Descriptor SchemaVersion必须大于等于1。"));
    }

    // bSensitive仅表示诊断脱敏，不提供加密存储。敏感值只允许Session临时存在，禁止落入SaveGame/INI/环境变量/命令行。
    if (Descriptor.bSensitive &&
        Descriptor.PersistenceScope != EGamePlatformSettingScope::Session)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsSensitivePersistenceUnsupported"),
            TEXT("敏感设置只能使用Session作用域；Settings不是凭据、令牌或密钥存储。"));
    }

    if (!Descriptor.DefaultValue.IsValidForType(Descriptor.ValueType))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorDefaultTypeMismatch"),
            TEXT("默认值类型与Descriptor声明不一致。"));
    }

    const bool bNumeric = IsNumericType(Descriptor.ValueType);
    if ((Descriptor.bHasMinimum || Descriptor.bHasMaximum) && !bNumeric)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorRangeUnsupported"),
            TEXT("只有Integer/Number设置允许声明最小值或最大值。"));
    }

    if (Descriptor.bHasMinimum &&
        !Descriptor.MinimumValue.IsValidForType(Descriptor.ValueType))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorMinimumTypeMismatch"),
            TEXT("最小值类型与Descriptor声明不一致。"));
    }

    if (Descriptor.bHasMaximum &&
        !Descriptor.MaximumValue.IsValidForType(Descriptor.ValueType))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorMaximumTypeMismatch"),
            TEXT("最大值类型与Descriptor声明不一致。"));
    }

    if (Descriptor.bHasMinimum && Descriptor.bHasMaximum &&
        CompareNumeric(Descriptor.MinimumValue, Descriptor.MaximumValue) > 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorRangeInvalid"),
            TEXT("最小值不能大于最大值。"));
    }

    const FGamePlatformResult DefaultValidation =
        ValidateValue(Descriptor, Descriptor.DefaultValue);
    if (!DefaultValidation.IsSuccess())
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorDefaultOutOfRange"),
            TEXT("默认值未通过Descriptor自身约束。"));
    }

    switch (Descriptor.DefaultLayer)
    {
    case EGamePlatformSettingLayer::PlatformDefault:
    case EGamePlatformSettingLayer::ProjectDefault:
    case EGamePlatformSettingLayer::ProviderDefault:
    case EGamePlatformSettingLayer::ServerDefault:
        break;
    default:
        return FGamePlatformResult::Failure(
            TEXT("SettingsDescriptorDefaultLayerInvalid"),
            TEXT("Descriptor默认值只能位于平台/项目/Provider/服务器默认层。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSettingsValidation::ValidateValue(
    const FGamePlatformSettingDescriptor& Descriptor,
    const FGamePlatformSettingValue& Value)
{
    if (!Value.IsValidForType(Descriptor.ValueType))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsValueTypeMismatch"),
            TEXT("设置值类型与Descriptor声明不一致或包含非有限数。"));
    }

    if (Descriptor.ValueType == EGamePlatformSettingValueType::String &&
        Value.StringValue.Len() > GamePlatformSettingsLimits::MaxStringLength)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsStringTooLong"),
            TEXT("字符串设置超过平台安全长度上限。"));
    }

    if (Descriptor.bHasMinimum &&
        CompareNumeric(Value, Descriptor.MinimumValue) < 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsValueBelowMinimum"),
            TEXT("设置值低于允许的最小值。"));
    }

    if (Descriptor.bHasMaximum &&
        CompareNumeric(Value, Descriptor.MaximumValue) > 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsValueAboveMaximum"),
            TEXT("设置值高于允许的最大值。"));
    }

    return FGamePlatformResult::Success();
}

bool FGamePlatformSettingsValidation::SupportsRuntime(
    const FGamePlatformSettingDescriptor& Descriptor,
    const EGamePlatformSettingRuntimeScope CurrentRuntime)
{
    return Descriptor.RuntimeScope == EGamePlatformSettingRuntimeScope::Any ||
        Descriptor.RuntimeScope == CurrentRuntime;
}

bool FGamePlatformSettingsValidation::IsLayerAllowed(
    const FGamePlatformSettingDescriptor& Descriptor,
    const EGamePlatformSettingLayer Layer,
    const EGamePlatformSettingRuntimeScope CurrentRuntime)
{
    if (!SupportsRuntime(Descriptor, CurrentRuntime))
    {
        return false;
    }

    if (CurrentRuntime == EGamePlatformSettingRuntimeScope::Client)
    {
        switch (Layer)
        {
        case EGamePlatformSettingLayer::PlatformDefault:
        case EGamePlatformSettingLayer::ProjectDefault:
        case EGamePlatformSettingLayer::ProviderDefault:
        case EGamePlatformSettingLayer::Session:
            return true;
        case EGamePlatformSettingLayer::User:
            return Descriptor.PersistenceScope == EGamePlatformSettingScope::User;
        default:
            return false;
        }
    }

    switch (Layer)
    {
    case EGamePlatformSettingLayer::PlatformDefault:
    case EGamePlatformSettingLayer::ProjectDefault:
    case EGamePlatformSettingLayer::ProviderDefault:
    case EGamePlatformSettingLayer::Session:
        return true;
    case EGamePlatformSettingLayer::ServerDefault:
    case EGamePlatformSettingLayer::Deployment:
    case EGamePlatformSettingLayer::Environment:
    case EGamePlatformSettingLayer::CommandLine:
        return Descriptor.PersistenceScope == EGamePlatformSettingScope::Server ||
            Descriptor.RuntimeScope == EGamePlatformSettingRuntimeScope::Server;
    default:
        return false;
    }
}
