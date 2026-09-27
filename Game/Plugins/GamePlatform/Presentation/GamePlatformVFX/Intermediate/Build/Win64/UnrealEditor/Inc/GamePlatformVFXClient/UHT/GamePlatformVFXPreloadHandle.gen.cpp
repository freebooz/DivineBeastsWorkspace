// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXPreloadHandle.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXPreloadHandle() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FGuid(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXPreloadHandle *************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXPreloadHandle>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXPreloadHandle); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** VFX \xe9\xa2\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe7\xa7\x9f\xe7\xba\xa6\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXPreloadHandle.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe9\xa2\x84\xe5\x8a\xa0\xe8\xbd\xbd\xe7\xa7\x9f\xe7\xba\xa6\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RequestId_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXPreloadHandle.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXPreloadHandle constinit property declarations *****
	static const UECodeGen_Private::FStructPropertyParams NewProp_RequestId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXPreloadHandle constinit property declarations *******
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXPreloadHandle>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXPreloadHandle Property Definitions ****************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_RequestId = { "RequestId", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXPreloadHandle, RequestId), Z_Construct_UScriptStruct_FGuid, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RequestId_MetaData), NewProp_RequestId_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequestId,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXPreloadHandle Property Definitions ******************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXPreloadHandle",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXPreloadHandle>(),
	alignof(FGamePlatformVFXPreloadHandle),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXPreloadHandle"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXPreloadHandle ***************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXPreloadHandle_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle, Z_Construct_UScriptStruct_FGamePlatformVFXPreloadHandle_Statics::NewStructOps, TEXT("GamePlatformVFXPreloadHandle"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXPreloadHandle, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXPreloadHandle), 1186480263U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXPreloadHandle_h__Script_GamePlatformVFXClient_751c4e42a8e2dcb89745c6f446b61215fc476cd6{
	TEXT("/Script/GamePlatformVFXClient"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
