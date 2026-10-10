#include "Nodes/GamePlatformPCGNodes.h"
#include "Nodes/GamePlatformPCGNodeMetadata.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "GamePlatformPCGLog.h"
#include "Metadata/PCGMetadata.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Schema/GamePlatformPCGSchema.h"
#include "Services/GamePlatformPCGLinearRules.h"
#include "Services/GamePlatformPCGPriorityRules.h"
#include "Services/GamePlatformPCGSpatialRules.h"

namespace
{
UPCGBasePointData* DuplicatePointInput(FPCGContext* Context, const FPCGTaggedData& Input)
{
    const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
    const UPCGBasePointData* PointData = SpatialData ? SpatialData->ToBasePointData(Context) : nullptr;
    return PointData ? Cast<UPCGBasePointData>(PointData->DuplicateData(Context)) : nullptr;
}

TArray<FPCGTaggedData> PointInputs(FPCGContext* Context)
{
    return Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
}

void WarnInvalidInput(const TCHAR* NodeName)
{
    UE_LOG(LogGamePlatformPCG, Warning, TEXT("%s：输入不是可转换的PCG点数据，按Fail-Safe输出空结果。"), NodeName);
}

template<typename T>
void EnsureAttribute(UPCGMetadata& Metadata, FName Name, const T& DefaultValue)
{
    Metadata.FindOrCreateAttribute<T>(Name, DefaultValue, true, true, true);
}

bool IsSchemaTypeCompatible(EGamePlatformPCGAttributeValueType Expected, EPCGMetadataTypes Actual)
{
    // UInt8为协议逻辑枚举：现有M0/M1节点的写入值是int32，UE5.8也有Byte原生类型。
    // P9的Guid真实Metadata存储尚未通过引擎验证，本阶段对该属性失败关闭。
    switch (Expected)
    {
    case EGamePlatformPCGAttributeValueType::Name: return Actual == EPCGMetadataTypes::Name;
    case EGamePlatformPCGAttributeValueType::Float: return Actual == EPCGMetadataTypes::Float;
    case EGamePlatformPCGAttributeValueType::Int32: return Actual == EPCGMetadataTypes::Integer32;
    case EGamePlatformPCGAttributeValueType::UInt8:
        return Actual == EPCGMetadataTypes::Integer32 || Actual == EPCGMetadataTypes::Byte;
    case EGamePlatformPCGAttributeValueType::Guid: return false;
    default: return false;
    }
}

void InitializeMetadataEntries(UPCGBasePointData& PointData)
{
    if (!PointData.Metadata)
    {
        return;
    }

    TPCGValueRange<PCGMetadataEntryKey> Keys = PointData.GetMetadataEntryValueRange();
    for (int32 Index = 0; Index < Keys.Num(); ++Index)
    {
        if (Keys[Index] == PCGInvalidEntryKey)
        {
            PointData.Metadata->InitializeOnSet(Keys[Index]);
        }
    }
}
}

bool GamePlatformPCGNodeMetadata::AssignMeshSetId(UPCGBasePointData& PointData, FName MeshSetId)
{
    // 上游WriteSchemaDefaults可能已经创建默认NAME_None属性；仅修改属性默认值并不能改变已有点值。
    if (MeshSetId.IsNone() || !PointData.Metadata)
    {
        return false;
    }

    FPCGMetadataAttribute<FName>* Attribute = PointData.Metadata->FindOrCreateAttribute<FName>(
        FGamePlatformPCGAttr::SpawnMeshSetId, MeshSetId, false, true, true);
    if (!Attribute)
    {
        return false;
    }

    // 初始化继承自上游的无效Entry，并对本次复制的点数据逐个SetValue。
    // 不修改源Graph、其它生成请求、客户端表现资源或服务器权威数据。
    InitializeMetadataEntries(PointData);
    const TConstPCGValueRange<PCGMetadataEntryKey> Keys = PointData.GetConstMetadataEntryValueRange();
    for (const PCGMetadataEntryKey Key : Keys)
    {
        Attribute->SetValue(Key, MeshSetId);
    }
    return true;
}

