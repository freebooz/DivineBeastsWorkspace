// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformDataVersion.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformDefinitionBase() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataVersion(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformPrimaryDataAsset(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformDefinitionBase **********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformDefinitionBase_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe5\x8f\xaa\xe8\xaf\xbb\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe6\x88\x90\xe5\x8a\x9f\xe5\x89\x8d\xe9\x80\x92\xe5\xbd\x92\xe6\x8c\x81\xe6\x9c\x89\xe5\x85\xa8\xe9\x83\xa8\xe5\xbf\x85\xe9\x9c\x80\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xe5\x9b\x9e\xe6\xbb\x9a\xe6\x95\xb4\xe6\x9d\xa1\xe7\xa7\x9f\xe7\xba\xa6\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformDefinitionBase.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformDefinitionBase.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe5\x8f\xaa\xe8\xaf\xbb\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe6\x88\x90\xe5\x8a\x9f\xe5\x89\x8d\xe9\x80\x92\xe5\xbd\x92\xe6\x8c\x81\xe6\x9c\x89\xe5\x85\xa8\xe9\x83\xa8\xe5\xbf\x85\xe9\x9c\x80\xe5\xae\x9a\xe4\xb9\x89\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xe5\x9b\x9e\xe6\xbb\x9a\xe6\x95\xb4\xe6\x9d\xa1\xe7\xa7\x9f\xe7\xba\xa6\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DataVersion_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xb5\x84\xe4\xba\xa7\xe5\xa3\xb0\xe6\x98\x8e\xe7\x9a\x84\xe7\xbb\x93\xe6\x9e\x84\xe5\x8f\x8a\xe5\x86\x85\xe5\xae\xb9\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x9b\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe4\xb8\x8d\xe5\xbe\x97\xe5\x8e\x9f\xe5\x9c\xb0\xe6\x94\xb9\xe5\x8a\xa8\xe5\x85\xb1\xe4\xba\xab\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformDefinitionBase.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xb5\x84\xe4\xba\xa7\xe5\xa3\xb0\xe6\x98\x8e\xe7\x9a\x84\xe7\xbb\x93\xe6\x9e\x84\xe5\x8f\x8a\xe5\x86\x85\xe5\xae\xb9\xe7\x89\x88\xe6\x9c\xac\xef\xbc\x9b\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x9f\xe4\xb8\x8d\xe5\xbe\x97\xe5\x8e\x9f\xe5\x9c\xb0\xe6\x94\xb9\xe5\x8a\xa8\xe5\x85\xb1\xe4\xba\xab\xe5\xae\x9a\xe4\xb9\x89\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RequiredDefinitions_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xbf\x85\xe9\x9c\x80\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe4\xb8\x8e\xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe5\x85\xb1\xe4\xba\xab\xe8\xaf\xb7\xe6\xb1\x82\xe5\x88\x86\xe7\xbb\x84\xe5\x92\x8c\xe7\xa7\x9f\xe7\xba\xa6\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x8c\xe7\xbc\xba\xe5\xa4\xb1\xe6\x88\x96\xe7\x8e\xaf\xe4\xbd\xbf\xe6\x95\xb4\xe4\xb8\xaa\xe8\xaf\xb7\xe6\xb1\x82\xe5\xa4\xb1\xe8\xb4\xa5\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformDefinitionBase.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xbf\x85\xe9\x9c\x80\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe4\xb8\x8e\xe6\xa0\xb9\xe5\xae\x9a\xe4\xb9\x89\xe5\x85\xb1\xe4\xba\xab\xe8\xaf\xb7\xe6\xb1\x82\xe5\x88\x86\xe7\xbb\x84\xe5\x92\x8c\xe7\xa7\x9f\xe7\xba\xa6\xe6\x9c\x9f\xe9\x99\x90\xef\xbc\x8c\xe7\xbc\xba\xe5\xa4\xb1\xe6\x88\x96\xe7\x8e\xaf\xe4\xbd\xbf\xe6\x95\xb4\xe4\xb8\xaa\xe8\xaf\xb7\xe6\xb1\x82\xe5\xa4\xb1\xe8\xb4\xa5\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformDefinitionBase constinit property declarations **************
	static const UECodeGen_Private::FStructPropertyParams NewProp_DataVersion;
	static const UECodeGen_Private::FStructPropertyParams NewProp_RequiredDefinitions_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_RequiredDefinitions;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformDefinitionBase constinit property declarations ****************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformDefinitionBase>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformDefinitionBase Property Definitions *************************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_DataVersion = { "DataVersion", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformDefinitionBase, DataVersion), Z_Construct_UScriptStruct_FGamePlatformDataVersion, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DataVersion_MetaData), NewProp_DataVersion_MetaData) }; // c9fd2d130f24651feb82229de0bfddea898b5a7b
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_RequiredDefinitions_Inner = { "RequiredDefinitions", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(0, nullptr) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_RequiredDefinitions = { "RequiredDefinitions", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformDefinitionBase, RequiredDefinitions), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RequiredDefinitions_MetaData), NewProp_RequiredDefinitions_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DataVersion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequiredDefinitions_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RequiredDefinitions,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformDefinitionBase Property Definitions ***************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformPrimaryDataAsset,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformDefinitionBase,
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
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformDefinitionBase;
UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformDefinitionBase;
		if (!Z_Registration_Info_UClass_UGamePlatformDefinitionBase.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformDefinitionBase"),
				Z_Registration_Info_UClass_UGamePlatformDefinitionBase.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformDefinitionBase.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformDefinitionBase.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformDefinitionBase.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformDefinitionBase.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformDefinitionBase::UGamePlatformDefinitionBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformDefinitionBase);
UGamePlatformDefinitionBase::~UGamePlatformDefinitionBase() {}
// ********** End Class UGamePlatformDefinitionBase ************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Definitions_GamePlatformDefinitionBase_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformDefinitionBase, TEXT("UGamePlatformDefinitionBase"), &Z_Registration_Info_UClass_UGamePlatformDefinitionBase, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformDefinitionBase), 2128717086U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Definitions_GamePlatformDefinitionBase_h__Script_GamePlatformData_dbf5f2b8dad54649d3a1167a685a642d82a4e491{
	TEXT("/Script/GamePlatformData"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
