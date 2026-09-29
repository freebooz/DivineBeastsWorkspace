#include "Nodes/GamePlatformPCGNodes.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "GamePlatformPCGLog.h"
#include "Metadata/PCGMetadata.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Schema/GamePlatformPCGSchema.h"
#include "Services/GamePlatformPCGLinearRules.h"
#include "Services/GamePlatformPCGPriorityRules.h"

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

bool HasAllRequiredAttributes(const UPCGMetadata& Metadata, const TArray<FName>& ExplicitRequired)
{
    if (!ExplicitRequired.IsEmpty())
    {
        for (FName Name : ExplicitRequired)
        {
            if (!FGamePlatformPCGSchema::IsKnownAttribute(Name) || !Metadata.HasAttribute(Name))
            {
                return false;
            }
        }
        return true;
    }

    TSet<FName> Available;
    TArray<FName> Names;
    TArray<EPCGMetadataTypes> Types;
    Metadata.GetAttributes(Names, Types);
    for (FName Name : Names)
    {
        Available.Add(Name);
    }
    return FGamePlatformPCGSchema::ValidateRequiredAttributes(Available).IsSuccess();
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

FPCGElementPtr UGamePlatformPCGWriteSchemaDefaultsSettings::CreateElement() const { return MakeShared<FGamePlatformPCGWriteSchemaDefaultsElement>(); }
FPCGElementPtr UGamePlatformPCGPriorityCarveSettings::CreateElement() const { return MakeShared<FGamePlatformPCGPriorityCarveElement>(); }
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

        EnsureAttribute(*OutputData->Metadata, FGamePlatformPCGAttr::SpawnMeshSetId, Settings->MeshSetId);
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
        if (!Metadata || !HasAllRequiredAttributes(*Metadata, Settings->RequiredAttributes))
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
