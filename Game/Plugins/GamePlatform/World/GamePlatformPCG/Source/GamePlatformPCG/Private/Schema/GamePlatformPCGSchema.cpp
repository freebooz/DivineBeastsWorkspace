#include "Schema/GamePlatformPCGSchema.h"

const FName FGamePlatformPCGAttr::BiomeId(TEXT("Pcg.Biome.Id"));
const FName FGamePlatformPCGAttr::BiomeWeight(TEXT("Pcg.Biome.Weight"));
const FName FGamePlatformPCGAttr::BiomePriority(TEXT("Pcg.Biome.Priority"));
const FName FGamePlatformPCGAttr::LayerName(TEXT("Pcg.Layer.Name"));
const FName FGamePlatformPCGAttr::LayerIndex(TEXT("Pcg.Layer.Index"));
const FName FGamePlatformPCGAttr::SpawnMeshSetId(TEXT("Pcg.Spawn.MeshSetId"));
const FName FGamePlatformPCGAttr::SpawnClassId(TEXT("Pcg.Spawn.ClassId"));
const FName FGamePlatformPCGAttr::SpawnFlags(TEXT("Pcg.Spawn.Flags"));
const FName FGamePlatformPCGAttr::RuleSlope(TEXT("Pcg.Rule.Slope"));
const FName FGamePlatformPCGAttr::RuleHeight(TEXT("Pcg.Rule.Height"));
const FName FGamePlatformPCGAttr::RuleWetness(TEXT("Pcg.Rule.Wetness"));
const FName FGamePlatformPCGAttr::RuleSpanLength(TEXT("Pcg.Rule.SpanLength"));
const FName FGamePlatformPCGAttr::ExcludeMask(TEXT("Pcg.Exclude.Mask"));
const FName FGamePlatformPCGAttr::ExcludeSource(TEXT("Pcg.Exclude.Source"));
const FName FGamePlatformPCGAttr::ExecGridBand(TEXT("Pcg.Exec.GridBand"));
const FName FGamePlatformPCGAttr::ExecLodBand(TEXT("Pcg.Exec.LodBand"));
const FName FGamePlatformPCGAttr::ExecSeed(TEXT("Pcg.Exec.Seed"));
const FName FGamePlatformPCGAttr::EnclosureKind(TEXT("Pcg.Encl.Kind"));
const FName FGamePlatformPCGAttr::ConnectorType(TEXT("Pcg.Connector.Type"));
const FName FGamePlatformPCGAttr::MutableId(TEXT("Pcg.Mutable.Id"));

