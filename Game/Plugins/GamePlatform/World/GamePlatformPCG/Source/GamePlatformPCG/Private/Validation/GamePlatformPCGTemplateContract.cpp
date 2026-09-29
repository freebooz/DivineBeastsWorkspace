#include "Services/GamePlatformPCGTemplateContract.h"

#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGTransformPoints.h"
#include "Nodes/GamePlatformPCGNodes.h"
#include "Schema/GamePlatformPCGSchema.h"

const FName FGamePlatformPCGTemplateIds::Base(TEXT("TPL_Base"));
const FName FGamePlatformPCGTemplateIds::ScatterSurface(TEXT("TPL_ScatterSurface"));
const FName FGamePlatformPCGTemplateIds::BiomeGenerator(TEXT("TPL_BiomeGenerator"));
const FName FGamePlatformPCGTemplateIds::LinearDresser(TEXT("TPL_LinearDresser"));
const FName FGamePlatformPCGTemplateIds::Enclosure(TEXT("TPL_Enclosure"));
const FName FGamePlatformPCGTemplateIds::EnclosureClosed(TEXT("TPL_EnclosureClosed"));
const FName FGamePlatformPCGTemplateIds::Connector(TEXT("TPL_Connector"));
const FName FGamePlatformPCGTemplateIds::GateInsert(TEXT("TPL_GateInsert"));
const FName FGamePlatformPCGTemplateIds::ParcelFill(TEXT("TPL_ParcelFill"));
const FName FGamePlatformPCGTemplateIds::CropField(TEXT("TPL_CropField"));
const FName FGamePlatformPCGTemplateIds::AssemblySpawn(TEXT("TPL_AssemblySpawn"));
const FName FGamePlatformPCGTemplateIds::InterfaceBand(TEXT("TPL_InterfaceBand"));
const FName FGamePlatformPCGTemplateIds::RailingAttached(TEXT("TPL_RailingAttached"));

TConstArrayView<FName> FGamePlatformPCGTemplateIds::All()
{
    static const TArray<FName> Ids =
    {
        Base, ScatterSurface, BiomeGenerator, LinearDresser, Enclosure, EnclosureClosed,
        Connector, GateInsert, ParcelFill, CropField, AssemblySpawn, InterfaceBand, RailingAttached
    };
    return Ids;
}

bool FGamePlatformPCGTemplateIds::IsKnown(FName TemplateId)
{
    return All().Contains(TemplateId);
}

bool FGamePlatformPCGTemplateContract::IsApprovedSettingsClass(const UClass* SettingsClass)
{
    if (!SettingsClass)
    {
        return false;
    }

    static const TSet<const UClass*> Approved =
    {
        UPCGCreatePointsGridSettings::StaticClass(),
        UPCGTransformPointsSettings::StaticClass(),
        UPCGDensityFilterSettings::StaticClass(),
        UPCGStaticMeshSpawnerSettings::StaticClass(),
        UGamePlatformPCGWriteSchemaDefaultsSettings::StaticClass(),
        UGamePlatformPCGPriorityCarveSettings::StaticClass(),
        UGamePlatformPCGProjectAlignSettings::StaticClass(),
        UGamePlatformPCGApplySpawnPolicySettings::StaticClass(),
        UGamePlatformPCGAssignMeshSetSettings::StaticClass(),
        UGamePlatformPCGValidateSchemaSettings::StaticClass(),
        UGamePlatformPCGFitPostsToSplineSettings::StaticClass(),
        UGamePlatformPCGBreakSpansByTagsSettings::StaticClass(),
        UGamePlatformPCGBuildRowsSettings::StaticClass(),
        UGamePlatformPCGSelectSpanMeshByLengthSettings::StaticClass()
    };

    return Approved.Contains(SettingsClass);
}

bool FGamePlatformPCGTemplateContract::IsTemplateHeaderValid(const UGamePlatformPCGProfileDefinition& Profile)
{
    return !Profile.TemplateId.IsNone() &&
           FGamePlatformPCGTemplateIds::IsKnown(Profile.TemplateId) &&
           Profile.TemplateVersion > 0 &&
           Profile.RequiredPCGSchemaMajor == FGamePlatformPCGSchema::CurrentVersion().Major;
}
