// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformRegionDefinition() {}

// ********** Begin Cross Module References ********************************************************
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformId(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTagContainer(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformWorld(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UEnum* Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UEnum* Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformRegionDefinition(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformRegionDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EGamePlatformRegionBoundsPolicy *******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy_Statics
template<> GAMEPLATFORMWORLD_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformRegionBoundsPolicy>()
{
	return Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "AxisAlignedBox.Comment", "/** \xe8\xbe\xb9\xe7\x95\x8c\xe7\x94\xb1\xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8c\xe6\xb3\xa8\xe5\x86\x8cProvider\xe6\x8f\x90\xe4\xbe\x9b\xe8\xbd\xb4\xe5\xaf\xb9\xe9\xbd\x90\xe7\x9b\x92\xef\xbc\x9b\xe5\xae\x9a\xe4\xb9\x89\xe8\xb5\x84\xe4\xba\xa7\xe6\x9c\xac\xe8\xba\xab\xe4\xb8\x8d\xe4\xbf\x9d\xe5\xad\x98""Actor\xe5\xae\x9e\xe4\xbe\x8b\xe6\x88\x96\xe4\xbd\x8d\xe7\xbd\xae\xe3\x80\x82 */" },
		{ "AxisAlignedBox.Name", "EGamePlatformRegionBoundsPolicy::AxisAlignedBox" },
		{ "AxisAlignedBox.ToolTip", "\xe8\xbe\xb9\xe7\x95\x8c\xe7\x94\xb1\xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8c\xe6\xb3\xa8\xe5\x86\x8cProvider\xe6\x8f\x90\xe4\xbe\x9b\xe8\xbd\xb4\xe5\xaf\xb9\xe9\xbd\x90\xe7\x9b\x92\xef\xbc\x9b\xe5\xae\x9a\xe4\xb9\x89\xe8\xb5\x84\xe4\xba\xa7\xe6\x9c\xac\xe8\xba\xab\xe4\xb8\x8d\xe4\xbf\x9d\xe5\xad\x98""Actor\xe5\xae\x9e\xe4\xbe\x8b\xe6\x88\x96\xe4\xbd\x8d\xe7\xbd\xae\xe3\x80\x82" },
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xbd\x93\xe5\x89\x8d\xe5\x8c\xba\xe5\x9f\x9f\xe8\xbe\xb9\xe7\x95\x8c\xe8\x83\xbd\xe5\x8a\x9b\xef\xbc\x9b\xe4\xb8\x8d\xe7\xad\x89\xe5\x90\x8c\xe4\xba\x8eWorld Partition\xe5\x8d\x95\xe5\x85\x83\xef\xbc\x8c\xe4\xb9\x9f\xe4\xb8\x8d\xe6\x8f\x90\xe4\xbe\x9b\xe5\xb0\x9a\xe6\x9c\xaa\xe5\xae\x9e\xe7\x8e\xb0\xe7\x9a\x84\xe5\xbd\xa2\xe7\x8a\xb6\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xbd\x93\xe5\x89\x8d\xe5\x8c\xba\xe5\x9f\x9f\xe8\xbe\xb9\xe7\x95\x8c\xe8\x83\xbd\xe5\x8a\x9b\xef\xbc\x9b\xe4\xb8\x8d\xe7\xad\x89\xe5\x90\x8c\xe4\xba\x8eWorld Partition\xe5\x8d\x95\xe5\x85\x83\xef\xbc\x8c\xe4\xb9\x9f\xe4\xb8\x8d\xe6\x8f\x90\xe4\xbe\x9b\xe5\xb0\x9a\xe6\x9c\xaa\xe5\xae\x9e\xe7\x8e\xb0\xe7\x9a\x84\xe5\xbd\xa2\xe7\x8a\xb6\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformRegionBoundsPolicy::AxisAlignedBox", (int64)EGamePlatformRegionBoundsPolicy::AxisAlignedBox },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorld,
	nullptr,
	"EGamePlatformRegionBoundsPolicy",
	"EGamePlatformRegionBoundsPolicy",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformRegionBoundsPolicy;
UEnum* Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformRegionBoundsPolicy.OuterSingleton)
		{
			ZRIE_EGamePlatformRegionBoundsPolicy.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy, (UObject*)Z_Construct_UPackage__Script_GamePlatformWorld(ETypeConstructPhase::Outer), TEXT("EGamePlatformRegionBoundsPolicy"));
		}
		return ZRIE_EGamePlatformRegionBoundsPolicy.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformRegionBoundsPolicy.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformRegionBoundsPolicy.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformRegionBoundsPolicy.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformRegionBoundsPolicy *********************************************

// ********** Begin Enum EGamePlatformRegionActivationPolicy ***************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy_Statics
template<> GAMEPLATFORMWORLD_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformRegionActivationPolicy>()
{
	return Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "AlwaysRegistered.Comment", "/** Provider\xe6\x9c\x89\xe6\x95\x88\xe6\xb3\xa8\xe5\x86\x8c\xe6\x9c\x9f\xe9\x97\xb4\xe5\x8f\x82\xe4\xb8\x8e\xe6\x9f\xa5\xe8\xaf\xa2\xef\xbc\x8c\xe6\x92\xa4\xe9\x94\x80/\xe9\x94\x80\xe6\xaf\x81\xe5\x90\x8e\xe9\x80\x80\xe5\x87\xba\xef\xbc\x9b\xe4\xb8\x8d\xe5\xbc\xba\xe5\x88\xb6\xe8\xaf\xa5Provider\xe6\xb0\xb8\xe4\xb9\x85\xe9\xa9\xbb\xe7\x95\x99\xe5\x86\x85\xe5\xad\x98\xe3\x80\x82 */" },
		{ "AlwaysRegistered.Name", "EGamePlatformRegionActivationPolicy::AlwaysRegistered" },
		{ "AlwaysRegistered.ToolTip", "Provider\xe6\x9c\x89\xe6\x95\x88\xe6\xb3\xa8\xe5\x86\x8c\xe6\x9c\x9f\xe9\x97\xb4\xe5\x8f\x82\xe4\xb8\x8e\xe6\x9f\xa5\xe8\xaf\xa2\xef\xbc\x8c\xe6\x92\xa4\xe9\x94\x80/\xe9\x94\x80\xe6\xaf\x81\xe5\x90\x8e\xe9\x80\x80\xe5\x87\xba\xef\xbc\x9b\xe4\xb8\x8d\xe5\xbc\xba\xe5\x88\xb6\xe8\xaf\xa5Provider\xe6\xb0\xb8\xe4\xb9\x85\xe9\xa9\xbb\xe7\x95\x99\xe5\x86\x85\xe5\xad\x98\xe3\x80\x82" },
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8c\xba\xe5\x9f\x9f\xe5\x8f\xaf\xe5\x8f\x82\xe4\xb8\x8e\xe6\x9f\xa5\xe8\xaf\xa2\xe7\x9a\x84\xe6\x97\xb6\xe6\x9c\xba\xef\xbc\x9b\xe5\x8f\xaa\xe8\xa1\xa8\xe8\xbe\xbe\xe5\xb9\xb3\xe5\x8f\xb0\xe5\xb7\xb2\xe5\xae\x9e\xe7\x8e\xb0\xe8\x83\xbd\xe5\x8a\x9b\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa3\xe8\xa1\xa8\xe6\xb5\x81\xe9\x80\x81\xe5\xae\x8c\xe6\x88\x90\xe6\x88\x96\xe7\xbd\x91\xe7\xbb\x9c\xe5\x87\x86\xe5\x85\xa5\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8c\xba\xe5\x9f\x9f\xe5\x8f\xaf\xe5\x8f\x82\xe4\xb8\x8e\xe6\x9f\xa5\xe8\xaf\xa2\xe7\x9a\x84\xe6\x97\xb6\xe6\x9c\xba\xef\xbc\x9b\xe5\x8f\xaa\xe8\xa1\xa8\xe8\xbe\xbe\xe5\xb9\xb3\xe5\x8f\xb0\xe5\xb7\xb2\xe5\xae\x9e\xe7\x8e\xb0\xe8\x83\xbd\xe5\x8a\x9b\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa3\xe8\xa1\xa8\xe6\xb5\x81\xe9\x80\x81\xe5\xae\x8c\xe6\x88\x90\xe6\x88\x96\xe7\xbd\x91\xe7\xbb\x9c\xe5\x87\x86\xe5\x85\xa5\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformRegionActivationPolicy::AlwaysRegistered", (int64)EGamePlatformRegionActivationPolicy::AlwaysRegistered },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorld,
	nullptr,
	"EGamePlatformRegionActivationPolicy",
	"EGamePlatformRegionActivationPolicy",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformRegionActivationPolicy;
UEnum* Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformRegionActivationPolicy.OuterSingleton)
		{
			ZRIE_EGamePlatformRegionActivationPolicy.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy, (UObject*)Z_Construct_UPackage__Script_GamePlatformWorld(ETypeConstructPhase::Outer), TEXT("EGamePlatformRegionActivationPolicy"));
		}
		return ZRIE_EGamePlatformRegionActivationPolicy.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformRegionActivationPolicy.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformRegionActivationPolicy.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformRegionActivationPolicy.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformRegionActivationPolicy *****************************************