bool GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(
    const UPCGMetadata& Metadata, const TArray<FName>& ExplicitRequired)
{
    TArray<FName> Names;
    TArray<EPCGMetadataTypes> Types;
    Metadata.GetAttributes(Names, Types);
    if (Names.Num() != Types.Num())
    {
        return false;
    }

    TSet<FName> Available;
    const TConstArrayView<FGamePlatformPCGAttributeDescriptor> Descriptors = FGamePlatformPCGSchema::GetAttributes();
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        const FName Name = Names[Index];
        const FGamePlatformPCGAttributeDescriptor* Descriptor = nullptr;
        for (const FGamePlatformPCGAttributeDescriptor& Candidate : Descriptors)
        {
            if (Candidate.Name == Name)
            {
                Descriptor = &Candidate;
                break;
            }
        }

        if (!Descriptor)
        {
            // 引擎或其他插件的普通元数据不受本Schema约束，但禁止注入未知Pcg.*字段。
            if (Name.ToString().StartsWith(TEXT("Pcg.")))
            {
                return false;
            }
            continue;
        }
        if (!IsSchemaTypeCompatible(Descriptor->ValueType, Types[Index]))
        {
            return false;
        }
        Available.Add(Name);
    }

    // 显式必需清单只能在核心字段基础上增加约束，不能绕过Layer/Exclude/Seed的强制要求。
    if (!FGamePlatformPCGSchema::ValidateRequiredAttributes(Available).IsSuccess())
    {
        return false;
    }
    for (const FName Name : ExplicitRequired)
    {
        if (!FGamePlatformPCGSchema::IsKnownAttribute(Name) || !Available.Contains(Name))
        {
            return false;
        }
    }
    return true;
}

FPCGElementPtr UGamePlatformPCGWriteSchemaDefaultsSettings::CreateElement() const { return MakeShared<FGamePlatformPCGWriteSchemaDefaultsElement>(); }
FPCGElementPtr UGamePlatformPCGWriteExcludeSettings::CreateElement() const { return MakeShared<FGamePlatformPCGWriteExcludeElement>(); }
FPCGElementPtr UGamePlatformPCGPriorityCarveSettings::CreateElement() const { return MakeShared<FGamePlatformPCGPriorityCarveElement>(); }
FPCGElementPtr UGamePlatformPCGSpatialCarveSettings::CreateElement() const { return MakeShared<FGamePlatformPCGSpatialCarveElement>(); }
FPCGElementPtr UGamePlatformPCGProjectAlignSettings::CreateElement() const { return MakeShared<FGamePlatformPCGProjectAlignElement>(); }
FPCGElementPtr UGamePlatformPCGApplySpawnPolicySettings::CreateElement() const { return MakeShared<FGamePlatformPCGApplySpawnPolicyElement>(); }
FPCGElementPtr UGamePlatformPCGAssignMeshSetSettings::CreateElement() const { return MakeShared<FGamePlatformPCGAssignMeshSetElement>(); }
FPCGElementPtr UGamePlatformPCGValidateSchemaSettings::CreateElement() const { return MakeShared<FGamePlatformPCGValidateSchemaElement>(); }
FPCGElementPtr UGamePlatformPCGFitPostsToSplineSettings::CreateElement() const { return MakeShared<FGamePlatformPCGFitPostsToSplineElement>(); }
FPCGElementPtr UGamePlatformPCGBreakSpansByTagsSettings::CreateElement() const { return MakeShared<FGamePlatformPCGBreakSpansByTagsElement>(); }
FPCGElementPtr UGamePlatformPCGBuildRowsSettings::CreateElement() const { return MakeShared<FGamePlatformPCGBuildRowsElement>(); }
FPCGElementPtr UGamePlatformPCGSelectSpanMeshByLengthSettings::CreateElement() const { return MakeShared<FGamePlatformPCGSelectSpanMeshByLengthElement>(); }

