// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Commandlets/GamePlatformWorldValidationCommandlet.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformWorldValidationCommandlet() {}

// ********** Begin Cross Module References ********************************************************
DATAVALIDATION_API UClass* Z_Construct_UClass_UDataValidationCommandlet(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformWorldEditor(ETypeConstructPhase);
GAMEPLATFORMWORLDEDITOR_API UClass* Z_Construct_UClass_UGamePlatformWorldValidationCommandlet(ETypeConstructPhase);
GAMEPLATFORMWORLDEDITOR_API UClass* Z_Construct_UClass_UGamePlatformWorldValidationCommandlet(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformWorldValidationCommandlet ***********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformWorldValidationCommandlet_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x96\xe7\x95\x8c\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\x93\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c\xe9\xaa\x8c\xe8\xaf\x81\xef\xbc\x9a\xe5\x8f\xaa\xe6\x89\xab\xe6\x8f\x8f\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe7\x9a\x84World/Region\xe5\xae\x9a\xe4\xb9\x89\xe5\x8f\x8a\xe6\xb4\xbe\xe7\x94\x9f\xe7\xb1\xbb\xef\xbc\x8c\xe4\xb8\x8d\xe7\x94\x9f\xe6\x88\x90\xe6\x88\x96\xe4\xbf\xae\xe6\x94\xb9\xe8\xb5\x84\xe4\xba\xa7\xe3\x80\x82\n * \xe8\xbf\x90\xe8\xa1\x8c\xe4\xba\x8e\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xef\xbc\x8c\xe5\xa4\x8d\xe7\x94\xa8\xe7\x9c\x9f\xe5\xae\x9e""EditorValidatorSubsystem\xe5\x8f\x8a\xe6\x97\xa2\xe6\x9c\x89World\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe3\x80\x82\n * -run=GamePlatformWorldEditor.GamePlatformWorldValidation\xef\xbc\x9b\xe6\x9c\xac\xe7\xb1\xbbMain\xe8\xbf\x94\xe5\x9b\x9e""0\xe4\xbb\x85\xe8\xa1\xa8\xe7\xa4\xba\xe9\x9d\x9e\xe7\xa9\xba\xe5\xae\x8c\xe6\x95\xb4\xe9\xaa\x8c\xe8\xaf\x81\xe9\x80\x9a\xe8\xbf\x87\xef\xbc\x8c\xe5\x85\xb6\xe4\xbb\x96\xe6\x83\x85\xe5\x86\xb5\xe8\xbf\x94\xe5\x9b\x9e""1\xe3\x80\x82\n * \xe6\xa8\xa1\xe5\x9d\x97\xe9\x99\x90\xe5\xae\x9a\xe5\x90\x8d\xe8\xae\xa9\xe5\xbc\x95\xe6\x93\x8e\xe5\x9c\xa8\xe6\x9f\xa5\xe6\x89\xbe""Commandlet\xe6\x97\xb6\xe6\x8f\x90\xe5\x89\x8d\xe5\x8a\xa0\xe8\xbd\xbdPostEngineInit\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe6\xa8\xa1\xe5\x9d\x97\xef\xbc\x8c\xe4\xb8\x8d\xe5\xbf\x85\xe6\x94\xb9\xe6\x8f\x92\xe4\xbb\xb6\xe5\x8a\xa0\xe8\xbd\xbd\xe9\x98\xb6\xe6\xae\xb5\xe3\x80\x82\n * \xe5\xbc\x95\xe6\x93\x8e\xe5\x90\xaf\xe5\x8a\xa8\xe5\xa4\xb1\xe8\xb4\xa5/\xe5\x85\xa8\xe5\xb1\x80\xe9\x94\x99\xe8\xaf\xaf\xe4\xbb\x8d\xe4\xbb\xa5\xe8\xbf\x9b\xe7\xa8\x8b\xe5\xae\x9e\xe9\x99\x85\xe9\x9d\x9e\xe9\x9b\xb6\xe9\x80\x80\xe5\x87\xba\xe7\xa0\x81\xe4\xb8\xba\xe5\x87\x86\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbb\x85\xe5\x87\xad\xe7\xbb\x93\xe6\x9e\x9c\xe6\x97\xa5\xe5\xbf\x97\xe5\xae\xa3\xe5\x91\x8a\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Commandlets/GamePlatformWorldValidationCommandlet.h" },
		{ "ModuleRelativePath", "Private/Commandlets/GamePlatformWorldValidationCommandlet.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x96\xe7\x95\x8c\xe5\xae\x9a\xe4\xb9\x89\xe4\xb8\x93\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c\xe9\xaa\x8c\xe8\xaf\x81\xef\xbc\x9a\xe5\x8f\xaa\xe6\x89\xab\xe6\x8f\x8f\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe7\x9a\x84World/Region\xe5\xae\x9a\xe4\xb9\x89\xe5\x8f\x8a\xe6\xb4\xbe\xe7\x94\x9f\xe7\xb1\xbb\xef\xbc\x8c\xe4\xb8\x8d\xe7\x94\x9f\xe6\x88\x90\xe6\x88\x96\xe4\xbf\xae\xe6\x94\xb9\xe8\xb5\x84\xe4\xba\xa7\xe3\x80\x82\n\xe8\xbf\x90\xe8\xa1\x8c\xe4\xba\x8e\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xef\xbc\x8c\xe5\xa4\x8d\xe7\x94\xa8\xe7\x9c\x9f\xe5\xae\x9e""EditorValidatorSubsystem\xe5\x8f\x8a\xe6\x97\xa2\xe6\x9c\x89World\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe3\x80\x82\n-run=GamePlatformWorldEditor.GamePlatformWorldValidation\xef\xbc\x9b\xe6\x9c\xac\xe7\xb1\xbbMain\xe8\xbf\x94\xe5\x9b\x9e""0\xe4\xbb\x85\xe8\xa1\xa8\xe7\xa4\xba\xe9\x9d\x9e\xe7\xa9\xba\xe5\xae\x8c\xe6\x95\xb4\xe9\xaa\x8c\xe8\xaf\x81\xe9\x80\x9a\xe8\xbf\x87\xef\xbc\x8c\xe5\x85\xb6\xe4\xbb\x96\xe6\x83\x85\xe5\x86\xb5\xe8\xbf\x94\xe5\x9b\x9e""1\xe3\x80\x82\n\xe6\xa8\xa1\xe5\x9d\x97\xe9\x99\x90\xe5\xae\x9a\xe5\x90\x8d\xe8\xae\xa9\xe5\xbc\x95\xe6\x93\x8e\xe5\x9c\xa8\xe6\x9f\xa5\xe6\x89\xbe""Commandlet\xe6\x97\xb6\xe6\x8f\x90\xe5\x89\x8d\xe5\x8a\xa0\xe8\xbd\xbdPostEngineInit\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe6\xa8\xa1\xe5\x9d\x97\xef\xbc\x8c\xe4\xb8\x8d\xe5\xbf\x85\xe6\x94\xb9\xe6\x8f\x92\xe4\xbb\xb6\xe5\x8a\xa0\xe8\xbd\xbd\xe9\x98\xb6\xe6\xae\xb5\xe3\x80\x82\n\xe5\xbc\x95\xe6\x93\x8e\xe5\x90\xaf\xe5\x8a\xa8\xe5\xa4\xb1\xe8\xb4\xa5/\xe5\x85\xa8\xe5\xb1\x80\xe9\x94\x99\xe8\xaf\xaf\xe4\xbb\x8d\xe4\xbb\xa5\xe8\xbf\x9b\xe7\xa8\x8b\xe5\xae\x9e\xe9\x99\x85\xe9\x9d\x9e\xe9\x9b\xb6\xe9\x80\x80\xe5\x87\xba\xe7\xa0\x81\xe4\xb8\xba\xe5\x87\x86\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe4\xbb\x85\xe5\x87\xad\xe7\xbb\x93\xe6\x9e\x9c\xe6\x97\xa5\xe5\xbf\x97\xe5\xae\xa3\xe5\x91\x8a\xe6\x88\x90\xe5\x8a\x9f\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformWorldValidationCommandlet constinit property declarations ***
// ********** End Class UGamePlatformWorldValidationCommandlet constinit property declarations *****
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformWorldValidationCommandlet>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UDataValidationCommandlet,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorldEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformWorldValidationCommandlet,
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
	0x000000A8u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet;
UClass* Z_Construct_UClass_UGamePlatformWorldValidationCommandlet(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformWorldValidationCommandlet;
		if (!Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformWorldValidationCommandlet"),
				Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformWorldValidationCommandlet::UGamePlatformWorldValidationCommandlet(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformWorldValidationCommandlet);
UGamePlatformWorldValidationCommandlet::~UGamePlatformWorldValidationCommandlet() {}
// ********** End Class UGamePlatformWorldValidationCommandlet *************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorldEditor_Private_Commandlets_GamePlatformWorldValidationCommandlet_h__Script_GamePlatformWorldEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformWorldValidationCommandlet, TEXT("UGamePlatformWorldValidationCommandlet"), &Z_Registration_Info_UClass_UGamePlatformWorldValidationCommandlet, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformWorldValidationCommandlet), 2926425425U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorldEditor_Private_Commandlets_GamePlatformWorldValidationCommandlet_h__Script_GamePlatformWorldEditor_a0f1aa4ab64fa35f90c47653d8c1eaf3b4aff4d6{
	TEXT("/Script/GamePlatformWorldEditor"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
