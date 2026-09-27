// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformWorldDefinition.h"
#include "Types/GamePlatformId.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformWorldDefinition() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformId(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase);
ENGINE_API UClass* Z_Construct_UClass_UWorld(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformWorld(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformWorldDefinition(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformWorldDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformWorldDefinition *********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformWorldDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\xad\xe7\xab\x8b\xe4\xb8\x96\xe7\x95\x8c\xe5\x86\x85\xe5\xae\xb9\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9bWorldId\xe5\xb0\xb1\xe6\x98\xaf\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84LogicalId\xef\xbc\x8c\xe5\x9c\xb0\xe5\x9b\xbe\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d\xe4\xb8\x8d\xe6\x94\xb9\xe5\x8f\x98\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe3\x80\x82\n * \xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe5\x8f\xaa\xe8\xaf\xbb\xef\xbc\x9b\xe4\xb8\x8d\xe5\x90\xab\xe7\xab\xaf\xe7\x82\xb9\xe3\x80\x81\xe5\x87\x86\xe5\x85\xa5\xe6\x9d\x90\xe6\x96\x99\xe6\x88\x96\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x88\x86\xe9\x85\x8d\xe3\x80\x82""DataVersion\xe4\xb8\x8eRequiredDefinitions\xe6\xb2\xbf\xe7\x94\xa8""Data\xe4\xbd\x93\xe7\xb3\xbb\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformWorldDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformWorldDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\xad\xe7\xab\x8b\xe4\xb8\x96\xe7\x95\x8c\xe5\x86\x85\xe5\xae\xb9\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9bWorldId\xe5\xb0\xb1\xe6\x98\xaf\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84LogicalId\xef\xbc\x8c\xe5\x9c\xb0\xe5\x9b\xbe\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d\xe4\xb8\x8d\xe6\x94\xb9\xe5\x8f\x98\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe3\x80\x82\n\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe5\x8f\xaa\xe8\xaf\xbb\xef\xbc\x9b\xe4\xb8\x8d\xe5\x90\xab\xe7\xab\xaf\xe7\x82\xb9\xe3\x80\x81\xe5\x87\x86\xe5\x85\xa5\xe6\x9d\x90\xe6\x96\x99\xe6\x88\x96\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x88\x86\xe9\x85\x8d\xe3\x80\x82""DataVersion\xe4\xb8\x8eRequiredDefinitions\xe6\xb2\xbf\xe7\x94\xa8""Data\xe4\xbd\x93\xe7\xb3\xbb\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MapIdentity_MetaData[] = {
		{ "Category", "GamePlatform|World" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe5\x9c\xb0\xe5\x9b\xbe\xe7\x9a\x84\xe9\xa1\xb6\xe5\xb1\x82UWorld\xe8\xbd\xaf\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe6\xa0\xa1\xe9\xaa\x8c\xe4\xb8\x8d\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x81\xe4\xb8\x8dTravel\xef\xbc\x8c\xe5\xad\x98\xe5\x9c\xa8\xe6\x80\xa7\xe4\xb8\x8e\xe7\xb1\xbb\xe5\x9e\x8b\xe7\x94\xb1\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe3\x80\x82\n     * \xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe4\xbd\x9c\xe4\xb8\xba\xe8\xb5\x84\xe4\xba\xa7""Bundle\xe9\xa2\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x95\xb4\xe5\xbc\xa0\xe5\x9c\xb0\xe5\x9b\xbe\xef\xbc\x9b\xe5\xae\x9e\xe9\x99\x85\xe5\x9c\xb0\xe5\x9b\xbe\xe8\xbf\x9b\xe5\x85\xa5\xe4\xb8\x8e\xe6\xb5\x81\xe9\x80\x81\xe7\x94\xb1\xe5\x90\x84\xe8\x87\xaa\xe6\x89\x80\xe6\x9c\x89\xe8\x80\x85\xe5\x8d\x8f\xe8\xb0\x83\xe3\x80\x82\n     */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformWorldDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe5\x9c\xb0\xe5\x9b\xbe\xe7\x9a\x84\xe9\xa1\xb6\xe5\xb1\x82UWorld\xe8\xbd\xaf\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe6\xa0\xa1\xe9\xaa\x8c\xe4\xb8\x8d\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x81\xe4\xb8\x8dTravel\xef\xbc\x8c\xe5\xad\x98\xe5\x9c\xa8\xe6\x80\xa7\xe4\xb8\x8e\xe7\xb1\xbb\xe5\x9e\x8b\xe7\x94\xb1\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe3\x80\x82\n\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe4\xbd\x9c\xe4\xb8\xba\xe8\xb5\x84\xe4\xba\xa7""Bundle\xe9\xa2\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x95\xb4\xe5\xbc\xa0\xe5\x9c\xb0\xe5\x9b\xbe\xef\xbc\x9b\xe5\xae\x9e\xe9\x99\x85\xe5\x9c\xb0\xe5\x9b\xbe\xe8\xbf\x9b\xe5\x85\xa5\xe4\xb8\x8e\xe6\xb5\x81\xe9\x80\x81\xe7\x94\xb1\xe5\x90\x84\xe8\x87\xaa\xe6\x89\x80\xe6\x9c\x89\xe8\x80\x85\xe5\x8d\x8f\xe8\xb0\x83\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefaultExperienceId_MetaData[] = {
		{ "Category", "GamePlatform|World" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaf\xe9\x80\x89\xe4\xb8\xad\xe7\xab\x8b\xe4\xbd\x93\xe9\xaa\x8c\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe7\xa9\xba\xe5\x80\xbc\xe8\xa1\xa8\xe7\xa4\xba\xe6\x9c\xaa\xe6\x8c\x87\xe5\xae\x9a\xef\xbc\x8c\xe5\x8d\x8a\xe5\xa1\xab\xe6\x88\x96\xe9\x9d\x9e\xe6\xb3\x95\xe7\x89\x88\xe6\x9c\xac\xe6\x8b\x92\xe7\xbb\x9d\xef\xbc\x8c\xe4\xb8\x8d\xe6\x98\xaf\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe8\xa7\x92\xe8\x89\xb2\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformWorldDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaf\xe9\x80\x89\xe4\xb8\xad\xe7\xab\x8b\xe4\xbd\x93\xe9\xaa\x8c\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe7\xa9\xba\xe5\x80\xbc\xe8\xa1\xa8\xe7\xa4\xba\xe6\x9c\xaa\xe6\x8c\x87\xe5\xae\x9a\xef\xbc\x8c\xe5\x8d\x8a\xe5\xa1\xab\xe6\x88\x96\xe9\x9d\x9e\xe6\xb3\x95\xe7\x89\x88\xe6\x9c\xac\xe6\x8b\x92\xe7\xbb\x9d\xef\xbc\x8c\xe4\xb8\x8d\xe6\x98\xaf\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe8\xa7\x92\xe8\x89\xb2\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Regions_MetaData[] = {
		{ "Category", "GamePlatform|World" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8c\xe5\xbf\x85\xe9\x9c\x80\xe5\x8c\xba\xe5\x9f\x9f\xe7\x9a\x84""Data\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe6\x97\xa0\xe9\x87\x8d\xe5\xa4\x8d\xe4\xb8\x94\xe6\xaf\x8f\xe9\xa1\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe5\x90\x8c\xe6\x97\xb6\xe4\xbd\x8d\xe4\xba\x8eRequiredDefinitions\xe3\x80\x82\n     * \xe8\xbf\x99\xe6\xa0\xb7\xe7\x9c\x9f\xe5\xae\x9e""Data\xe7\xa7\x9f\xe7\xba\xa6\xe9\x80\x92\xe5\xbd\x92\xe6\x8c\x81\xe6\x9c\x89\xe6\x89\x80\xe9\x9c\x80\xe5\x8c\xba\xe5\x9f\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbb\x85\xe5\x87\xad\xe5\x8c\xba\xe5\x9f\x9f\xe5\x88\x97\xe8\xa1\xa8\xe5\x81\x87\xe5\xae\x9a\xe4\xbe\x9d\xe8\xb5\x96\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82\n     */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformWorldDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8c\xe5\xbf\x85\xe9\x9c\x80\xe5\x8c\xba\xe5\x9f\x9f\xe7\x9a\x84""Data\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe6\x97\xa0\xe9\x87\x8d\xe5\xa4\x8d\xe4\xb8\x94\xe6\xaf\x8f\xe9\xa1\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe5\x90\x8c\xe6\x97\xb6\xe4\xbd\x8d\xe4\xba\x8eRequiredDefinitions\xe3\x80\x82\n\xe8\xbf\x99\xe6\xa0\xb7\xe7\x9c\x9f\xe5\xae\x9e""Data\xe7\xa7\x9f\xe7\xba\xa6\xe9\x80\x92\xe5\xbd\x92\xe6\x8c\x81\xe6\x9c\x89\xe6\x89\x80\xe9\x9c\x80\xe5\x8c\xba\xe5\x9f\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbb\x85\xe5\x87\xad\xe5\x8c\xba\xe5\x9f\x9f\xe5\x88\x97\xe8\xa1\xa8\xe5\x81\x87\xe5\xae\x9a\xe4\xbe\x9d\xe8\xb5\x96\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ReadinessTimeoutSeconds_MetaData[] = {
		{ "Category", "GamePlatform|World" },
		{ "ClampMin", "0.001" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x96\xe7\x95\x8c\xe5\xb0\xb1\xe7\xbb\xaa\xe7\xad\x89\xe5\xbe\x85\xe4\xb8\x8a\xe9\x99\x90\xef\xbc\x8c\xe5\x8d\x95\xe4\xbd\x8d\xe7\xa7\x92\xef\xbc\x8c\xe9\xbb\x98\xe8\xae\xa4""60\xef\xbc\x9b\xe5\xbf\x85\xe9\xa1\xbb\xe6\x9c\x89\xe9\x99\x90\xe4\xb8\x94\xe5\xa4\xa7\xe4\xba\x8e\xe9\x9b\xb6\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa5\xe5\x88\xb0\xe6\x9c\x9f\xe4\xbb\xa3\xe6\x9b\xbf\xe7\x9c\x9f\xe5\xae\x9e\xe5\xb0\xb1\xe7\xbb\xaa\xe3\x80\x82\n     * \xe8\xbf\x99\xe9\x87\x8c\xe5\x8f\xaa\xe5\xa3\xb0\xe6\x98\x8e\xe7\xad\x96\xe7\x95\xa5\xef\xbc\x8c\xe8\xbf\x90\xe8\xa1\x8c\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe8\xb4\x9f\xe8\xb4\xa3\xe5\x9f\xba\xe4\xba\x8e\xe5\x8d\x95\xe8\xb0\x83\xe6\x97\xb6\xe9\x97\xb4\xe6\x89\xa7\xe8\xa1\x8c\xe6\x88\xaa\xe6\xad\xa2\xe4\xb8\x8e\xe5\xa4\xb1\xe8\xb4\xa5\xe6\x94\xb6\xe6\x95\x9b\xe3\x80\x82\n     */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformWorldDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x96\xe7\x95\x8c\xe5\xb0\xb1\xe7\xbb\xaa\xe7\xad\x89\xe5\xbe\x85\xe4\xb8\x8a\xe9\x99\x90\xef\xbc\x8c\xe5\x8d\x95\xe4\xbd\x8d\xe7\xa7\x92\xef\xbc\x8c\xe9\xbb\x98\xe8\xae\xa4""60\xef\xbc\x9b\xe5\xbf\x85\xe9\xa1\xbb\xe6\x9c\x89\xe9\x99\x90\xe4\xb8\x94\xe5\xa4\xa7\xe4\xba\x8e\xe9\x9b\xb6\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa5\xe5\x88\xb0\xe6\x9c\x9f\xe4\xbb\xa3\xe6\x9b\xbf\xe7\x9c\x9f\xe5\xae\x9e\xe5\xb0\xb1\xe7\xbb\xaa\xe3\x80\x82\n\xe8\xbf\x99\xe9\x87\x8c\xe5\x8f\xaa\xe5\xa3\xb0\xe6\x98\x8e\xe7\xad\x96\xe7\x95\xa5\xef\xbc\x8c\xe8\xbf\x90\xe8\xa1\x8c\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe8\xb4\x9f\xe8\xb4\xa3\xe5\x9f\xba\xe4\xba\x8e\xe5\x8d\x95\xe8\xb0\x83\xe6\x97\xb6\xe9\x97\xb4\xe6\x89\xa7\xe8\xa1\x8c\xe6\x88\xaa\xe6\xad\xa2\xe4\xb8\x8e\xe5\xa4\xb1\xe8\xb4\xa5\xe6\x94\xb6\xe6\x95\x9b\xe3\x80\x82" },
#endif
		{ "Units", "s" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformWorldDefinition constinit property declarations *************
	static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_MapIdentity;
	static const UECodeGen_Private::FStructPropertyParams NewProp_DefaultExperienceId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Regions_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_Regions;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ReadinessTimeoutSeconds;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformWorldDefinition constinit property declarations ***************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformWorldDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformWorldDefinition Property Definitions ************************
const UECodeGen_Private::FSoftObjectPropertyParams UHT_STATICS::NewProp_MapIdentity = { "MapIdentity", nullptr, (EPropertyFlags)0x0014000000010015, UECodeGen_Private::EPropertyGenFlags::SoftObject, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformWorldDefinition, MapIdentity), Z_Construct_UClass_UWorld, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MapIdentity_MetaData), NewProp_MapIdentity_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_DefaultExperienceId = { "DefaultExperienceId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformWorldDefinition, DefaultExperienceId), Z_Construct_UScriptStruct_FGamePlatformId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefaultExperienceId_MetaData), NewProp_DefaultExperienceId_MetaData) }; // 286244d92f71be2940db0b00e936e92e3a915491
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Regions_Inner = { "Regions", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(0, nullptr) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_Regions = { "Regions", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformWorldDefinition, Regions), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Regions_MetaData), NewProp_Regions_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_ReadinessTimeoutSeconds = { "ReadinessTimeoutSeconds", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformWorldDefinition, ReadinessTimeoutSeconds), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ReadinessTimeoutSeconds_MetaData), NewProp_ReadinessTimeoutSeconds_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MapIdentity,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefaultExperienceId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Regions_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Regions,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReadinessTimeoutSeconds,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformWorldDefinition Property Definitions **************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformDefinitionBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorld,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformWorldDefinition,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformWorldDefinition;
UClass* Z_Construct_UClass_UGamePlatformWorldDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformWorldDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformWorldDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformWorldDefinition"),
				Z_Registration_Info_UClass_UGamePlatformWorldDefinition.InnerSingleton,
				nullptr,
				DataSizeOf<TClass>(),
				alignof(TClass),
				TClass::StaticClassFlags,
				TClass::StaticClassCastFlags(),
				TClass::StaticConfigName(),
				(UClass::ClassConstructorType)InternalConstructor<TClass>,
				(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
				UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
				&TClass::Super::StaticClass,
				&TClass::WithinClass::StaticClass
			);
		}
		return Z_Registration_Info_UClass_UGamePlatformWorldDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformWorldDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformWorldDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformWorldDefinition.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformWorldDefinition::UGamePlatformWorldDefinition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformWorldDefinition);
UGamePlatformWorldDefinition::~UGamePlatformWorldDefinition() {}
// ********** End Class UGamePlatformWorldDefinition ***********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h__Script_GamePlatformWorld_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformWorldDefinition, TEXT("UGamePlatformWorldDefinition"), &Z_Registration_Info_UClass_UGamePlatformWorldDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformWorldDefinition), 62733691U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h__Script_GamePlatformWorld_728a5efb893383680ae20062fe07b5912bfb7068{
	TEXT("/Script/GamePlatformWorld"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
