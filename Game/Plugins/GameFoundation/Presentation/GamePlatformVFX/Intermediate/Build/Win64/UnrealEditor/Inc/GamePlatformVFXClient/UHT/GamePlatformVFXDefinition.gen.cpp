// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Types/GamePlatformVFXParameters.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXDefinition() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UPrimaryDataAsset(ETypeConstructPhase);
NIAGARA_API UClass* Z_Construct_UClass_UNiagaraSystem(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinition(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXParameters(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXDefinition ***********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * VFX \xe4\xb8\xbb\xe6\x95\xb0\xe6\x8d\xae\xe5\xae\x9a\xe4\xb9\x89\xe6\xa0\xb9\xe7\xb1\xbb\xe3\x80\x82\n * StableId \xe6\x98\xaf\xe7\xa8\xb3\xe5\xae\x9a\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe5\x85\x81\xe8\xae\xb8\xe7\x94\xb1\xe8\xb5\x84\xe4\xba\xa7\xe6\x96\x87\xe4\xbb\xb6\xe5\x90\x8d\xe9\x9a\x90\xe5\xbc\x8f\xe6\x8e\xa8\xe5\xaf\xbc\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformVFXDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe4\xb8\xbb\xe6\x95\xb0\xe6\x8d\xae\xe5\xae\x9a\xe4\xb9\x89\xe6\xa0\xb9\xe7\xb1\xbb\xe3\x80\x82\nStableId \xe6\x98\xaf\xe7\xa8\xb3\xe5\xae\x9a\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xb8\x8d\xe5\x85\x81\xe8\xae\xb8\xe7\x94\xb1\xe8\xb5\x84\xe4\xba\xa7\xe6\x96\x87\xe4\xbb\xb6\xe5\x90\x8d\xe9\x9a\x90\xe5\xbc\x8f\xe6\x8e\xa8\xe5\xaf\xbc\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StableId_MetaData[] = {
		{ "Category", "Identity" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SchemaVersion_MetaData[] = {
		{ "Category", "Version" },
		{ "ClampMin", "1" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ContentRevision_MetaData[] = {
		{ "Category", "Version" },
		{ "ClampMin", "0" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ContentCategory_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Behavior_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NiagaraSystem_MetaData[] = {
		{ "AssetBundles", "Client" },
		{ "Category", "VFX" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Client Bundle \xe7\x94\xa8\xe4\xba\x8e\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe8\xa1\xa8\xe7\x8e\xb0\xe8\xb5\x84\xe6\xba\x90\xe5\x88\x86\xe7\xbb\x84\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Client Bundle \xe7\x94\xa8\xe4\xba\x8e\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe8\xa1\xa8\xe7\x8e\xb0\xe8\xb5\x84\xe6\xba\x90\xe5\x88\x86\xe7\xbb\x84\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefaultParameters_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PoolingMode_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefaultImportance_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAutoDestroy_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bPreCullCheck_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXDefinition.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXDefinition constinit property declarations ***************
	static const UECodeGen_Private::FNamePropertyParams NewProp_StableId;
	static const UECodeGen_Private::FIntPropertyParams NewProp_SchemaVersion;
	static const UECodeGen_Private::FIntPropertyParams NewProp_ContentRevision;
	static const UECodeGen_Private::FNamePropertyParams NewProp_ContentCategory;
	static const UECodeGen_Private::FBytePropertyParams NewProp_Behavior_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Behavior;
	static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_NiagaraSystem;
	static const UECodeGen_Private::FStructPropertyParams NewProp_DefaultParameters;
	static const UECodeGen_Private::FBytePropertyParams NewProp_PoolingMode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_PoolingMode;
	static const UECodeGen_Private::FBytePropertyParams NewProp_DefaultImportance_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_DefaultImportance;
	static void NewProp_bAutoDestroy_SetBit(void* Obj)
	{
		((UGamePlatformVFXDefinition*)Obj)->bAutoDestroy = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAutoDestroy;
	static void NewProp_bPreCullCheck_SetBit(void* Obj)
	{
		((UGamePlatformVFXDefinition*)Obj)->bPreCullCheck = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bPreCullCheck;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformVFXDefinition constinit property declarations *****************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformVFXDefinition Property Definitions **************************
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_StableId = { "StableId", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, StableId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StableId_MetaData), NewProp_StableId_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_SchemaVersion = { "SchemaVersion", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, SchemaVersion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SchemaVersion_MetaData), NewProp_SchemaVersion_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_ContentRevision = { "ContentRevision", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, ContentRevision), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ContentRevision_MetaData), NewProp_ContentRevision_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_ContentCategory = { "ContentCategory", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, ContentCategory), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ContentCategory_MetaData), NewProp_ContentCategory_MetaData) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_Behavior_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_Behavior = { "Behavior", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, Behavior), Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Behavior_MetaData), NewProp_Behavior_MetaData) }; // fa3d38034f24a243a2556af8fd3a0ee35f323fc8
const UECodeGen_Private::FSoftObjectPropertyParams UHT_STATICS::NewProp_NiagaraSystem = { "NiagaraSystem", nullptr, (EPropertyFlags)0x0014000000000015, UECodeGen_Private::EPropertyGenFlags::SoftObject, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, NiagaraSystem), Z_Construct_UClass_UNiagaraSystem, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NiagaraSystem_MetaData), NewProp_NiagaraSystem_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_DefaultParameters = { "DefaultParameters", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, DefaultParameters), Z_Construct_UScriptStruct_FGamePlatformVFXParameters, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefaultParameters_MetaData), NewProp_DefaultParameters_MetaData) }; // f0817a3267f68b32f73d8fcfd4b7a8c4e3369716
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_PoolingMode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_PoolingMode = { "PoolingMode", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, PoolingMode), Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PoolingMode_MetaData), NewProp_PoolingMode_MetaData) }; // e200ae9e3f34268f2f1b7ea98802e4d4b4235d94
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_DefaultImportance_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_DefaultImportance = { "DefaultImportance", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXDefinition, DefaultImportance), Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefaultImportance_MetaData), NewProp_DefaultImportance_MetaData) }; // 3d70a34062b22947f968a62f1473bd0862f8865b
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bAutoDestroy = { "bAutoDestroy", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UGamePlatformVFXDefinition), &UHT_STATICS::NewProp_bAutoDestroy_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAutoDestroy_MetaData), NewProp_bAutoDestroy_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bPreCullCheck = { "bPreCullCheck", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UGamePlatformVFXDefinition), &UHT_STATICS::NewProp_bPreCullCheck_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bPreCullCheck_MetaData), NewProp_bPreCullCheck_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_StableId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SchemaVersion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ContentRevision,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ContentCategory,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Behavior_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Behavior,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NiagaraSystem,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefaultParameters,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PoolingMode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PoolingMode,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefaultImportance_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefaultImportance,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bAutoDestroy,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bPreCullCheck,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformVFXDefinition Property Definitions ****************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UPrimaryDataAsset,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXDefinition,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001000A1u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXDefinition;
UClass* Z_Construct_UClass_UGamePlatformVFXDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXDefinition"),
				Z_Registration_Info_UClass_UGamePlatformVFXDefinition.InnerSingleton,
				nullptr,
				DataSizeOf<TClass>(),
				alignof(TClass),
				TClass::StaticClassFlags,
				TClass::StaticClassCastFlags(),
				TClass::StaticConfigName(),
				(UClass::ClassConstructorType)InternalConstructor<TClass>,
				(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
				UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
				&TClass::Super::StaticClass,
				&TClass::WithinClass::StaticClass
			);
		}
		return Z_Registration_Info_UClass_UGamePlatformVFXDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXDefinition.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformVFXDefinition::UGamePlatformVFXDefinition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXDefinition);
UGamePlatformVFXDefinition::~UGamePlatformVFXDefinition() {}
// ********** End Class UGamePlatformVFXDefinition *************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXDefinition_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXDefinition, TEXT("UGamePlatformVFXDefinition"), &Z_Registration_Info_UClass_UGamePlatformVFXDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXDefinition), 3586896424U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXDefinition_h__Script_GamePlatformVFXClient_c45a948ac59c35315209e37299b10b48966e3cdd{
	TEXT("/Script/GamePlatformVFXClient"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
