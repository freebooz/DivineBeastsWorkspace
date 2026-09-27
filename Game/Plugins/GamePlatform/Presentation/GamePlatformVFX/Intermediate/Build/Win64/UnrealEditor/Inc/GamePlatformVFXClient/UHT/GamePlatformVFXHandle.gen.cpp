// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXHandle.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXHandle() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FGuid(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXHandle(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXHandle ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXHandle_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXHandle>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXHandle); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x96\xe7\x95\x8c\xe5\x86\x85\xe4\xb8\x80\xe6\xac\xa1 VFX \xe5\xae\x9e\xe4\xbe\x8b\xe7\x9a\x84\xe5\xae\x89\xe5\x85\xa8\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82Generation \xe9\x98\xb2\xe6\xad\xa2\xe6\x97\xa7\xe4\xb8\x96\xe7\x95\x8c\xe5\x8f\xa5\xe6\x9f\x84\xe8\xaf\xaf\xe6\x93\x8d\xe4\xbd\x9c\xe6\x96\xb0\xe4\xb8\x96\xe7\x95\x8c\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXHandle.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x96\xe7\x95\x8c\xe5\x86\x85\xe4\xb8\x80\xe6\xac\xa1 VFX \xe5\xae\x9e\xe4\xbe\x8b\xe7\x9a\x84\xe5\xae\x89\xe5\x85\xa8\xe5\x8f\xa5\xe6\x9f\x84\xe3\x80\x82Generation \xe9\x98\xb2\xe6\xad\xa2\xe6\x97\xa7\xe4\xb8\x96\xe7\x95\x8c\xe5\x8f\xa5\xe6\x9f\x84\xe8\xaf\xaf\xe6\x93\x8d\xe4\xbd\x9c\xe6\x96\xb0\xe4\xb8\x96\xe7\x95\x8c\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InstanceId_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXHandle.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Generation_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXHandle.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXHandle constinit property declarations ************
	static const UECodeGen_Private::FStructPropertyParams NewProp_InstanceId;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Generation;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXHandle constinit property declarations **************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXHandle>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXHandle Property Definitions ***********************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_InstanceId = { "InstanceId", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXHandle, InstanceId), Z_Construct_UScriptStruct_FGuid, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InstanceId_MetaData), NewProp_InstanceId_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Generation = { "Generation", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXHandle, Generation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Generation_MetaData), NewProp_Generation_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_InstanceId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Generation,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXHandle Property Definitions *************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXHandle",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXHandle>(),
	alignof(FGamePlatformVFXHandle),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXHandle(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXHandle, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXHandle"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXHandle **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXHandle_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXHandle, Z_Construct_UScriptStruct_FGamePlatformVFXHandle_Statics::NewStructOps, TEXT("GamePlatformVFXHandle"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXHandle, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXHandle), 3748329066U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXHandle_h__Script_GamePlatformVFXClient_119acf31cb9078f6d6b2ea0237e669b0b33584a3{
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
