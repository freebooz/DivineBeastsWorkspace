// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformResult.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformResult() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase);
GAMEPLATFORMCORE_API UEnum* Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus(ETypeConstructPhase);
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformResult(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EGamePlatformResultStatus *************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus_Statics
template<> GAMEPLATFORMCORE_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformResultStatus>()
{
	return Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "Cancelled.Comment", "/** \xe6\x93\x8d\xe4\xbd\x9c\xe8\xa2\xab\xe5\x8f\x96\xe6\xb6\x88\xef\xbc\x9b\xe4\xb8\x8d\xe8\xa1\xa8\xe7\xa4\xba\xe4\xb8\x9a\xe5\x8a\xa1\xe8\xa1\xa5\xe5\x81\xbf\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90\xe3\x80\x82 */" },
		{ "Cancelled.Name", "EGamePlatformResultStatus::Cancelled" },
		{ "Cancelled.ToolTip", "\xe6\x93\x8d\xe4\xbd\x9c\xe8\xa2\xab\xe5\x8f\x96\xe6\xb6\x88\xef\xbc\x9b\xe4\xb8\x8d\xe8\xa1\xa8\xe7\xa4\xba\xe4\xb8\x9a\xe5\x8a\xa1\xe8\xa1\xa5\xe5\x81\xbf\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90\xe3\x80\x82" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x90\x8c\xe6\xad\xa5\xe6\x88\x96\xe5\xbc\x82\xe6\xad\xa5\xe8\xb0\x83\xe7\x94\xa8\xe7\x9a\x84\xe4\xb8\xad\xe7\xab\x8b\xe7\xbb\x93\xe6\x9e\x9c\xe5\x88\x86\xe7\xb1\xbb\xef\xbc\x9b\xe6\x8f\x8f\xe8\xbf\xb0\xe7\xbb\x93\xe6\x9e\x9c\xef\xbc\x8c\xe4\xb8\x8d\xe8\xa7\xa6\xe5\x8f\x91\xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x81\xe5\x9b\x9e\xe6\xbb\x9a\xe3\x80\x81\xe9\x87\x8d\xe8\xaf\x95\xe6\x88\x96\xe8\xb5\x84\xe6\xba\x90\xe9\x87\x8a\xe6\x94\xbe\xe3\x80\x82 */" },
#endif
		{ "Failed.Comment", "/** \xe6\x93\x8d\xe4\xbd\x9c\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8c""Code\xe7\xbb\x99\xe5\x87\xba\xe5\x8f\xaf\xe6\xa3\x80\xe7\xb4\xa2\xe5\x8e\x9f\xe5\x9b\xa0\xe3\x80\x82 */" },
		{ "Failed.Name", "EGamePlatformResultStatus::Failed" },
		{ "Failed.ToolTip", "\xe6\x93\x8d\xe4\xbd\x9c\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8c""Code\xe7\xbb\x99\xe5\x87\xba\xe5\x8f\xaf\xe6\xa3\x80\xe7\xb4\xa2\xe5\x8e\x9f\xe5\x9b\xa0\xe3\x80\x82" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformResult.h" },
		{ "NotExecuted.Comment", "/** \xe5\xb0\x9a\xe6\x9c\xaa\xe6\x89\xa7\xe8\xa1\x8c\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe5\x80\xbc\xe5\xbf\x85\xe9\xa1\xbb\xe4\xb8\x8d\xe8\x83\xbd\xe8\xa2\xab\xe8\xaf\xaf\xe8\xae\xa4\xe4\xb8\xba\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82 */" },
		{ "NotExecuted.Name", "EGamePlatformResultStatus::NotExecuted" },
		{ "NotExecuted.ToolTip", "\xe5\xb0\x9a\xe6\x9c\xaa\xe6\x89\xa7\xe8\xa1\x8c\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe5\x80\xbc\xe5\xbf\x85\xe9\xa1\xbb\xe4\xb8\x8d\xe8\x83\xbd\xe8\xa2\xab\xe8\xaf\xaf\xe8\xae\xa4\xe4\xb8\xba\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82" },
		{ "Succeeded.Comment", "/** \xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe7\xa1\xae\xe8\xae\xa4\xe6\x93\x8d\xe4\xbd\x9c\xe6\x88\x90\xe5\x8a\x9f\xe5\xae\x8c\xe6\x88\x90\xe3\x80\x82 */" },
		{ "Succeeded.Name", "EGamePlatformResultStatus::Succeeded" },
		{ "Succeeded.ToolTip", "\xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe7\xa1\xae\xe8\xae\xa4\xe6\x93\x8d\xe4\xbd\x9c\xe6\x88\x90\xe5\x8a\x9f\xe5\xae\x8c\xe6\x88\x90\xe3\x80\x82" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x90\x8c\xe6\xad\xa5\xe6\x88\x96\xe5\xbc\x82\xe6\xad\xa5\xe8\xb0\x83\xe7\x94\xa8\xe7\x9a\x84\xe4\xb8\xad\xe7\xab\x8b\xe7\xbb\x93\xe6\x9e\x9c\xe5\x88\x86\xe7\xb1\xbb\xef\xbc\x9b\xe6\x8f\x8f\xe8\xbf\xb0\xe7\xbb\x93\xe6\x9e\x9c\xef\xbc\x8c\xe4\xb8\x8d\xe8\xa7\xa6\xe5\x8f\x91\xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x81\xe5\x9b\x9e\xe6\xbb\x9a\xe3\x80\x81\xe9\x87\x8d\xe8\xaf\x95\xe6\x88\x96\xe8\xb5\x84\xe6\xba\x90\xe9\x87\x8a\xe6\x94\xbe\xe3\x80\x82" },
#endif
		{ "Unsupported.Comment", "/** \xe5\xbd\x93\xe5\x89\x8d\xe5\xae\x9e\xe7\x8e\xb0\xe6\x88\x96\xe7\x8e\xaf\xe5\xa2\x83\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe8\x83\xbd\xe5\x8a\x9b\xe3\x80\x82 */" },
		{ "Unsupported.Name", "EGamePlatformResultStatus::Unsupported" },
		{ "Unsupported.ToolTip", "\xe5\xbd\x93\xe5\x89\x8d\xe5\xae\x9e\xe7\x8e\xb0\xe6\x88\x96\xe7\x8e\xaf\xe5\xa2\x83\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe8\x83\xbd\xe5\x8a\x9b\xe3\x80\x82" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformResultStatus::NotExecuted", (int64)EGamePlatformResultStatus::NotExecuted },
		{ "EGamePlatformResultStatus::Succeeded", (int64)EGamePlatformResultStatus::Succeeded },
		{ "EGamePlatformResultStatus::Failed", (int64)EGamePlatformResultStatus::Failed },
		{ "EGamePlatformResultStatus::Cancelled", (int64)EGamePlatformResultStatus::Cancelled },
		{ "EGamePlatformResultStatus::Unsupported", (int64)EGamePlatformResultStatus::Unsupported },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformCore,
	nullptr,
	"EGamePlatformResultStatus",
	"EGamePlatformResultStatus",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformResultStatus;
UEnum* Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformResultStatus.OuterSingleton)
		{
			ZRIE_EGamePlatformResultStatus.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus, (UObject*)Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase::Outer), TEXT("EGamePlatformResultStatus"));
		}
		return ZRIE_EGamePlatformResultStatus.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformResultStatus.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformResultStatus.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformResultStatus.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformResultStatus ***************************************************

// ********** Begin ScriptStruct FGamePlatformResult ***********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformResult_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformResult>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformResult); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe8\x87\xaa\xe5\x8c\x85\xe5\x90\xab\xe7\x9a\x84\xe7\xbb\x93\xe6\x9e\x9c\xe5\x80\xbc\xef\xbc\x9b""Code\xe7\x94\xa8\xe4\xba\x8e\xe7\xa8\x8b\xe5\xba\x8f\xe5\x88\x86\xe6\x94\xaf\xef\xbc\x8cMessage\xe4\xbe\x9b\xe8\xaf\x8a\xe6\x96\xad\xe8\x80\x8c\xe9\x9d\x9e\xe6\x9c\xac\xe5\x9c\xb0\xe5\x8c\x96UI\xe6\x88\x96\xe6\x95\x8f\xe6\x84\x9f\xe6\x95\xb0\xe6\x8d\xae\xe4\xbc\xa0\xe8\xbe\x93\xe3\x80\x82\n * \xe5\xb7\xa5\xe5\x8e\x82\xe4\xbf\x9d\xe8\xaf\x81\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8f\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe8\xaf\x8a\xe6\x96\xad\xe5\xae\x8c\xe6\x95\xb4\xef\xbc\x9b\xe5\x85\xac\xe5\xbc\x80""C++\xe5\xad\x97\xe6\xae\xb5\xe4\xb8\xba\xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe5\xa5\x91\xe7\xba\xa6\xef\xbc\x8c\xe7\x9b\xb4\xe6\x8e\xa5\xe8\xb5\x8b\xe5\x80\xbc\xe9\x9c\x80\xe8\x87\xaa\xe8\xa1\x8c\xe4\xbf\x9d\xe6\x8c\x81\xe4\xb8\x80\xe8\x87\xb4\xe3\x80\x82\n * \xe6\x97\xa0UObject\xe6\x88\x96\xe5\x9b\x9e\xe8\xb0\x83\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xef\xbc\x9b\xe5\x8f\xaf\xe5\x9c\xa8\xe7\xba\xbf\xe7\xa8\x8b\xe9\x97\xb4\xe5\xa4\x8d\xe5\x88\xb6\xef\xbc\x8c\xe7\xa6\x81\xe6\xad\xa2\xe6\x97\xa0\xe5\x90\x8c\xe6\xad\xa5\xe5\xb9\xb6\xe5\x8f\x91\xe4\xbf\xae\xe6\x94\xb9\xe5\x90\x8c\xe4\xb8\x80\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82\n */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformResult.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\x87\xaa\xe5\x8c\x85\xe5\x90\xab\xe7\x9a\x84\xe7\xbb\x93\xe6\x9e\x9c\xe5\x80\xbc\xef\xbc\x9b""Code\xe7\x94\xa8\xe4\xba\x8e\xe7\xa8\x8b\xe5\xba\x8f\xe5\x88\x86\xe6\x94\xaf\xef\xbc\x8cMessage\xe4\xbe\x9b\xe8\xaf\x8a\xe6\x96\xad\xe8\x80\x8c\xe9\x9d\x9e\xe6\x9c\xac\xe5\x9c\xb0\xe5\x8c\x96UI\xe6\x88\x96\xe6\x95\x8f\xe6\x84\x9f\xe6\x95\xb0\xe6\x8d\xae\xe4\xbc\xa0\xe8\xbe\x93\xe3\x80\x82\n\xe5\xb7\xa5\xe5\x8e\x82\xe4\xbf\x9d\xe8\xaf\x81\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8f\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe8\xaf\x8a\xe6\x96\xad\xe5\xae\x8c\xe6\x95\xb4\xef\xbc\x9b\xe5\x85\xac\xe5\xbc\x80""C++\xe5\xad\x97\xe6\xae\xb5\xe4\xb8\xba\xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe5\xa5\x91\xe7\xba\xa6\xef\xbc\x8c\xe7\x9b\xb4\xe6\x8e\xa5\xe8\xb5\x8b\xe5\x80\xbc\xe9\x9c\x80\xe8\x87\xaa\xe8\xa1\x8c\xe4\xbf\x9d\xe6\x8c\x81\xe4\xb8\x80\xe8\x87\xb4\xe3\x80\x82\n\xe6\x97\xa0UObject\xe6\x88\x96\xe5\x9b\x9e\xe8\xb0\x83\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xef\xbc\x9b\xe5\x8f\xaf\xe5\x9c\xa8\xe7\xba\xbf\xe7\xa8\x8b\xe9\x97\xb4\xe5\xa4\x8d\xe5\x88\xb6\xef\xbc\x8c\xe7\xa6\x81\xe6\xad\xa2\xe6\x97\xa0\xe5\x90\x8c\xe6\xad\xa5\xe5\xb9\xb6\xe5\x8f\x91\xe4\xbf\xae\xe6\x94\xb9\xe5\x90\x8c\xe4\xb8\x80\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Status_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe9\xbb\x98\xe8\xae\xa4\xe6\x9c\xaa\xe6\x89\xa7\xe8\xa1\x8c\xef\xbc\x9b\xe4\xbb\x85\xe6\x98\xbe\xe5\xbc\x8fSucceeded\xe5\x8f\xaf\xe8\x83\xbd\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82\xe8\x93\x9d\xe5\x9b\xbe\xe5\x8f\xaa\xe8\x83\xbd\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformResult.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe9\xbb\x98\xe8\xae\xa4\xe6\x9c\xaa\xe6\x89\xa7\xe8\xa1\x8c\xef\xbc\x9b\xe4\xbb\x85\xe6\x98\xbe\xe5\xbc\x8fSucceeded\xe5\x8f\xaf\xe8\x83\xbd\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82\xe8\x93\x9d\xe5\x9b\xbe\xe5\x8f\xaa\xe8\x83\xbd\xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Code_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x9c\xba\xe5\x99\xa8\xe5\x8f\xaf\xe8\xaf\xbb\xe8\xaf\x8a\xe6\x96\xad\xef\xbc\x9b\xe6\x88\x90\xe5\x8a\x9f\xe6\x97\xb6\xe4\xb8\xbaNAME_None\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8f\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe7\xbc\xba\xe7\xa0\x81\xe7\x94\xb1\xe5\xb7\xa5\xe5\x8e\x82\xe8\xa1\xa5\xe5\x85\x85\xe6\x98\x8e\xe7\xa1\xae\xe9\x94\x99\xe8\xaf\xaf\xe7\xa0\x81\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformResult.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x9c\xba\xe5\x99\xa8\xe5\x8f\xaf\xe8\xaf\xbb\xe8\xaf\x8a\xe6\x96\xad\xef\xbc\x9b\xe6\x88\x90\xe5\x8a\x9f\xe6\x97\xb6\xe4\xb8\xbaNAME_None\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8f\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe7\xbc\xba\xe7\xa0\x81\xe7\x94\xb1\xe5\xb7\xa5\xe5\x8e\x82\xe8\xa1\xa5\xe5\x85\x85\xe6\x98\x8e\xe7\xa1\xae\xe9\x94\x99\xe8\xaf\xaf\xe7\xa0\x81\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Message_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xba\xba\xe7\xb1\xbb\xe5\x8f\xaf\xe8\xaf\xbb\xe4\xb8\xad\xe6\x96\x87\xe8\xaf\x8a\xe6\x96\xad\xe6\x88\x96\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe6\x8f\x90\xe4\xbe\x9b\xe7\x9a\x84\xe8\xaf\xb4\xe6\x98\x8e\xef\xbc\x9b\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe8\xb4\x9f\xe8\xb4\xa3\xe8\x84\xb1\xe6\x95\x8f\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformResult.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xba\xba\xe7\xb1\xbb\xe5\x8f\xaf\xe8\xaf\xbb\xe4\xb8\xad\xe6\x96\x87\xe8\xaf\x8a\xe6\x96\xad\xe6\x88\x96\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe6\x8f\x90\xe4\xbe\x9b\xe7\x9a\x84\xe8\xaf\xb4\xe6\x98\x8e\xef\xbc\x9b\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe8\xb4\x9f\xe8\xb4\xa3\xe8\x84\xb1\xe6\x95\x8f\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformResult constinit property declarations ***************
	static const UECodeGen_Private::FBytePropertyParams NewProp_Status_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Status;
	static const UECodeGen_Private::FNamePropertyParams NewProp_Code;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Message;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformResult constinit property declarations *****************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformResult>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformResult Property Definitions **************************
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_Status_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_Status = { "Status", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformResult, Status), Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Status_MetaData), NewProp_Status_MetaData) }; // 06d71c98f8782a2093db59850c8d9198b011ce90
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_Code = { "Code", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformResult, Code), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Code_MetaData), NewProp_Code_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Message = { "Message", nullptr, (EPropertyFlags)0x0010000000000014, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformResult, Message), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Message_MetaData), NewProp_Message_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Status_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Status,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Code,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Message,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformResult Property Definitions ****************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformCore,
	nullptr,
	&NewStructOps,
	"GamePlatformResult",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformResult>(),
	alignof(FGamePlatformResult),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformResult;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformResult(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformResult.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformResult.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformResult, (UObject*)Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase::Outer), TEXT("GamePlatformResult"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformResult.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformResult.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformResult.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformResult.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformResult *************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformResult_h__Script_GamePlatformCore_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_GamePlatformCore_EGamePlatformResultStatus, TEXT("EGamePlatformResultStatus"), &ZRIE_EGamePlatformResultStatus, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 114760856U) },
	};
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformResult, Z_Construct_UScriptStruct_FGamePlatformResult_Statics::NewStructOps, TEXT("GamePlatformResult"),&Z_Registration_Info_UScriptStruct_FGamePlatformResult, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformResult), 1883980131U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformResult_h__Script_GamePlatformCore_05d524ee5a2c32acaf198249b74e6883756ba48b{
	TEXT("/Script/GamePlatformCore"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