bool FGamePlatformPCGWriteSchemaDefaultsElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGWriteSchemaDefaultsSettings* Settings = Context->GetInputSettings<UGamePlatformPCGWriteSchemaDefaultsSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("WriteSchemaDefaults"));
            continue;
        }

        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::BiomeId, FName());
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::BiomeWeight, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::BiomePriority, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::LayerName, Settings->DefaultLayerName);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::LayerIndex, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::SpawnMeshSetId, FName());
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::SpawnClassId, FName());
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::SpawnFlags, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::RuleSlope, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::RuleHeight, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::RuleWetness, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::RuleSpanLength, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ExcludeMask, 0.0f);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ExcludeSource, FName());
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ExecGridBand, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ExecLodBand, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ExecSeed, Settings->DefaultSeed);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::EnclosureKind, 0);
        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::ConnectorType, 0);

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGWriteExcludeElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGWriteExcludeSettings* Settings = Context->GetInputSettings<UGamePlatformPCGWriteExcludeSettings>();
    check(Settings);

    if (Settings->ExcludeSource.IsNone() || !FMath::IsFinite(Settings->ExcludeStrength))
    {
        UE_LOG(LogGamePlatformPCG, Warning, TEXT("WriteExclude：排除来源为空或强度非法，按Fail-Safe输出空结果。"));
        return true;
    }

    const float Strength = FMath::Clamp(Settings->ExcludeStrength, 0.0f, 1.0f);
    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("WriteExclude"));
            continue;
        }

        FPCGMetadataAttribute<float>* Mask = OutputData->Metadata->FindOrCreateAttribute<float>(
            FGamePlatformPCGAttr::ExcludeMask, Strength, false, true, true);
        FPCGMetadataAttribute<FName>* Source = OutputData->Metadata->FindOrCreateAttribute<FName>(
            FGamePlatformPCGAttr::ExcludeSource, Settings->ExcludeSource, false, true, true);
        if (!Mask || !Source)
        {
            continue;
        }

        InitializeMetadataEntries(*OutputData);
        TPCGValueRange<PCGMetadataEntryKey> Entries = OutputData->GetMetadataEntryValueRange();
        for (const PCGMetadataEntryKey Key : Entries)
        {
            Mask->SetValue(Key, Strength);
            Source->SetValue(Key, Settings->ExcludeSource);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGPriorityCarveElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGPriorityCarveSettings* Settings = Context->GetInputSettings<UGamePlatformPCGPriorityCarveSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("PriorityCarve"));
            continue;
        }

        const FPCGMetadataAttribute<float>* Mask = OutputData->Metadata->GetConstTypedAttribute<float>(FGamePlatformPCGAttr::ExcludeMask);
        if (!Mask)
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("PriorityCarve：缺少Pcg.Exclude.Mask，按Fail-Safe输出空结果。"));
            continue;
        }

        const TConstPCGValueRange<PCGMetadataEntryKey> Entries = OutputData->GetConstMetadataEntryValueRange();
        TPCGValueRange<float> Densities = OutputData->GetDensityValueRange();
        for (int32 Index = 0; Index < Densities.Num(); ++Index)
        {
            const float ExcludeMask = Mask->GetValueFromItemKey(Entries[Index]);
            if (!FMath::IsFinite(ExcludeMask) || FGamePlatformPCGPriorityRules::ShouldCarve(
                    Settings->SubjectPriority, Settings->CarverPriority, ExcludeMask, Settings->ExcludeThreshold))
            {
                Densities[Index] = 0.0f;
            }
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGSpatialCarveElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGSpatialCarveSettings* Settings = Context->GetInputSettings<UGamePlatformPCGSpatialCarveSettings>();
    check(Settings);

    if (Settings->Masks.Num() > 256 || !FMath::IsFinite(Settings->ExcludeThreshold) ||
        Settings->ExcludeThreshold < 0.0f || Settings->ExcludeThreshold > 1.0f)
    {
        UE_LOG(LogGamePlatformPCG, Warning, TEXT("SpatialCarve：边界数量或阈值非法，拒绝整批生成。"));
        return true;
    }

    TArray<FGamePlatformPCGPreparedSpatialMask> Prepared;
    if (!FGamePlatformPCGSpatialRules::PrepareMasks(Settings->Masks, Prepared))
    {
        UE_LOG(LogGamePlatformPCG, Warning, TEXT("SpatialCarve：空间掩码非法或越界，拒绝整批生成。"));
        return true;
    }

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("SpatialCarve"));
            continue;
        }

        // 上游默认协议字段可能已经存在；必须为每个点写入值，不能仅修改Metadata的默认值。
        FPCGMetadataAttribute<float>* Mask = OutputData->Metadata->FindOrCreateAttribute<float>(
            FGamePlatformPCGAttr::ExcludeMask, 0.0f, false, true, true);
        FPCGMetadataAttribute<FName>* Source = OutputData->Metadata->FindOrCreateAttribute<FName>(
            FGamePlatformPCGAttr::ExcludeSource, FName(), false, true, true);
        if (!Mask || !Source)
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("SpatialCarve：排除元数据类型不正确或不可写，拒绝输出。"));
            continue;
        }
        InitializeMetadataEntries(*OutputData);

        const TConstPCGValueRange<PCGMetadataEntryKey> Keys = OutputData->GetConstMetadataEntryValueRange();
        const TConstPCGValueRange<FTransform> Transforms = OutputData->GetConstTransformValueRange();
        TPCGValueRange<float> Densities = OutputData->GetDensityValueRange();
        if (Keys.Num() != Densities.Num() || Transforms.Num() != Densities.Num())
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("SpatialCarve：点数据长度不一致，拒绝输出。"));
            continue;
        }

        bool bValid = true;
        for (int32 I = 0; I < Densities.Num(); ++I)
        {
            const FVector P = Transforms[I].GetLocation();
            FGuid SourceId;
            float Strength = 0.0f;
            if (!FGamePlatformPCGSpatialRules::EvaluatePrepared(
                    FVector2D(P.X, P.Y), Settings->SubjectPriority, Settings->ExcludeThreshold,
                    Prepared, Strength, SourceId))
            {
                bValid = false;
                break;
            }
            Mask->SetValue(Keys[I], Strength);
            Source->SetValue(Keys[I], SourceId.IsValid() ? FName(*SourceId.ToString(EGuidFormats::Digits)) : NAME_None);
            if (SourceId.IsValid())
            {
                Densities[I] = 0.0f;
            }
        }
        if (!bValid)
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("SpatialCarve：空间计算失败，拒绝全部输出。"));
            continue;
        }
        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGProjectAlignElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGProjectAlignSettings* Settings = Context->GetInputSettings<UGamePlatformPCGProjectAlignSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData)
        {
            WarnInvalidInput(TEXT("ProjectAlign"));
            continue;
        }

        if (Settings->bKeepVertical)
        {
            TPCGValueRange<FTransform> Transforms = OutputData->GetTransformValueRange();
            for (FTransform& Transform : Transforms)
            {
                const float Yaw = Transform.Rotator().Yaw;
                Transform.SetRotation(FRotator(0.0f, Yaw, 0.0f).Quaternion());
            }
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGApplySpawnPolicyElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGApplySpawnPolicySettings* Settings = Context->GetInputSettings<UGamePlatformPCGApplySpawnPolicySettings>();
    check(Settings);

    const float DensityScale = FMath::Clamp(Settings->DensityScale, 0.0f, 1.0f);
    const float UniformScale = FMath::Max(0.01f, Settings->UniformScale);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData)
        {
            WarnInvalidInput(TEXT("ApplySpawnPolicy"));
            continue;
        }

        TPCGValueRange<float> Densities = OutputData->GetDensityValueRange();
        for (float& Density : Densities)
        {
            Density = FMath::Clamp(Density * DensityScale, 0.0f, 1.0f);
        }

        TPCGValueRange<FTransform> Transforms = OutputData->GetTransformValueRange();
        for (FTransform& Transform : Transforms)
        {
            Transform.SetScale3D(Transform.GetScale3D() * UniformScale);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGAssignMeshSetElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGAssignMeshSetSettings* Settings = Context->GetInputSettings<UGamePlatformPCGAssignMeshSetSettings>();
    check(Settings);

    if (Settings->MeshSetId.IsNone())
    {
        UE_LOG(LogGamePlatformPCG, Warning, TEXT("AssignMeshSet：MeshSetId为空，按Fail-Safe输出空结果。"));
        return true;
    }

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("AssignMeshSet"));
            continue;
        }

        if (!GamePlatformPCGNodeMetadata::AssignMeshSetId(*OutputData, Settings->MeshSetId))
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("AssignMeshSet：稳定MeshSetId逐点写入失败，按Fail-Safe丢弃输出。"));
            continue;
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGValidateSchemaElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGValidateSchemaSettings* Settings = Context->GetInputSettings<UGamePlatformPCGValidateSchemaSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
        const UPCGBasePointData* PointData = SpatialData ? SpatialData->ToBasePointData(Context) : nullptr;
        const UPCGMetadata* Metadata = PointData ? PointData->ConstMetadata() : nullptr;
        if (!Metadata || !GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Metadata, Settings->RequiredAttributes))
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("ValidateSchema：必需字段缺失或包含未注册字段，按Fail-Safe丢弃该输入。"));
            continue;
        }

        Context->OutputData.TaggedData.Add(Input);
    }
    return true;
}

