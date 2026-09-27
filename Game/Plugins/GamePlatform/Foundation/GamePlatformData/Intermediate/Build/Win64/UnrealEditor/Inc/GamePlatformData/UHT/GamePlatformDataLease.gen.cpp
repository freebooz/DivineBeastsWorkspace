// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformDataLease.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformDataLease() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FGuid(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UEnum* Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime(ETypeConstructPhase);
GAMEPLATFORMDATA_API UEnum* Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState(ETypeConstructPhase);
GAMEPLATFORMDATA_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataLease(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EGamePlatformDataLifetime *************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime_Statics
template<> GAMEPLATFORMDATA_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformDataLifetime>()
{
	return Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xb5\x84\xe6\xba\x90\xe4\xbd\xbf\xe7\x94\xa8\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\xae\x9e\xe4\xbe\x8b\xe7\xa7\x9f\xe7\xba\xa6\xe8\xb7\xa8\xe5\x88\x87\xe5\x9b\xbe\xef\xbc\x8c\xe4\xb8\x96\xe7\x95\x8c\xe7\xa7\x9f\xe7\xba\xa6\xe7\xbb\x91\xe5\xae\x9a\xe7\x94\xb3\xe8\xaf\xb7\xe6\x97\xb6\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe7\x9a\x84\xe6\x89\x80\xe5\xb1\x9e\xe4\xb8\x96\xe7\x95\x8c\xe3\x80\x82 */" },
#endif
		{ "Instance.Comment", "/** \xe5\xbd\x93\xe5\x89\x8d\xe6\xb8\xb8\xe6\x88\x8f\xe5\xae\x9e\xe4\xbe\x8b\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\x88\x87\xe5\x9b\xbe\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe6\x92\xa4\xe9\x94\x80\xef\xbc\x8c\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe4\xbb\x8d\xe9\xa1\xbb\xe5\xad\x98\xe6\xb4\xbb\xe3\x80\x82 */" },
		{ "Instance.Name", "EGamePlatformDataLifetime::Instance" },
		{ "Instance.ToolTip", "\xe5\xbd\x93\xe5\x89\x8d\xe6\xb8\xb8\xe6\x88\x8f\xe5\xae\x9e\xe4\xbe\x8b\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\x88\x87\xe5\x9b\xbe\xe4\xb8\x8d\xe8\x87\xaa\xe5\x8a\xa8\xe6\x92\xa4\xe9\x94\x80\xef\xbc\x8c\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe4\xbb\x8d\xe9\xa1\xbb\xe5\xad\x98\xe6\xb4\xbb\xe3\x80\x82" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xb5\x84\xe6\xba\x90\xe4\xbd\xbf\xe7\x94\xa8\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\xae\x9e\xe4\xbe\x8b\xe7\xa7\x9f\xe7\xba\xa6\xe8\xb7\xa8\xe5\x88\x87\xe5\x9b\xbe\xef\xbc\x8c\xe4\xb8\x96\xe7\x95\x8c\xe7\xa7\x9f\xe7\xba\xa6\xe7\xbb\x91\xe5\xae\x9a\xe7\x94\xb3\xe8\xaf\xb7\xe6\x97\xb6\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe7\x9a\x84\xe6\x89\x80\xe5\xb1\x9e\xe4\xb8\x96\xe7\x95\x8c\xe3\x80\x82" },
#endif
		{ "World.Comment", "/** \xe7\x94\xb3\xe8\xaf\xb7\xe6\x97\xb6\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\x8f\xaa\xe7\x94\xb1\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x96\xe7\x95\x8c\xe6\xb8\x85\xe7\x90\x86\xe6\x88\x96\xe6\x98\xbe\xe5\xbc\x8f\xe9\x87\x8a\xe6\x94\xbe\xe6\x92\xa4\xe9\x94\x80\xe3\x80\x82 */" },
		{ "World.Name", "EGamePlatformDataLifetime::World" },
		{ "World.ToolTip", "\xe7\x94\xb3\xe8\xaf\xb7\xe6\x97\xb6\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x9b\xe5\x8f\xaa\xe7\x94\xb1\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x96\xe7\x95\x8c\xe6\xb8\x85\xe7\x90\x86\xe6\x88\x96\xe6\x98\xbe\xe5\xbc\x8f\xe9\x87\x8a\xe6\x94\xbe\xe6\x92\xa4\xe9\x94\x80\xe3\x80\x82" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformDataLifetime::Instance", (int64)EGamePlatformDataLifetime::Instance },
		{ "EGamePlatformDataLifetime::World", (int64)EGamePlatformDataLifetime::World },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
	nullptr,
	"EGamePlatformDataLifetime",
	"EGamePlatformDataLifetime",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformDataLifetime;
UEnum* Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformDataLifetime.OuterSingleton)
		{
			ZRIE_EGamePlatformDataLifetime.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime, (UObject*)Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase::Outer), TEXT("EGamePlatformDataLifetime"));
		}
		return ZRIE_EGamePlatformDataLifetime.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformDataLifetime.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformDataLifetime.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformDataLifetime.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformDataLifetime ***************************************************

// ********** Begin Enum EGamePlatformDataRequestState *********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState_Statics
template<> GAMEPLATFORMDATA_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformDataRequestState>()
{
	return Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "Cancelled.Comment", "/** \xe5\xae\x8c\xe6\x88\x90\xe9\x80\x9a\xe7\x9f\xa5\xe4\xb8\xad\xe7\x9a\x84\xe5\x8f\x96\xe6\xb6\x88\xe5\xbf\xab\xe7\x85\xa7\xef\xbc\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe8\xae\xb0\xe5\xbd\x95\xe5\xb7\xb2\xe6\x92\xa4\xe9\x94\x80\xef\xbc\x8c\xe6\x9f\xa5\xe8\xaf\xa2\xe8\xbf\x94\xe5\x9b\x9eReleased\xe3\x80\x82 */" },
		{ "Cancelled.Name", "EGamePlatformDataRequestState::Cancelled" },
		{ "Cancelled.ToolTip", "\xe5\xae\x8c\xe6\x88\x90\xe9\x80\x9a\xe7\x9f\xa5\xe4\xb8\xad\xe7\x9a\x84\xe5\x8f\x96\xe6\xb6\x88\xe5\xbf\xab\xe7\x85\xa7\xef\xbc\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe8\xae\xb0\xe5\xbd\x95\xe5\xb7\xb2\xe6\x92\xa4\xe9\x94\x80\xef\xbc\x8c\xe6\x9f\xa5\xe8\xaf\xa2\xe8\xbf\x94\xe5\x9b\x9eReleased\xe3\x80\x82" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xaf\xb7\xe6\xb1\x82\xe7\x8a\xb6\xe6\x80\x81\xe4\xb8\x8e\xe8\xb5\x84\xe6\xba\x90\xe4\xbd\xbf\xe7\x94\xa8\xe6\x9c\x9f\xe9\x99\x90\xe5\x88\x86\xe5\xbc\x80\xef\xbc\x9bSucceeded\xe4\xbb\x8d\xe6\x8c\x81\xe6\x9c\x89\xe8\xb5\x84\xe6\xba\x90\xef\xbc\x8cReleased\xe4\xb8\x8d\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82 */" },
#endif
		{ "Failed.Comment", "/** \xe6\x9c\xac\xe6\xac\xa1\xe8\xaf\xb7\xe6\xb1\x82\xe5\xa4\xb1\xe8\xb4\xa5\xe7\xbb\x88\xe6\x80\x81\xef\xbc\x9b\xe6\x9c\xac\xe8\xaf\xb7\xe6\xb1\x82\xe8\xb5\x84\xe6\xba\x90\xe5\xb7\xb2\xe5\x9b\x9e\xe6\xbb\x9a\xe3\x80\x82 */" },
		{ "Failed.Name", "EGamePlatformDataRequestState::Failed" },
		{ "Failed.ToolTip", "\xe6\x9c\xac\xe6\xac\xa1\xe8\xaf\xb7\xe6\xb1\x82\xe5\xa4\xb1\xe8\xb4\xa5\xe7\xbb\x88\xe6\x80\x81\xef\xbc\x9b\xe6\x9c\xac\xe8\xaf\xb7\xe6\xb1\x82\xe8\xb5\x84\xe6\xba\x90\xe5\xb7\xb2\xe5\x9b\x9e\xe6\xbb\x9a\xe3\x80\x82" },
		{ "Invalid.Comment", "/** \xe6\x9c\xaa\xe7\xad\xbe\xe5\x8f\x91\xe3\x80\x81\xe8\xb7\xa8\xe4\xbd\x9c\xe7\x94\xa8\xe5\x9f\x9f\xe6\x88\x96\xe8\xa2\xab\xe4\xbf\xae\xe6\x94\xb9\xe7\x9a\x84\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82 */" },
		{ "Invalid.Name", "EGamePlatformDataRequestState::Invalid" },
		{ "Invalid.ToolTip", "\xe6\x9c\xaa\xe7\xad\xbe\xe5\x8f\x91\xe3\x80\x81\xe8\xb7\xa8\xe4\xbd\x9c\xe7\x94\xa8\xe5\x9f\x9f\xe6\x88\x96\xe8\xa2\xab\xe4\xbf\xae\xe6\x94\xb9\xe7\x9a\x84\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82" },
		{ "Loading.Comment", "/** \xe5\x8f\x91\xe7\x8e\xb0\xe3\x80\x81\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x88\x96\xe7\xbb\x88\xe6\x80\x81\xe5\x8f\x91\xe5\xb8\x83\xe4\xbb\x8d\xe5\x9c\xa8\xe7\xad\x89\xe5\xbe\x85\xef\xbc\x8c\xe5\xb0\x9a\xe4\xb8\x8d\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82 */" },
		{ "Loading.Name", "EGamePlatformDataRequestState::Loading" },
		{ "Loading.ToolTip", "\xe5\x8f\x91\xe7\x8e\xb0\xe3\x80\x81\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x88\x96\xe7\xbb\x88\xe6\x80\x81\xe5\x8f\x91\xe5\xb8\x83\xe4\xbb\x8d\xe5\x9c\xa8\xe7\xad\x89\xe5\xbe\x85\xef\xbc\x8c\xe5\xb0\x9a\xe4\xb8\x8d\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
		{ "Released.Comment", "/** \xe5\xb7\xb2\xe7\xad\xbe\xe5\x8f\x91\xe7\x9a\x84\xe7\xa7\x9f\xe7\xba\xa6\xe5\xb7\xb2\xe9\x87\x8a\xe6\x94\xbe\xef\xbc\x8c\xe5\x8f\xaa\xe4\xbf\x9d\xe7\x95\x99\xe5\xb9\x82\xe7\xad\x89\xe6\xa0\xa1\xe9\xaa\x8c\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe4\xb8\x8d\xe5\x86\x8d\xe6\x8c\x81\xe6\x9c\x89\xe8\xb5\x84\xe6\xba\x90\xe3\x80\x82 */" },
		{ "Released.Name", "EGamePlatformDataRequestState::Released" },
		{ "Released.ToolTip", "\xe5\xb7\xb2\xe7\xad\xbe\xe5\x8f\x91\xe7\x9a\x84\xe7\xa7\x9f\xe7\xba\xa6\xe5\xb7\xb2\xe9\x87\x8a\xe6\x94\xbe\xef\xbc\x8c\xe5\x8f\xaa\xe4\xbf\x9d\xe7\x95\x99\xe5\xb9\x82\xe7\xad\x89\xe6\xa0\xa1\xe9\xaa\x8c\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe4\xb8\x8d\xe5\x86\x8d\xe6\x8c\x81\xe6\x9c\x89\xe8\xb5\x84\xe6\xba\x90\xe3\x80\x82" },
		{ "Succeeded.Comment", "/** \xe6\x9c\xac\xe6\xac\xa1\xe8\xaf\xb7\xe6\xb1\x82\xe6\x88\x90\xe5\x8a\x9f\xe7\xbb\x88\xe6\x80\x81\xef\xbc\x9b\xe7\xa7\x9f\xe7\xba\xa6\xe5\xb0\x9a\xe6\x9c\xaa\xe9\x87\x8a\xe6\x94\xbe\xef\xbc\x8c\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82 */" },
		{ "Succeeded.Name", "EGamePlatformDataRequestState::Succeeded" },
		{ "Succeeded.ToolTip", "\xe6\x9c\xac\xe6\xac\xa1\xe8\xaf\xb7\xe6\xb1\x82\xe6\x88\x90\xe5\x8a\x9f\xe7\xbb\x88\xe6\x80\x81\xef\xbc\x9b\xe7\xa7\x9f\xe7\xba\xa6\xe5\xb0\x9a\xe6\x9c\xaa\xe9\x87\x8a\xe6\x94\xbe\xef\xbc\x8c\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xaf\xb7\xe6\xb1\x82\xe7\x8a\xb6\xe6\x80\x81\xe4\xb8\x8e\xe8\xb5\x84\xe6\xba\x90\xe4\xbd\xbf\xe7\x94\xa8\xe6\x9c\x9f\xe9\x99\x90\xe5\x88\x86\xe5\xbc\x80\xef\xbc\x9bSucceeded\xe4\xbb\x8d\xe6\x8c\x81\xe6\x9c\x89\xe8\xb5\x84\xe6\xba\x90\xef\xbc\x8cReleased\xe4\xb8\x8d\xe5\x8f\xaf\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformDataRequestState::Invalid", (int64)EGamePlatformDataRequestState::Invalid },
		{ "EGamePlatformDataRequestState::Loading", (int64)EGamePlatformDataRequestState::Loading },
		{ "EGamePlatformDataRequestState::Succeeded", (int64)EGamePlatformDataRequestState::Succeeded },
		{ "EGamePlatformDataRequestState::Failed", (int64)EGamePlatformDataRequestState::Failed },
		{ "EGamePlatformDataRequestState::Cancelled", (int64)EGamePlatformDataRequestState::Cancelled },
		{ "EGamePlatformDataRequestState::Released", (int64)EGamePlatformDataRequestState::Released },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
	nullptr,
	"EGamePlatformDataRequestState",
	"EGamePlatformDataRequestState",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformDataRequestState;
UEnum* Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformDataRequestState.OuterSingleton)
		{
			ZRIE_EGamePlatformDataRequestState.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState, (UObject*)Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase::Outer), TEXT("EGamePlatformDataRequestState"));
		}
		return ZRIE_EGamePlatformDataRequestState.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformDataRequestState.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformDataRequestState.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformDataRequestState.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformDataRequestState ***********************************************

