// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Loading/GamePlatformAssetManager.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformAssetManager() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UAssetManager(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformAssetManager(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformAssetManager(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformAssetManager ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformAssetManager_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x9c\xa8\xe4\xb8\xbb\xe5\xb7\xa5\xe7\xa8\x8b\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x80\xe6\xac\xa1\xe7\x9a\x84\xe5\xbc\x95\xe6\x93\x8e\xe8\xb5\x84\xe4\xba\xa7\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\xe3\x80\x82\xe8\xbf\x9b\xe7\xa8\x8b\xe7\x99\xbb\xe8\xae\xb0\xe8\xa1\xa8\xe7\xa7\x81\xe6\x9c\x89\xef\xbc\x8c\xe4\xb8\x9a\xe5\x8a\xa1\xe5\xbf\x85\xe9\xa1\xbb\xe9\x80\x9a\xe8\xbf\x87\xe5\xae\x9e\xe4\xbe\x8b\xe6\x9c\x8d\xe5\x8a\xa1\xe7\x99\xbb\xe8\xae\xb0\xe7\xa7\x9f\xe7\xba\xa6\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Loading/GamePlatformAssetManager.h" },
		{ "ModuleRelativePath", "Public/Loading/GamePlatformAssetManager.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x9c\xa8\xe4\xb8\xbb\xe5\xb7\xa5\xe7\xa8\x8b\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x80\xe6\xac\xa1\xe7\x9a\x84\xe5\xbc\x95\xe6\x93\x8e\xe8\xb5\x84\xe4\xba\xa7\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\xe3\x80\x82\xe8\xbf\x9b\xe7\xa8\x8b\xe7\x99\xbb\xe8\xae\xb0\xe8\xa1\xa8\xe7\xa7\x81\xe6\x9c\x89\xef\xbc\x8c\xe4\xb8\x9a\xe5\x8a\xa1\xe5\xbf\x85\xe9\xa1\xbb\xe9\x80\x9a\xe8\xbf\x87\xe5\xae\x9e\xe4\xbe\x8b\xe6\x9c\x8d\xe5\x8a\xa1\xe7\x99\xbb\xe8\xae\xb0\xe7\xa7\x9f\xe7\xba\xa6\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformAssetManager constinit property declarations ****************
// ********** End Class UGamePlatformAssetManager constinit property declarations ******************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformAssetManager>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UAssetManager,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformAssetManager,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformAssetManager;
UClass* Z_Construct_UClass_UGamePlatformAssetManager(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformAssetManager;
		if (!Z_Registration_Info_UClass_UGamePlatformAssetManager.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformAssetManager"),
				Z_Registration_Info_UClass_UGamePlatformAssetManager.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformAssetManager.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformAssetManager.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformAssetManager.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformAssetManager.OuterSingleton;
}
#undef UHT_STATICS
// ********** End Class UGamePlatformAssetManager **************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Loading_GamePlatformAssetManager_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformAssetManager, TEXT("UGamePlatformAssetManager"), &Z_Registration_Info_UClass_UGamePlatformAssetManager, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformAssetManager), 4084584564U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Loading_GamePlatformAssetManager_h__Script_GamePlatformData_884d2ce9c6a009512f48df6f28ac0b90964e462d{
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
