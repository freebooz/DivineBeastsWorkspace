// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Catalogs/GamePlatformVFXCatalogTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXCatalogTypes() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTag(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTagContainer(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXCatalogEntry **************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXCatalogEntry>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXCatalogEntry); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x80\xe6\x9d\xa1\xe7\xa1\xae\xe5\xae\x9a\xe6\x80\xa7\xe7\x9a\x84\xe8\xaf\xad\xe4\xb9\x89\xe5\x88\xb0 VFX Definition \xe6\x98\xa0\xe5\xb0\x84\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x80\xe6\x9d\xa1\xe7\xa1\xae\xe5\xae\x9a\xe6\x80\xa7\xe7\x9a\x84\xe8\xaf\xad\xe4\xb9\x89\xe5\x88\xb0 VFX Definition \xe6\x98\xa0\xe5\xb0\x84\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SemanticTag_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RequiredContextTags_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_BlockedContextTags_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefinitionId_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Scope_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Priority_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAllowParentSemanticFallback_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Catalogs/GamePlatformVFXCatalogTypes.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXCatalogEntry constinit property declarations ******
	static const UECodeGen_Private::FStructPropertyParams NewProp_SemanticTag;
	static const UECodeGen_Private::FStructPropertyParams NewProp_RequiredContextTags;
	static const UECodeGen_Private::FStructPropertyParams NewProp_BlockedContextTags;
	static const UECodeGen_Private::FStructPropertyParams NewProp_DefinitionId;
	static const UECodeGen_Private::FBytePropertyParams NewProp_Scope_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Scope;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Priority;
	static void NewProp_bAllowParentSemanticFallback_SetBit(void* Obj)
	{
		((FGamePlatformVFXCatalogEntry*)Obj)->bAllowParentSemanticFallback = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAllowParentSemanticFallback;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXCatalogEntry constinit property declarations ********
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXCatalogEntry>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXCatalogEntry Property Definitions *****************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_SemanticTag = { "SemanticTag", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, SemanticTag), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SemanticTag_MetaData), NewProp_SemanticTag_MetaData) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_RequiredContextTags = { "RequiredContextTags", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, RequiredContextTags), Z_Construct_UScriptStruct_FGameplayTagContainer, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RequiredContextTags_MetaData), NewProp_RequiredContextTags_MetaData) }; // 93faf2d4041600295d23f175e0992095f880d07b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_BlockedContextTags = { "BlockedContextTags", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, BlockedContextTags), Z_Construct_UScriptStruct_FGameplayTagContainer, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_BlockedContextTags_MetaData), NewProp_BlockedContextTags_MetaData) }; // 93faf2d4041600295d23f175e0992095f880d07b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_DefinitionId = { "DefinitionId", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, DefinitionId), Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefinitionId_MetaData), NewProp_DefinitionId_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_Scope_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_Scope = { "Scope", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, Scope), Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Scope_MetaData), NewProp_Scope_MetaData) }; // 6bbc7b82fbe77bd12d5decbcedfbce5a756cd702
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Priority = { "Priority", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXCatalogEntry, Priority), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Priority_MetaData), NewProp_Priority_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bAllowParentSemanticFallback = { "bAllowParentSemanticFallback", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(FGamePlatformVFXCatalogEntry), &UHT_STATICS::NewProp_bAllowParentSemanticFallback_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAllowParentSemanticFallback_MetaData), NewProp_bAllowParentSemanticFallback_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SemanticTag,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequiredContextTags,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_BlockedContextTags,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefinitionId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Scope_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Scope,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Priority,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bAllowParentSemanticFallback,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXCatalogEntry Property Definitions *******************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXCatalogEntry",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXCatalogEntry>(),
	alignof(FGamePlatformVFXCatalogEntry),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXCatalogEntry"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXCatalogEntry ****************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Catalogs_GamePlatformVFXCatalogTypes_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry, Z_Construct_UScriptStruct_FGamePlatformVFXCatalogEntry_Statics::NewStructOps, TEXT("GamePlatformVFXCatalogEntry"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXCatalogEntry, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXCatalogEntry), 1598873014U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Catalogs_GamePlatformVFXCatalogTypes_h__Script_GamePlatformVFXClient_6487ba90f7dc555df5957d6d46c0a8124e32046e{
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