// ********** Begin ScriptStruct FGamePlatformDataLease ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformDataLease_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformDataLease>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformDataLease); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x8d\xe5\x8f\xaf\xe4\xbc\xaa\xe9\x80\xa0\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xe7\x9a\x84\xe5\x80\xbc\xe5\x8f\xa5\xe6\x9f\x84\xef\xbc\x9b\xe6\x89\x80\xe6\x9c\x89\xe6\x93\x8d\xe4\xbd\x9c\xe6\xa0\xb8\xe5\xaf\xb9\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x86\x85\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe5\x89\xaf\xe6\x9c\xacRequestState\xe4\xbb\x85\xe4\xb8\xba\xe5\x8f\x96\xe5\xbe\x97\xe6\x97\xb6\xe5\xbf\xab\xe7\x85\xa7\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x8d\xe5\x8f\xaf\xe4\xbc\xaa\xe9\x80\xa0\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xe7\x9a\x84\xe5\x80\xbc\xe5\x8f\xa5\xe6\x9f\x84\xef\xbc\x9b\xe6\x89\x80\xe6\x9c\x89\xe6\x93\x8d\xe4\xbd\x9c\xe6\xa0\xb8\xe5\xaf\xb9\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x86\x85\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe5\x89\xaf\xe6\x9c\xacRequestState\xe4\xbb\x85\xe4\xb8\xba\xe5\x8f\x96\xe5\xbe\x97\xe6\x97\xb6\xe5\xbf\xab\xe7\x85\xa7\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ScopeId_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\xb8\xb8\xe6\x88\x8f\xe5\xae\x9e\xe4\xbe\x8b\xe9\x97\xa8\xe9\x9d\xa2\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe8\xb7\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe9\x87\x8a\xe6\x94\xbe\xe8\xa2\xab\xe6\x8b\x92\xe7\xbb\x9d\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xb8\xb8\xe6\x88\x8f\xe5\xae\x9e\xe4\xbe\x8b\xe9\x97\xa8\xe9\x9d\xa2\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe8\xb7\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe9\x87\x8a\xe6\x94\xbe\xe8\xa2\xab\xe6\x8b\x92\xe7\xbb\x9d\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LeaseId_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xaf\xb7\xe6\xb1\x82\xe5\x94\xaf\xe4\xb8\x80\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe9\x87\x8d\xe5\xa4\x8d\xe5\x88\xa9\xe7\x94\xa8\xe6\xa7\xbd\xe4\xbd\x8d\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xaf\xb7\xe6\xb1\x82\xe5\x94\xaf\xe4\xb8\x80\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe9\x87\x8d\xe5\xa4\x8d\xe5\x88\xa9\xe7\x94\xa8\xe6\xa7\xbd\xe4\xbd\x8d\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Generation_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x9c\xac\xe6\xac\xa1\xe7\x94\xb3\xe8\xaf\xb7\xe4\xbb\xa3\xe6\xac\xa1\xef\xbc\x8c\xe5\xbc\x82\xe6\xad\xa5\xe5\xae\x8c\xe6\x88\x90\xe5\xbf\x85\xe9\xa1\xbb\xe4\xb8\x8e\xe5\xad\x98\xe6\xb4\xbb\xe8\xae\xb0\xe5\xbd\x95\xe4\xb8\x80\xe8\x87\xb4\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x9c\xac\xe6\xac\xa1\xe7\x94\xb3\xe8\xaf\xb7\xe4\xbb\xa3\xe6\xac\xa1\xef\xbc\x8c\xe5\xbc\x82\xe6\xad\xa5\xe5\xae\x8c\xe6\x88\x90\xe5\xbf\x85\xe9\xa1\xbb\xe4\xb8\x8e\xe5\xad\x98\xe6\xb4\xbb\xe8\xae\xb0\xe5\xbd\x95\xe4\xb8\x80\xe8\x87\xb4\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefinitionId_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbd\x9c\xe4\xb8\xba\xe4\xbb\xbb\xe6\x84\x8f\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbd\x9c\xe4\xb8\xba\xe4\xbb\xbb\xe6\x84\x8f\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Bundles_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x9c\xac\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe9\x9c\x80\xe8\xa6\x81\xe7\x9a\x84\xe5\x8e\xbb\xe9\x87\x8d\xe5\x88\x86\xe7\xbb\x84\xef\xbc\x8c\xe7\xa9\xba\xe9\x9b\x86\xe5\x90\x88\xe4\xbb\x8d\xe6\x8c\x81\xe6\x9c\x89\xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x9c\xac\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe9\x9c\x80\xe8\xa6\x81\xe7\x9a\x84\xe5\x8e\xbb\xe9\x87\x8d\xe5\x88\x86\xe7\xbb\x84\xef\xbc\x8c\xe7\xa9\xba\xe9\x9b\x86\xe5\x90\x88\xe4\xbb\x8d\xe6\x8c\x81\xe6\x9c\x89\xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RequestState_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\x96\xe5\xbe\x97\xe6\xad\xa4\xe5\x80\xbc\xe6\x97\xb6\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe7\x8a\xb6\xe6\x80\x81\xef\xbc\x9b\xe5\xae\x9e\xe6\x97\xb6\xe7\x8a\xb6\xe6\x80\x81\xe4\xbd\xbf\xe7\x94\xa8GetLeaseState\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataLease.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\x96\xe5\xbe\x97\xe6\xad\xa4\xe5\x80\xbc\xe6\x97\xb6\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe7\x8a\xb6\xe6\x80\x81\xef\xbc\x9b\xe5\xae\x9e\xe6\x97\xb6\xe7\x8a\xb6\xe6\x80\x81\xe4\xbd\xbf\xe7\x94\xa8GetLeaseState\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformDataLease constinit property declarations ************
	static const UECodeGen_Private::FStructPropertyParams NewProp_ScopeId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_LeaseId;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_Generation;
	static const UECodeGen_Private::FStructPropertyParams NewProp_DefinitionId;
	static const UECodeGen_Private::FNamePropertyParams NewProp_Bundles_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_Bundles;
	static const UECodeGen_Private::FBytePropertyParams NewProp_RequestState_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_RequestState;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformDataLease constinit property declarations **************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformDataLease>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformDataLease Property Definitions ***********************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ScopeId = { "ScopeId", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, ScopeId), Z_Construct_UScriptStruct_FGuid, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ScopeId_MetaData), NewProp_ScopeId_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_LeaseId = { "LeaseId", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, LeaseId), Z_Construct_UScriptStruct_FGuid, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LeaseId_MetaData), NewProp_LeaseId_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_Generation = { "Generation", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, Generation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Generation_MetaData), NewProp_Generation_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_DefinitionId = { "DefinitionId", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, DefinitionId), Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefinitionId_MetaData), NewProp_DefinitionId_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_Bundles_Inner = { "Bundles", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_Bundles = { "Bundles", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, Bundles), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Bundles_MetaData), NewProp_Bundles_MetaData) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_RequestState_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_RequestState = { "RequestState", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataLease, RequestState), Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RequestState_MetaData), NewProp_RequestState_MetaData) }; // 66006762c7588ceaa30485562669ceab19660f65
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ScopeId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LeaseId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Generation,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefinitionId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Bundles_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Bundles,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequestState_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequestState,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformDataLease Property Definitions *************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
	nullptr,
	&NewStructOps,
	"GamePlatformDataLease",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformDataLease>(),
	alignof(FGamePlatformDataLease),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformDataLease;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataLease(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformDataLease, (UObject*)Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase::Outer), TEXT("GamePlatformDataLease"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformDataLease.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformDataLease **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataLease_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_GamePlatformData_EGamePlatformDataLifetime, TEXT("EGamePlatformDataLifetime"), &ZRIE_EGamePlatformDataLifetime, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1114773124U) },
		{ Z_Construct_UEnum_GamePlatformData_EGamePlatformDataRequestState, TEXT("EGamePlatformDataRequestState"), &ZRIE_EGamePlatformDataRequestState, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1711302498U) },
	};
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformDataLease, Z_Construct_UScriptStruct_FGamePlatformDataLease_Statics::NewStructOps, TEXT("GamePlatformDataLease"),&Z_Registration_Info_UScriptStruct_FGamePlatformDataLease, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformDataLease), 1389531848U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataLease_h__Script_GamePlatformData_7d8342229fececce312aa002d2fd1a14376d10e2{
	TEXT("/Script/GamePlatformData"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
