// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVersion.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVersion() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase);
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVersion(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVersion **********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVersion_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVersion>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVersion); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe4\xb8\x89\xe6\xae\xb5\xe9\x9d\x9e\xe8\xb4\x9f\xe6\x95\xb0\xe5\x80\xbc\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4""0.0.0\xe6\x9c\x89\xe6\x95\x88\xef\xbc\x8c\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe9\xa2\x84\xe5\x8f\x91\xe5\xb8\x83\xe3\x80\x81\xe6\x9e\x84\xe5\xbb\xba\xe5\x85\x83\xe6\x95\xb0\xe6\x8d\xae\xe6\x88\x96\xe8\x87\xaa\xe5\x8a\xa8\xe5\x85\xbc\xe5\xae\xb9\xe7\xad\x96\xe7\x95\xa5\xe3\x80\x82\n * \xe5\x90\x84\xe5\x88\x86\xe9\x87\x8f\xe8\x8c\x83\xe5\x9b\xb4""0..INT32_MAX\xe3\x80\x82\xe6\xad\xa4\xe5\x80\xbc\xe4\xb8\x8e\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x81UE\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x81\xe5\x8d\x8f\xe8\xae\xae\xe7\x89\x88\xe6\x9c\xac\xe5\x88\x86\xe5\x88\xab\xe7\x94\xb1\xe6\x89\x80\xe5\xb1\x9e\xe9\xa2\x86\xe5\x9f\x9f\xe8\xa7\xa3\xe9\x87\x8a\xe3\x80\x82\n * \xe6\x97\xa0\xe8\xb5\x84\xe6\xba\x90\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xef\xbc\x9b\xe7\x8b\xac\xe7\xab\x8b\xe5\x80\xbc\xe5\x8f\xaf\xe5\x9c\xa8\xe4\xbb\xbb\xe6\x84\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe6\x93\x8d\xe4\xbd\x9c\xef\xbc\x8c\xe5\x85\xb1\xe4\xba\xab\xe5\x8f\xaf\xe5\x8f\x98\xe5\xae\x9e\xe4\xbe\x8b\xe9\x9c\x80\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe5\x90\x8c\xe6\xad\xa5\xe3\x80\x82\n */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x89\xe6\xae\xb5\xe9\x9d\x9e\xe8\xb4\x9f\xe6\x95\xb0\xe5\x80\xbc\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4""0.0.0\xe6\x9c\x89\xe6\x95\x88\xef\xbc\x8c\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe9\xa2\x84\xe5\x8f\x91\xe5\xb8\x83\xe3\x80\x81\xe6\x9e\x84\xe5\xbb\xba\xe5\x85\x83\xe6\x95\xb0\xe6\x8d\xae\xe6\x88\x96\xe8\x87\xaa\xe5\x8a\xa8\xe5\x85\xbc\xe5\xae\xb9\xe7\xad\x96\xe7\x95\xa5\xe3\x80\x82\n\xe5\x90\x84\xe5\x88\x86\xe9\x87\x8f\xe8\x8c\x83\xe5\x9b\xb4""0..INT32_MAX\xe3\x80\x82\xe6\xad\xa4\xe5\x80\xbc\xe4\xb8\x8e\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x81UE\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x81\xe5\x8d\x8f\xe8\xae\xae\xe7\x89\x88\xe6\x9c\xac\xe5\x88\x86\xe5\x88\xab\xe7\x94\xb1\xe6\x89\x80\xe5\xb1\x9e\xe9\xa2\x86\xe5\x9f\x9f\xe8\xa7\xa3\xe9\x87\x8a\xe3\x80\x82\n\xe6\x97\xa0\xe8\xb5\x84\xe6\xba\x90\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\x83\xef\xbc\x9b\xe7\x8b\xac\xe7\xab\x8b\xe5\x80\xbc\xe5\x8f\xaf\xe5\x9c\xa8\xe4\xbb\xbb\xe6\x84\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe6\x93\x8d\xe4\xbd\x9c\xef\xbc\x8c\xe5\x85\xb1\xe4\xba\xab\xe5\x8f\xaf\xe5\x8f\x98\xe5\xae\x9e\xe4\xbe\x8b\xe9\x9c\x80\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xe5\x90\x8c\xe6\xad\xa5\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Major_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
		{ "ClampMin", "0" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\xbb\xe7\x89\x88\xe6\x9c\xac\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x9b\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\x8d\xe6\xa0\xb9\xe6\x8d\xae\xe5\xae\x83\xe8\x87\xaa\xe5\x8a\xa8\xe6\x8e\xa5\xe5\x8f\x97\xe6\x88\x96\xe6\x8b\x92\xe7\xbb\x9d\xe5\x85\xbc\xe5\xae\xb9\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\xbb\xe7\x89\x88\xe6\x9c\xac\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x9b\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\x8d\xe6\xa0\xb9\xe6\x8d\xae\xe5\xae\x83\xe8\x87\xaa\xe5\x8a\xa8\xe6\x8e\xa5\xe5\x8f\x97\xe6\x88\x96\xe6\x8b\x92\xe7\xbb\x9d\xe5\x85\xbc\xe5\xae\xb9\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Minor_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
		{ "ClampMin", "0" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\xac\xa1\xe7\x89\x88\xe6\x9c\xac\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x8c\xe6\xaf\x94\xe8\xbe\x83\xe4\xbc\x98\xe5\x85\x88\xe7\xba\xa7\xe4\xbd\x8e\xe4\xba\x8e\xe4\xb8\xbb\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xac\xa1\xe7\x89\x88\xe6\x9c\xac\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x8c\xe6\xaf\x94\xe8\xbe\x83\xe4\xbc\x98\xe5\x85\x88\xe7\xba\xa7\xe4\xbd\x8e\xe4\xba\x8e\xe4\xb8\xbb\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Patch_MetaData[] = {
		{ "Category", "GamePlatform|Core" },
		{ "ClampMin", "0" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xa1\xa5\xe4\xb8\x81\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x8c\xe6\xaf\x94\xe8\xbe\x83\xe4\xbc\x98\xe5\x85\x88\xe7\xba\xa7\xe6\x9c\x80\xe4\xbd\x8e\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xa1\xa5\xe4\xb8\x81\xe6\x95\xb0\xe5\xad\x97\xef\xbc\x8c\xe6\xaf\x94\xe8\xbe\x83\xe4\xbc\x98\xe5\x85\x88\xe7\xba\xa7\xe6\x9c\x80\xe4\xbd\x8e\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVersion constinit property declarations **************
	static const UECodeGen_Private::FIntPropertyParams NewProp_Major;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Minor;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Patch;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVersion constinit property declarations ****************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVersion>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVersion Property Definitions *************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Major = { "Major", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVersion, Major), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Major_MetaData), NewProp_Major_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Minor = { "Minor", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVersion, Minor), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Minor_MetaData), NewProp_Minor_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Patch = { "Patch", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVersion, Patch), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Patch_MetaData), NewProp_Patch_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Major,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Minor,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Patch,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVersion Property Definitions ***************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformCore,
	nullptr,
	&NewStructOps,
	"GamePlatformVersion",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVersion>(),
	alignof(FGamePlatformVersion),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVersion;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVersion(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVersion.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVersion.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVersion, (UObject*)Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase::Outer), TEXT("GamePlatformVersion"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVersion.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVersion.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVersion.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVersion.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVersion ************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformVersion_h__Script_GamePlatformCore_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVersion, Z_Construct_UScriptStruct_FGamePlatformVersion_Statics::NewStructOps, TEXT("GamePlatformVersion"),&Z_Registration_Info_UScriptStruct_FGamePlatformVersion, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVersion), 1219515443U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformVersion_h__Script_GamePlatformCore_16970b4b70183beba5234d90c211e09e6332628e{
	TEXT("/Script/GamePlatformCore"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