namespace
{
const TArray<FGamePlatformPCGAttributeDescriptor>& GetSchemaDescriptors()
{
    static const TArray<FGamePlatformPCGAttributeDescriptor> Descriptors =
    {
        {FGamePlatformPCGAttr::BiomeId, EGamePlatformPCGAttributeValueType::Name, false},
        {FGamePlatformPCGAttr::BiomeWeight, EGamePlatformPCGAttributeValueType::Float, false},
        {FGamePlatformPCGAttr::BiomePriority, EGamePlatformPCGAttributeValueType::Int32, false},
        {FGamePlatformPCGAttr::LayerName, EGamePlatformPCGAttributeValueType::Name, true},
        {FGamePlatformPCGAttr::LayerIndex, EGamePlatformPCGAttributeValueType::Int32, false},
        {FGamePlatformPCGAttr::SpawnMeshSetId, EGamePlatformPCGAttributeValueType::Name, false},
        {FGamePlatformPCGAttr::SpawnClassId, EGamePlatformPCGAttributeValueType::Name, false},
        {FGamePlatformPCGAttr::SpawnFlags, EGamePlatformPCGAttributeValueType::Int32, false},
        {FGamePlatformPCGAttr::RuleSlope, EGamePlatformPCGAttributeValueType::Float, false},
        {FGamePlatformPCGAttr::RuleHeight, EGamePlatformPCGAttributeValueType::Float, false},
        {FGamePlatformPCGAttr::RuleWetness, EGamePlatformPCGAttributeValueType::Float, false},
        {FGamePlatformPCGAttr::RuleSpanLength, EGamePlatformPCGAttributeValueType::Float, false},
        {FGamePlatformPCGAttr::ExcludeMask, EGamePlatformPCGAttributeValueType::Float, true},
        {FGamePlatformPCGAttr::ExcludeSource, EGamePlatformPCGAttributeValueType::Name, false},
        {FGamePlatformPCGAttr::ExecGridBand, EGamePlatformPCGAttributeValueType::Int32, false},
        {FGamePlatformPCGAttr::ExecLodBand, EGamePlatformPCGAttributeValueType::Int32, false},
        {FGamePlatformPCGAttr::ExecSeed, EGamePlatformPCGAttributeValueType::Int32, true},
        {FGamePlatformPCGAttr::EnclosureKind, EGamePlatformPCGAttributeValueType::UInt8, false},
        {FGamePlatformPCGAttr::ConnectorType, EGamePlatformPCGAttributeValueType::UInt8, false},
        // UE PCG Metadata对FGuid的底层存储在正式写入节点落地前仍需逐版本验证；
        // Schema语义保持Guid，禁止因此把协议字段退化为业务自定义字符串。
        {FGamePlatformPCGAttr::MutableId, EGamePlatformPCGAttributeValueType::Guid, false}
    };
    return Descriptors;
}
}

FGamePlatformPCGSchemaVersion FGamePlatformPCGSchema::CurrentVersion()
{
    return {};
}

TConstArrayView<FGamePlatformPCGAttributeDescriptor> FGamePlatformPCGSchema::GetAttributes()
{
    return GetSchemaDescriptors();
}

bool FGamePlatformPCGSchema::IsKnownAttribute(FName AttributeName)
{
    return GetSchemaDescriptors().ContainsByPredicate(
        [AttributeName](const FGamePlatformPCGAttributeDescriptor& Descriptor)
        {
            return Descriptor.Name == AttributeName;
        });
}

bool FGamePlatformPCGSchema::IsCoreRequiredAttribute(FName AttributeName)
{
    const FGamePlatformPCGAttributeDescriptor* Descriptor = GetSchemaDescriptors().FindByPredicate(
        [AttributeName](const FGamePlatformPCGAttributeDescriptor& Candidate)
        {
            return Candidate.Name == AttributeName;
        });
    return Descriptor && Descriptor->bCoreRequired;
}

FGamePlatformResult FGamePlatformPCGSchema::ValidateAttributeName(FName AttributeName)
{
    if (AttributeName.IsNone())
    {
        return FGamePlatformResult::Failure(TEXT("PCGEmptyAttribute"), TEXT("PCG协议属性名不能为空。"));
    }

    const FString Text = AttributeName.ToString();
    if (!Text.StartsWith(TEXT("Pcg.")))
    {
        return FGamePlatformResult::Failure(TEXT("PCGAttributePrefixInvalid"), TEXT("自定义PCG属性必须使用Pcg.*前缀。"));
    }

    if (!IsKnownAttribute(AttributeName))
    {
        return FGamePlatformResult::Failure(TEXT("PCGUnknownAttribute"), TEXT("发现未注册的Pcg.*属性；必须先升级Schema再使用。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformPCGSchema::ValidateRequiredAttributes(const TSet<FName>& AvailableAttributes)
{
    for (const FGamePlatformPCGAttributeDescriptor& Descriptor : GetSchemaDescriptors())
    {
        if (Descriptor.bCoreRequired && !AvailableAttributes.Contains(Descriptor.Name))
        {
            return FGamePlatformResult::Failure(TEXT("PCGMissingRequiredAttribute"), TEXT("PCG数据缺少Schema v1核心必需字段。"));
        }
    }

    return FGamePlatformResult::Success();
}
