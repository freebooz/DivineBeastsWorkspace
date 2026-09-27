// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformDataVersion.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformDataVersion() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataVersion(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformDataVersion ******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformDataVersion_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformDataVersion>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformDataVersion); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x95\xb0\xe6\x8d\xae\xe6\xa0\xbc\xe5\xbc\x8f\xe4\xb8\x8e\xe5\x86\x85\xe5\xae\xb9\xe4\xbf\xae\xe8\xae\xa2\xef\xbc\x9b\xe5\x85\xbc\xe5\xae\xb9\xe8\x8c\x83\xe5\x9b\xb4\xe7\x94\xb1\xe5\x85\xb7\xe4\xbd\x93\xe5\xae\x9a\xe4\xb9\x89\xe7\xb1\xbb\xe7\x9a\x84""CDO\xe4\xbb\xa3\xe7\xa0\x81\xe5\xa3\xb0\xe6\x98\x8e\xef\xbc\x8c\xe8\xb5\x84\xe4\xba\xa7\xe4\xb8\x8d\xe8\x83\xbd\xe6\x89\xa9\xe5\xa4\xa7\xe8\x8c\x83\xe5\x9b\xb4\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x95\xb0\xe6\x8d\xae\xe6\xa0\xbc\xe5\xbc\x8f\xe4\xb8\x8e\xe5\x86\x85\xe5\xae\xb9\xe4\xbf\xae\xe8\xae\xa2\xef\xbc\x9b\xe5\x85\xbc\xe5\xae\xb9\xe8\x8c\x83\xe5\x9b\xb4\xe7\x94\xb1\xe5\x85\xb7\xe4\xbd\x93\xe5\xae\x9a\xe4\xb9\x89\xe7\xb1\xbb\xe7\x9a\x84""CDO\xe4\xbb\xa3\xe7\xa0\x81\xe5\xa3\xb0\xe6\x98\x8e\xef\xbc\x8c\xe8\xb5\x84\xe4\xba\xa7\xe4\xb8\x8d\xe8\x83\xbd\xe6\x89\xa9\xe5\xa4\xa7\xe8\x8c\x83\xe5\x9b\xb4\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SchemaVersion_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
		{ "ClampMin", "1" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xba\x8f\xe5\x88\x97\xe5\x8c\x96\xe7\xbb\x93\xe6\x9e\x84\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x8c\xe4\xbb\x8e""1\xe5\xbc\x80\xe5\xa7\x8b\xef\xbc\x9b\xe4\xb8\x8d\xe7\xad\x89\xe4\xba\x8e\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe7\x89\x88\xe6\x9c\xac\xe6\x88\x96\xe5\xbc\x95\xe6\x93\x8e\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xba\x8f\xe5\x88\x97\xe5\x8c\x96\xe7\xbb\x93\xe6\x9e\x84\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x8c\xe4\xbb\x8e""1\xe5\xbc\x80\xe5\xa7\x8b\xef\xbc\x9b\xe4\xb8\x8d\xe7\xad\x89\xe4\xba\x8e\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xe7\x89\x88\xe6\x9c\xac\xe6\x88\x96\xe5\xbc\x95\xe6\x93\x8e\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ContentRevision_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
		{ "ClampMin", "1" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x90\x8c\xe7\xbb\x93\xe6\x9e\x84\xe7\x9a\x84\xe5\x86\x85\xe5\xae\xb9\xe4\xbf\xae\xe8\xae\xa2\xef\xbc\x8c\xe4\xbb\x8e""1\xe5\xbc\x80\xe5\xa7\x8b\xef\xbc\x9b\xe4\xb8\x8d\xe9\x9a\x90\xe5\x90\xab\xe8\xb7\xa8\xe7\x89\x88\xe6\x9c\xac\xe8\xbf\x81\xe7\xa7\xbb\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformDataVersion.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x90\x8c\xe7\xbb\x93\xe6\x9e\x84\xe7\x9a\x84\xe5\x86\x85\xe5\xae\xb9\xe4\xbf\xae\xe8\xae\xa2\xef\xbc\x8c\xe4\xbb\x8e""1\xe5\xbc\x80\xe5\xa7\x8b\xef\xbc\x9b\xe4\xb8\x8d\xe9\x9a\x90\xe5\x90\xab\xe8\xb7\xa8\xe7\x89\x88\xe6\x9c\xac\xe8\xbf\x81\xe7\xa7\xbb\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformDataVersion constinit property declarations **********
	static const UECodeGen_Private::FIntPropertyParams NewProp_SchemaVersion;
	static const UECodeGen_Private::FIntPropertyParams NewProp_ContentRevision;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformDataVersion constinit property declarations ************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformDataVersion>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformDataVersion Property Definitions *********************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_SchemaVersion = { "SchemaVersion", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataVersion, SchemaVersion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SchemaVersion_MetaData), NewProp_SchemaVersion_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_ContentRevision = { "ContentRevision", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformDataVersion, ContentRevision), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ContentRevision_MetaData), NewProp_ContentRevision_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SchemaVersion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ContentRevision,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformDataVersion Property Definitions ***********************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
	nullptr,
	&NewStructOps,
	"GamePlatformDataVersion",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformDataVersion>(),
	alignof(FGamePlatformDataVersion),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataVersion(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformDataVersion, (UObject*)Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase::Outer), TEXT("GamePlatformDataVersion"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformDataVersion ********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataVersion_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformDataVersion, Z_Construct_UScriptStruct_FGamePlatformDataVersion_Statics::NewStructOps, TEXT("GamePlatformDataVersion"),&Z_Registration_Info_UScriptStruct_FGamePlatformDataVersion, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformDataVersion), 3388812563U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataVersion_h__Script_GamePlatformData_03ee60f2c34a485c91232325b20d7b4b790d41dd{
	TEXT("/Script/GamePlatformData"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