bool FGamePlatformPCGFitPostsToSplineElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGFitPostsToSplineSettings* Settings = Context->GetInputSettings<UGamePlatformPCGFitPostsToSplineSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData)
        {
            WarnInvalidInput(TEXT("FitPostsToSpline"));
            continue;
        }

        const TConstPCGValueRange<FTransform> Transforms = OutputData->GetConstTransformValueRange();
        TPCGValueRange<float> Densities = OutputData->GetDensityValueRange();
        FVector LastKept = FVector::ZeroVector;
        bool bHasLast = false;

        for (int32 Index = 0; Index < Transforms.Num(); ++Index)
        {
            const FVector Position = Transforms[Index].GetLocation();
            if (!bHasLast)
            {
                LastKept = Position;
                bHasLast = true;
                continue;
            }

            const float Distance = FVector::Distance(LastKept, Position);
            if (FGamePlatformPCGLinearRules::ShouldKeepPost(Distance, Settings->PostSpacingCm))
            {
                LastKept = Position;
            }
            else
            {
                Densities[Index] = 0.0f;
            }
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGBreakSpansByTagsElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGBreakSpansByTagsSettings* Settings = Context->GetInputSettings<UGamePlatformPCGBreakSpansByTagsSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        const bool bBlocked = Settings->BlockingTags.ContainsByPredicate(
            [&Input](const FString& BlockingTag)
            {
                return Input.Tags.Contains(BlockingTag);
            });

        if (!bBlocked)
        {
            Context->OutputData.TaggedData.Add(Input);
        }
    }
    return true;
}

bool FGamePlatformPCGBuildRowsElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGBuildRowsSettings* Settings = Context->GetInputSettings<UGamePlatformPCGBuildRowsSettings>();
    check(Settings);

    const float Spacing = FMath::Max(1.0f, Settings->RowSpacingCm);
    const FQuat ToRowSpace = FRotator(0.0f, -Settings->RowYawDegrees, 0.0f).Quaternion();
    const FQuat FromRowSpace = ToRowSpace.Inverse();

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData)
        {
            WarnInvalidInput(TEXT("BuildRows"));
            continue;
        }

        TPCGValueRange<FTransform> Transforms = OutputData->GetTransformValueRange();
        for (FTransform& Transform : Transforms)
        {
            FVector Local = ToRowSpace.RotateVector(Transform.GetLocation());
            Local.Y = FMath::GridSnap(Local.Y, Spacing);
            Transform.SetLocation(FromRowSpace.RotateVector(Local));

            const FVector Scale = Transform.GetScale3D();
            Transform.SetRotation(FRotator(0.0f, Settings->RowYawDegrees, 0.0f).Quaternion());
            Transform.SetScale3D(Scale);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}

bool FGamePlatformPCGSelectSpanMeshByLengthElement::ExecuteInternal(FPCGContext* Context) const
{
    const UGamePlatformPCGSelectSpanMeshByLengthSettings* Settings = Context->GetInputSettings<UGamePlatformPCGSelectSpanMeshByLengthSettings>();
    check(Settings);

    for (const FPCGTaggedData& Input : PointInputs(Context))
    {
        UPCGBasePointData* OutputData = DuplicatePointInput(Context, Input);
        if (!OutputData || !OutputData->Metadata)
        {
            WarnInvalidInput(TEXT("SelectSpanMeshByLength"));
            continue;
        }

        const FPCGMetadataAttribute<float>* SpanLength = OutputData->Metadata->GetConstTypedAttribute<float>(FGamePlatformPCGAttr::RuleSpanLength);
        if (!SpanLength)
        {
            UE_LOG(LogGamePlatformPCG, Warning, TEXT("SelectSpanMeshByLength：缺少Pcg.Rule.SpanLength，按Fail-Safe输出空结果。"));
            continue;
        }

        FPCGMetadataAttribute<FName>* MeshSetId = OutputData->Metadata->FindOrCreateAttribute<FName>(
            FGamePlatformPCGAttr::SpawnMeshSetId, Settings->FallbackMeshSetId, false, true, true);
        if (!MeshSetId)
        {
            continue;
        }

        InitializeMetadataEntries(*OutputData);
        TPCGValueRange<PCGMetadataEntryKey> Entries = OutputData->GetMetadataEntryValueRange();
        for (int32 Index = 0; Index < Entries.Num(); ++Index)
        {
            const float Length = SpanLength->GetValueFromItemKey(Entries[Index]);
            FName Selected;
            if (!FGamePlatformPCGLinearRules::SelectSpanMeshByLength(Length, Settings->Rules, Selected))
            {
                Selected = Settings->FallbackMeshSetId;
            }

            if (!Selected.IsNone())
            {
                MeshSetId->SetValue(Entries[Index], Selected);
            }
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutputData;
    }
    return true;
}
