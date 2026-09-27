// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformPrimaryDataAsset() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UPrimaryDataAsset(ETypeConstructPhase);
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformId(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformPrimaryDataAsset(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformPrimaryDataAsset(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformPrimaryDataAsset ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformPrimaryDataAsset_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe5\x9f\xba\xe7\xb1\xbb\xef\xbc\x9b\xe8\xba\xab\xe4\xbb\xbd\xe4\xb8\x8e\xe6\x96\x87\xe4\xbb\xb6\xe5\x90\x8d\xe7\xa7\xb0\xe3\x80\x81\xe8\xb7\xaf\xe5\xbe\x84\xe3\x80\x81\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d\xe6\x97\xa0\xe5\x85\xb3\xe3\x80\x82\xe5\x8f\xaa\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe8\xae\xbf\xe9\x97\xaeUObject\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformPrimaryDataAsset.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformPrimaryDataAsset.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe5\x9f\xba\xe7\xb1\xbb\xef\xbc\x9b\xe8\xba\xab\xe4\xbb\xbd\xe4\xb8\x8e\xe6\x96\x87\xe4\xbb\xb6\xe5\x90\x8d\xe7\xa7\xb0\xe3\x80\x81\xe8\xb7\xaf\xe5\xbe\x84\xe3\x80\x81\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d\xe6\x97\xa0\xe5\x85\xb3\xe3\x80\x82\xe5\x8f\xaa\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe8\xae\xbf\xe9\x97\xaeUObject\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LogicalId_MetaData[] = {
		{ "Category", "GamePlatform|Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x94\xaf\xe4\xb8\x80\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xbf\x9d\xe5\xad\x98\xe5\x90\x8e\xe8\xbf\x9b\xe5\x85\xa5\xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xe6\xb3\xa8\xe5\x86\x8c\xe8\xa1\xa8\xef\xbc\x9b\xe5\xb7\xb2\xe5\x8f\x91\xe5\xb8\x83\xe8\xba\xab\xe4\xbb\xbd\xe5\x8f\x98\xe6\x9b\xb4\xe9\xa1\xbb\xe8\xbf\x81\xe7\xa7\xbb\xe5\xbc\x95\xe7\x94\xa8\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformPrimaryDataAsset.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x94\xaf\xe4\xb8\x80\xe9\x80\xbb\xe8\xbe\x91\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x8c\xe4\xbf\x9d\xe5\xad\x98\xe5\x90\x8e\xe8\xbf\x9b\xe5\x85\xa5\xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xe6\xb3\xa8\xe5\x86\x8c\xe8\xa1\xa8\xef\xbc\x9b\xe5\xb7\xb2\xe5\x8f\x91\xe5\xb8\x83\xe8\xba\xab\xe4\xbb\xbd\xe5\x8f\x98\xe6\x9b\xb4\xe9\xa1\xbb\xe8\xbf\x81\xe7\xa7\xbb\xe5\xbc\x95\xe7\x94\xa8\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformPrimaryDataAsset constinit property declarations ************
	static const UECodeGen_Private::FStructPropertyParams NewProp_LogicalId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformPrimaryDataAsset constinit property declarations **************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformPrimaryDataAsset>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformPrimaryDataAsset Property Definitions ***********************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_LogicalId = { "LogicalId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformPrimaryDataAsset, LogicalId), Z_Construct_UScriptStruct_FGamePlatformId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LogicalId_MetaData), NewProp_LogicalId_MetaData) }; // 286244d92f71be2940db0b00e936e92e3a915491
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LogicalId,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformPrimaryDataAsset Property Definitions *************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UPrimaryDataAsset,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformPrimaryDataAsset,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset;
UClass* Z_Construct_UClass_UGamePlatformPrimaryDataAsset(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformPrimaryDataAsset;
		if (!Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformPrimaryDataAsset"),
				Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformPrimaryDataAsset::UGamePlatformPrimaryDataAsset(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformPrimaryDataAsset);
UGamePlatformPrimaryDataAsset::~UGamePlatformPrimaryDataAsset() {}
// ********** End Class UGamePlatformPrimaryDataAsset **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Definitions_GamePlatformPrimaryDataAsset_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformPrimaryDataAsset, TEXT("UGamePlatformPrimaryDataAsset"), &Z_Registration_Info_UClass_UGamePlatformPrimaryDataAsset, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformPrimaryDataAsset), 2540482585U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Definitions_GamePlatformPrimaryDataAsset_h__Script_GamePlatformData_32a6dde0822738497cb2f6661fb4516b541d0d68{
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