// ********** Begin Class UGamePlatformRegionDefinition ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformRegionDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\xad\xe7\xab\x8b\xe9\x80\xbb\xe8\xbe\x91\xe5\x8c\xba\xe5\x9f\x9f\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9bRegionId\xe5\xb0\xb1\xe6\x98\xaf\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84LogicalId\xef\xbc\x8c\xe6\x97\xa2\xe4\xb8\x8d\xe6\x98\xafShard\xe4\xb9\x9f\xe4\xb8\x8d\xe6\x98\xaf\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82\n * \xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe5\x8f\xaa\xe8\xaf\xbb\xef\xbc\x9b\xe8\xb5\x84\xe6\xba\x90\xe9\x9c\x80\xe6\xb1\x82\xe6\xb2\xbf\xe7\x94\xa8RequiredDefinitions\xef\xbc\x8c\xe8\xbf\x90\xe8\xa1\x8c\xe8\xbe\xb9\xe7\x95\x8c\xe5\x92\x8c\xe8\xa7\x82\xe5\xaf\x9f\xe4\xb8\xbb\xe4\xbd\x93\xe7\x94\xb1\xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8cProvider\xe6\x8f\x90\xe4\xbe\x9b\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformRegionDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\xad\xe7\xab\x8b\xe9\x80\xbb\xe8\xbe\x91\xe5\x8c\xba\xe5\x9f\x9f\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9bRegionId\xe5\xb0\xb1\xe6\x98\xaf\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84LogicalId\xef\xbc\x8c\xe6\x97\xa2\xe4\xb8\x8d\xe6\x98\xafShard\xe4\xb9\x9f\xe4\xb8\x8d\xe6\x98\xaf\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82\n\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe5\x8f\xaa\xe8\xaf\xbb\xef\xbc\x9b\xe8\xb5\x84\xe6\xba\x90\xe9\x9c\x80\xe6\xb1\x82\xe6\xb2\xbf\xe7\x94\xa8RequiredDefinitions\xef\xbc\x8c\xe8\xbf\x90\xe8\xa1\x8c\xe8\xbe\xb9\xe7\x95\x8c\xe5\x92\x8c\xe8\xa7\x82\xe5\xaf\x9f\xe4\xb8\xbb\xe4\xbd\x93\xe7\x94\xb1\xe6\x9c\xac\xe4\xb8\x96\xe7\x95\x8cProvider\xe6\x8f\x90\xe4\xbe\x9b\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ParentRegionId_MetaData[] = {
		{ "Category", "GamePlatform|World|Region" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaf\xe9\x80\x89\xe7\x88\xb6\xe5\x8c\xba\xe5\x9f\x9f\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe7\xa9\xba\xe5\x80\xbc\xe8\xa1\xa8\xe7\xa4\xba\xe6\xa0\xb9\xe5\x8c\xba\xe5\x9f\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe8\x87\xaa\xe6\x8c\x87\xe3\x80\x82\n     * \xe7\x88\xb6\xe5\x85\xb3\xe7\xb3\xbb\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe5\xbd\xa2\xe6\x88\x90\xe8\xb5\x84\xe4\xba\xa7\xe7\xa7\x9f\xe7\xba\xa6\xef\xbc\x9b\xe5\xae\x9e\xe9\x99\x85\xe4\xbd\xbf\xe7\x94\xa8\xe7\x9a\x84\xe7\x88\xb6\xe5\xae\x9a\xe4\xb9\x89\xe9\xa1\xbb\xe7\x94\xb1\xe6\xb6\x88\xe8\xb4\xb9\xe6\x96\xb9\xe7\xba\xb3\xe5\x85\xa5RequiredDefinitions\xef\xbc\x8c\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe5\xae\x8c\xe6\x95\xb4\xe7\x88\xb6\xe9\x93\xbe\xe3\x80\x82\n     */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaf\xe9\x80\x89\xe7\x88\xb6\xe5\x8c\xba\xe5\x9f\x9f\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe7\xa9\xba\xe5\x80\xbc\xe8\xa1\xa8\xe7\xa4\xba\xe6\xa0\xb9\xe5\x8c\xba\xe5\x9f\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe8\x87\xaa\xe6\x8c\x87\xe3\x80\x82\n\xe7\x88\xb6\xe5\x85\xb3\xe7\xb3\xbb\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe5\xbd\xa2\xe6\x88\x90\xe8\xb5\x84\xe4\xba\xa7\xe7\xa7\x9f\xe7\xba\xa6\xef\xbc\x9b\xe5\xae\x9e\xe9\x99\x85\xe4\xbd\xbf\xe7\x94\xa8\xe7\x9a\x84\xe7\x88\xb6\xe5\xae\x9a\xe4\xb9\x89\xe9\xa1\xbb\xe7\x94\xb1\xe6\xb6\x88\xe8\xb4\xb9\xe6\x96\xb9\xe7\xba\xb3\xe5\x85\xa5RequiredDefinitions\xef\xbc\x8c\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe5\xae\x8c\xe6\x95\xb4\xe7\x88\xb6\xe9\x93\xbe\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RegionTypeTag_MetaData[] = {
		{ "Category", "GamePlatform|World|Region" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xbf\x85\xe5\xa1\xab\xe7\x9a\x84\xe4\xb8\xad\xe7\xab\x8b\xe5\x8c\xba\xe5\x9f\x9f\xe8\xaf\xad\xe4\xb9\x89\xe5\x90\x8d\xef\xbc\x9b\xe4\xb8\x8d\xe6\x98\xaf""FGameplayTag\xef\xbc\x8c\xe4\xb8\x8d\xe8\xa6\x81\xe6\xb1\x82\xe9\xa1\xb9\xe7\x9b\xae\xe6\xa0\x87\xe7\xad\xbe\xe6\xb3\xa8\xe5\x86\x8c\xef\xbc\x8c\xe4\xb8\x8d\xe5\x86\x85\xe7\xbd\xae\xe5\xa4\xa7\xe5\x8e\x85/\xe7\xab\x9e\xe6\x8a\x80\xe8\xa7\x84\xe5\x88\x99\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xbf\x85\xe5\xa1\xab\xe7\x9a\x84\xe4\xb8\xad\xe7\xab\x8b\xe5\x8c\xba\xe5\x9f\x9f\xe8\xaf\xad\xe4\xb9\x89\xe5\x90\x8d\xef\xbc\x9b\xe4\xb8\x8d\xe6\x98\xaf""FGameplayTag\xef\xbc\x8c\xe4\xb8\x8d\xe8\xa6\x81\xe6\xb1\x82\xe9\xa1\xb9\xe7\x9b\xae\xe6\xa0\x87\xe7\xad\xbe\xe6\xb3\xa8\xe5\x86\x8c\xef\xbc\x8c\xe4\xb8\x8d\xe5\x86\x85\xe7\xbd\xae\xe5\xa4\xa7\xe5\x8e\x85/\xe7\xab\x9e\xe6\x8a\x80\xe8\xa7\x84\xe5\x88\x99\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_BoundsPolicy_MetaData[] = {
		{ "Category", "GamePlatform|World|Region" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xbe\xb9\xe7\x95\x8c\xe8\xa7\xa3\xe9\x87\x8a\xe6\x96\xb9\xe5\xbc\x8f\xef\xbc\x9b\xe7\xac\xac\xe4\xb8\x80\xe7\x89\x88\xe5\x8f\xaa\xe6\x94\xaf\xe6\x8c\x81\xe8\xbd\xb4\xe5\xaf\xb9\xe9\xbd\x90\xe7\x9b\x92\xef\xbc\x8c\xe9\x9d\x9e\xe6\xb3\x95\xe6\x9e\x9a\xe4\xb8\xbe\xe5\x80\xbc\xe6\x98\x8e\xe7\xa1\xae\xe5\xa4\xb1\xe8\xb4\xa5\xe8\x80\x8c\xe9\x9d\x9e\xe5\x9b\x9e\xe9\x80\x80\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xbe\xb9\xe7\x95\x8c\xe8\xa7\xa3\xe9\x87\x8a\xe6\x96\xb9\xe5\xbc\x8f\xef\xbc\x9b\xe7\xac\xac\xe4\xb8\x80\xe7\x89\x88\xe5\x8f\xaa\xe6\x94\xaf\xe6\x8c\x81\xe8\xbd\xb4\xe5\xaf\xb9\xe9\xbd\x90\xe7\x9b\x92\xef\xbc\x8c\xe9\x9d\x9e\xe6\xb3\x95\xe6\x9e\x9a\xe4\xb8\xbe\xe5\x80\xbc\xe6\x98\x8e\xe7\xa1\xae\xe5\xa4\xb1\xe8\xb4\xa5\xe8\x80\x8c\xe9\x9d\x9e\xe5\x9b\x9e\xe9\x80\x80\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ActivationPolicy_MetaData[] = {
		{ "Category", "GamePlatform|World|Region" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\xb3\xa8\xe5\x86\x8c\xe6\x9c\x9f\xe9\x97\xb4\xe7\x9a\x84\xe6\x9f\xa5\xe8\xaf\xa2\xe8\xb5\x84\xe6\xa0\xbc\xef\xbc\x9b\xe7\xac\xac\xe4\xb8\x80\xe7\x89\x88\xe4\xbb\x85\xe6\x94\xaf\xe6\x8c\x81""AlwaysRegistered\xef\xbc\x8c\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe6\xbf\x80\xe6\xb4\xbb\xe6\xb5\x81\xe9\x80\x81\xe6\x88\x96\xe7\x8e\xa9\xe6\xb3\x95\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xb3\xa8\xe5\x86\x8c\xe6\x9c\x9f\xe9\x97\xb4\xe7\x9a\x84\xe6\x9f\xa5\xe8\xaf\xa2\xe8\xb5\x84\xe6\xa0\xbc\xef\xbc\x9b\xe7\xac\xac\xe4\xb8\x80\xe7\x89\x88\xe4\xbb\x85\xe6\x94\xaf\xe6\x8c\x81""AlwaysRegistered\xef\xbc\x8c\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe6\xbf\x80\xe6\xb4\xbb\xe6\xb5\x81\xe9\x80\x81\xe6\x88\x96\xe7\x8e\xa9\xe6\xb3\x95\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_GameplayTags_MetaData[] = {
		{ "Category", "GamePlatform|World|Region" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaf\xe9\x80\x89\xe4\xb8\xad\xe7\xab\x8bGameplay\xe6\xa0\x87\xe7\xad\xbe\xe9\x9b\x86\xe5\x90\x88\xef\xbc\x9b\xe6\xa0\x87\xe7\xad\xbe\xe6\xb3\xa8\xe5\x86\x8c\xe6\xb2\xbf\xe7\x94\xa8\xe5\xbc\x95\xe6\x93\x8e\xe6\x9c\xba\xe5\x88\xb6\xef\xbc\x8c\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe6\xa0\x87\xe7\xad\xbe\xe4\xb8\x8d\xe8\x83\xbd\xe6\x8e\x88\xe6\x9d\x83\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe7\x8e\xa9\xe6\xb3\x95\xe7\xbb\x93\xe6\x9e\x9c\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformRegionDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaf\xe9\x80\x89\xe4\xb8\xad\xe7\xab\x8bGameplay\xe6\xa0\x87\xe7\xad\xbe\xe9\x9b\x86\xe5\x90\x88\xef\xbc\x9b\xe6\xa0\x87\xe7\xad\xbe\xe6\xb3\xa8\xe5\x86\x8c\xe6\xb2\xbf\xe7\x94\xa8\xe5\xbc\x95\xe6\x93\x8e\xe6\x9c\xba\xe5\x88\xb6\xef\xbc\x8c\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe6\xa0\x87\xe7\xad\xbe\xe4\xb8\x8d\xe8\x83\xbd\xe6\x8e\x88\xe6\x9d\x83\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe7\x8e\xa9\xe6\xb3\x95\xe7\xbb\x93\xe6\x9e\x9c\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformRegionDefinition constinit property declarations ************
	static const UECodeGen_Private::FStructPropertyParams NewProp_ParentRegionId;
	static const UECodeGen_Private::FNamePropertyParams NewProp_RegionTypeTag;
	static const UECodeGen_Private::FBytePropertyParams NewProp_BoundsPolicy_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_BoundsPolicy;
	static const UECodeGen_Private::FBytePropertyParams NewProp_ActivationPolicy_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ActivationPolicy;
	static const UECodeGen_Private::FStructPropertyParams NewProp_GameplayTags;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformRegionDefinition constinit property declarations **************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformRegionDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformRegionDefinition Property Definitions ***********************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ParentRegionId = { "ParentRegionId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformRegionDefinition, ParentRegionId), Z_Construct_UScriptStruct_FGamePlatformId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ParentRegionId_MetaData), NewProp_ParentRegionId_MetaData) }; // 286244d92f71be2940db0b00e936e92e3a915491
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_RegionTypeTag = { "RegionTypeTag", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformRegionDefinition, RegionTypeTag), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RegionTypeTag_MetaData), NewProp_RegionTypeTag_MetaData) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_BoundsPolicy_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_BoundsPolicy = { "BoundsPolicy", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformRegionDefinition, BoundsPolicy), Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_BoundsPolicy_MetaData), NewProp_BoundsPolicy_MetaData) }; // 20feda1fb16208906d3618707acac453f7602ae4
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_ActivationPolicy_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_ActivationPolicy = { "ActivationPolicy", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformRegionDefinition, ActivationPolicy), Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ActivationPolicy_MetaData), NewProp_ActivationPolicy_MetaData) }; // 47437784e7af967e7800eee8397572d5899e9b73
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_GameplayTags = { "GameplayTags", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformRegionDefinition, GameplayTags), Z_Construct_UScriptStruct_FGameplayTagContainer, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_GameplayTags_MetaData), NewProp_GameplayTags_MetaData) }; // 93faf2d4041600295d23f175e0992095f880d07b
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ParentRegionId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RegionTypeTag,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_BoundsPolicy_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_BoundsPolicy,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ActivationPolicy_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ActivationPolicy,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_GameplayTags,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformRegionDefinition Property Definitions *************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformDefinitionBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorld,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformRegionDefinition,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformRegionDefinition;
UClass* Z_Construct_UClass_UGamePlatformRegionDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformRegionDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformRegionDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformRegionDefinition"),
				Z_Registration_Info_UClass_UGamePlatformRegionDefinition.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformRegionDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformRegionDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformRegionDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformRegionDefinition.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformRegionDefinition::UGamePlatformRegionDefinition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformRegionDefinition);
UGamePlatformRegionDefinition::~UGamePlatformRegionDefinition() {}
// ********** End Class UGamePlatformRegionDefinition **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h__Script_GamePlatformWorld_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionBoundsPolicy, TEXT("EGamePlatformRegionBoundsPolicy"), &ZRIE_EGamePlatformRegionBoundsPolicy, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 553572895U) },
		{ Z_Construct_UEnum_GamePlatformWorld_EGamePlatformRegionActivationPolicy, TEXT("EGamePlatformRegionActivationPolicy"), &ZRIE_EGamePlatformRegionActivationPolicy, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1195603844U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformRegionDefinition, TEXT("UGamePlatformRegionDefinition"), &Z_Registration_Info_UClass_UGamePlatformRegionDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformRegionDefinition), 2202545759U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h__Script_GamePlatformWorld_89725afdf802251f66d634c652c07f9ae7515892{
	TEXT("/Script/GamePlatformWorld"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
