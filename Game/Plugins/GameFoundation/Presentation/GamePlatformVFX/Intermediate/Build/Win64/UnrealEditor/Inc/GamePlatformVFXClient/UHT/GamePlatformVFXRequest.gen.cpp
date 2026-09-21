// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXRequest.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXSpawnContext.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXRequest() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTag(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTagContainer(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXParameters(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXRequest(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXRequest *******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXRequest_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXRequest>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXRequest); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * VFX \xe6\x92\xad\xe6\x94\xbe\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82\n * \xe9\xab\x98\xe5\xb1\x82\xe4\xbc\x98\xe5\x85\x88\xe6\x8f\x90\xe4\xbe\x9b SemanticTag + ContextTags\xef\xbc\x9b\xe5\x8f\xaa\xe6\x9c\x89\xe5\xb7\xb2\xe6\x98\x8e\xe7\xa1\xae\xe7\x9f\xa5\xe9\x81\x93\xe5\x86\x85\xe5\xae\xb9\xe6\x97\xb6\xe6\x89\x8d\xe4\xbd\xbf\xe7\x94\xa8 ExplicitDefinitionId\xe3\x80\x82\n */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe6\x92\xad\xe6\x94\xbe\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82\n\xe9\xab\x98\xe5\xb1\x82\xe4\xbc\x98\xe5\x85\x88\xe6\x8f\x90\xe4\xbe\x9b SemanticTag + ContextTags\xef\xbc\x9b\xe5\x8f\xaa\xe6\x9c\x89\xe5\xb7\xb2\xe6\x98\x8e\xe7\xa1\xae\xe7\x9f\xa5\xe9\x81\x93\xe5\x86\x85\xe5\xae\xb9\xe6\x97\xb6\xe6\x89\x8d\xe4\xbd\xbf\xe7\x94\xa8 ExplicitDefinitionId\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SemanticTag_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ContextTags_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExplicitDefinitionId_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SpawnContext_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Parameters_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Importance_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXRequest.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXRequest constinit property declarations ***********
	static const UECodeGen_Private::FStructPropertyParams NewProp_SemanticTag;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ContextTags;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ExplicitDefinitionId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_SpawnContext;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Parameters;
	static const UECodeGen_Private::FBytePropertyParams NewProp_Importance_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Importance;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXRequest constinit property declarations *************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXRequest>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXRequest Property Definitions **********************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_SemanticTag = { "SemanticTag", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, SemanticTag), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SemanticTag_MetaData), NewProp_SemanticTag_MetaData) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ContextTags = { "ContextTags", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, ContextTags), Z_Construct_UScriptStruct_FGameplayTagContainer, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ContextTags_MetaData), NewProp_ContextTags_MetaData) }; // 93faf2d4041600295d23f175e0992095f880d07b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ExplicitDefinitionId = { "ExplicitDefinitionId", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, ExplicitDefinitionId), Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExplicitDefinitionId_MetaData), NewProp_ExplicitDefinitionId_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_SpawnContext = { "SpawnContext", nullptr, (EPropertyFlags)0x0010008000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, SpawnContext), Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SpawnContext_MetaData), NewProp_SpawnContext_MetaData) }; // d2f879187d5df29fcd72d1397a81f971d7122d9d
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Parameters = { "Parameters", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, Parameters), Z_Construct_UScriptStruct_FGamePlatformVFXParameters, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Parameters_MetaData), NewProp_Parameters_MetaData) }; // f0817a3267f68b32f73d8fcfd4b7a8c4e3369716
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_Importance_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_Importance = { "Importance", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXRequest, Importance), Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Importance_MetaData), NewProp_Importance_MetaData) }; // 3d70a34062b22947f968a62f1473bd0862f8865b
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SemanticTag,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ContextTags,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ExplicitDefinitionId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SpawnContext,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Parameters,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Importance_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Importance,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXRequest Property Definitions ************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXRequest",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXRequest>(),
	alignof(FGamePlatformVFXRequest),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000205),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXRequest(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXRequest, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXRequest"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXRequest *********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXRequest_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXRequest, Z_Construct_UScriptStruct_FGamePlatformVFXRequest_Statics::NewStructOps, TEXT("GamePlatformVFXRequest"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXRequest, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXRequest), 1151578499U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXRequest_h__Script_GamePlatformVFXClient_2c15126720c7bc0fbe5c5e0e9185f99c538b6d1e{
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
