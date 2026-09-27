// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXSpawnContext.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXSpawnContext() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FTransform(ETypeConstructPhase);
ENGINE_API UClass* Z_Construct_UClass_AActor(ETypeConstructPhase);
ENGINE_API UClass* Z_Construct_UClass_USceneComponent(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXSpawnContext **************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXSpawnContext>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXSpawnContext); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8d\x95\xe6\xac\xa1 VFX \xe6\x92\xad\xe6\x94\xbe\xe7\x9a\x84\xe4\xb8\x96\xe7\x95\x8c/\xe9\x99\x84\xe7\x9d\x80\xe4\xb8\x8a\xe4\xb8\x8b\xe6\x96\x87\xef\xbc\x9b\xe4\xb8\x8d\xe5\x86\x99\xe5\x9b\x9e Definition \xe9\x9d\x99\xe6\x80\x81\xe8\xb5\x84\xe4\xba\xa7\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8d\x95\xe6\xac\xa1 VFX \xe6\x92\xad\xe6\x94\xbe\xe7\x9a\x84\xe4\xb8\x96\xe7\x95\x8c/\xe9\x99\x84\xe7\x9d\x80\xe4\xb8\x8a\xe4\xb8\x8b\xe6\x96\x87\xef\xbc\x9b\xe4\xb8\x8d\xe5\x86\x99\xe5\x9b\x9e Definition \xe9\x9d\x99\xe6\x80\x81\xe8\xb5\x84\xe4\xba\xa7\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_WorldTransform_MetaData[] = {
		{ "Category", "VFX" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x9c\xaa\xe9\x99\x84\xe7\x9d\x80\xe6\x97\xb6\xe4\xbd\xbf\xe7\x94\xa8\xe7\x9a\x84\xe4\xb8\x96\xe7\x95\x8c\xe5\x8f\x98\xe6\x8d\xa2\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x9c\xaa\xe9\x99\x84\xe7\x9d\x80\xe6\x97\xb6\xe4\xbd\xbf\xe7\x94\xa8\xe7\x9a\x84\xe4\xb8\x96\xe7\x95\x8c\xe5\x8f\x98\xe6\x8d\xa2\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttachComponent_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaf\xe9\x80\x89\xe9\x99\x84\xe7\x9d\x80\xe7\xbb\x84\xe4\xbb\xb6\xe3\x80\x82\xe5\xbc\x82\xe6\xad\xa5\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x9c\x9f\xe9\x97\xb4\xe4\xbd\xbf\xe7\x94\xa8\xe5\xbc\xb1\xe5\xbc\x95\xe7\x94\xa8\xef\xbc\x8c\xe7\x9b\xae\xe6\xa0\x87\xe5\xa4\xb1\xe6\x95\x88\xe5\x90\x8e\xe8\xaf\xb7\xe6\xb1\x82\xe4\xbc\x9a\xe5\xa4\xb1\xe8\xb4\xa5\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaf\xe9\x80\x89\xe9\x99\x84\xe7\x9d\x80\xe7\xbb\x84\xe4\xbb\xb6\xe3\x80\x82\xe5\xbc\x82\xe6\xad\xa5\xe5\x8a\xa0\xe8\xbd\xbd\xe6\x9c\x9f\xe9\x97\xb4\xe4\xbd\xbf\xe7\x94\xa8\xe5\xbc\xb1\xe5\xbc\x95\xe7\x94\xa8\xef\xbc\x8c\xe7\x9b\xae\xe6\xa0\x87\xe5\xa4\xb1\xe6\x95\x88\xe5\x90\x8e\xe8\xaf\xb7\xe6\xb1\x82\xe4\xbc\x9a\xe5\xa4\xb1\xe8\xb4\xa5\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttachSocket_MetaData[] = {
		{ "Category", "VFX" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe9\x99\x84\xe7\x9d\x80 Socket / Bone \xe5\x90\x8d\xe7\xa7\xb0\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe9\x99\x84\xe7\x9d\x80 Socket / Bone \xe5\x90\x8d\xe7\xa7\xb0\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SourceActor_MetaData[] = {
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TargetActor_MetaData[] = {
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXSpawnContext.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXSpawnContext constinit property declarations ******
	static const UECodeGen_Private::FStructPropertyParams NewProp_WorldTransform;
	static const UECodeGen_Private::FWeakObjectPropertyParams NewProp_AttachComponent;
	static const UECodeGen_Private::FNamePropertyParams NewProp_AttachSocket;
	static const UECodeGen_Private::FWeakObjectPropertyParams NewProp_SourceActor;
	static const UECodeGen_Private::FWeakObjectPropertyParams NewProp_TargetActor;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXSpawnContext constinit property declarations ********
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXSpawnContext>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXSpawnContext Property Definitions *****************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_WorldTransform = { "WorldTransform", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXSpawnContext, WorldTransform), Z_Construct_UScriptStruct_FTransform, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_WorldTransform_MetaData), NewProp_WorldTransform_MetaData) };
const UECodeGen_Private::FWeakObjectPropertyParams UHT_STATICS::NewProp_AttachComponent = { "AttachComponent", nullptr, (EPropertyFlags)0x0014000000082008, UECodeGen_Private::EPropertyGenFlags::WeakObject, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXSpawnContext, AttachComponent), Z_Construct_UClass_USceneComponent, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttachComponent_MetaData), NewProp_AttachComponent_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_AttachSocket = { "AttachSocket", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXSpawnContext, AttachSocket), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttachSocket_MetaData), NewProp_AttachSocket_MetaData) };
const UECodeGen_Private::FWeakObjectPropertyParams UHT_STATICS::NewProp_SourceActor = { "SourceActor", nullptr, (EPropertyFlags)0x0014000000002000, UECodeGen_Private::EPropertyGenFlags::WeakObject, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXSpawnContext, SourceActor), Z_Construct_UClass_AActor, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SourceActor_MetaData), NewProp_SourceActor_MetaData) };
const UECodeGen_Private::FWeakObjectPropertyParams UHT_STATICS::NewProp_TargetActor = { "TargetActor", nullptr, (EPropertyFlags)0x0014000000002000, UECodeGen_Private::EPropertyGenFlags::WeakObject, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXSpawnContext, TargetActor), Z_Construct_UClass_AActor, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TargetActor_MetaData), NewProp_TargetActor_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_WorldTransform,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttachComponent,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttachSocket,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SourceActor,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_TargetActor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXSpawnContext Property Definitions *******************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXSpawnContext",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXSpawnContext>(),
	alignof(FGamePlatformVFXSpawnContext),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000205),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXSpawnContext"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXSpawnContext ****************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXSpawnContext_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext, Z_Construct_UScriptStruct_FGamePlatformVFXSpawnContext_Statics::NewStructOps, TEXT("GamePlatformVFXSpawnContext"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXSpawnContext, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXSpawnContext), 3539499288U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXSpawnContext_h__Script_GamePlatformVFXClient_5a321795e25b2ed64d893b367efde5b7417d5f8a{
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
