// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXRegistrationHandle.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXRegistrationHandle() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FGuid(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXRegistrationHandle ********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXRegistrationHandle>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXRegistrationHandle); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Catalog \xe6\xb3\xa8\xe5\x86\x8c\xe5\x8f\xa5\xe6\x9f\x84\xef\xbc\x9b\xe5\x86\x85\xe5\xae\xb9\xe5\x8c\x85\xe5\xa4\xb1\xe6\xb4\xbb\xe5\x89\x8d\xe4\xbd\xbf\xe7\x94\xa8\xe8\xaf\xa5\xe5\x8f\xa5\xe6\x9f\x84\xe6\x92\xa4\xe9\x94\x80\xe6\xb3\xa8\xe5\x86\x8c\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRegistrationHandle.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Catalog \xe6\xb3\xa8\xe5\x86\x8c\xe5\x8f\xa5\xe6\x9f\x84\xef\xbc\x9b\xe5\x86\x85\xe5\xae\xb9\xe5\x8c\x85\xe5\xa4\xb1\xe6\xb4\xbb\xe5\x89\x8d\xe4\xbd\xbf\xe7\x94\xa8\xe8\xaf\xa5\xe5\x8f\xa5\xe6\x9f\x84\xe6\x92\xa4\xe9\x94\x80\xe6\xb3\xa8\xe5\x86\x8c\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RegistrationId_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRegistrationHandle.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXRegistrationHandle constinit property declarations 
	static const UECodeGen_Private::FStructPropertyParams NewProp_RegistrationId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXRegistrationHandle constinit property declarations **
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXRegistrationHandle>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXRegistrationHandle Property Definitions ***********
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_RegistrationId = { "RegistrationId", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRegistrationHandle, RegistrationId), Z_Construct_UScriptStruct_FGuid, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RegistrationId_MetaData), NewProp_RegistrationId_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RegistrationId,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXRegistrationHandle Property Definitions *************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXRegistrationHandle",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXRegistrationHandle>(),
	alignof(FGamePlatformVFXRegistrationHandle),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXRegistrationHandle"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXRegistrationHandle **********************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXRegistrationHandle_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle, Z_Construct_UScriptStruct_FGamePlatformVFXRegistrationHandle_Statics::NewStructOps, TEXT("GamePlatformVFXRegistrationHandle"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXRegistrationHandle, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXRegistrationHandle), 331636079U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXRegistrationHandle_h__Script_GamePlatformVFXClient_ea9345eadc9e04eb40adf25d4c829b57e3332778{
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
