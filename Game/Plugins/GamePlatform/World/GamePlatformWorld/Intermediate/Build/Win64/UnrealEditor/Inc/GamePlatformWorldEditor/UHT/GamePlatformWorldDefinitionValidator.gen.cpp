// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Validation/GamePlatformWorldDefinitionValidator.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformWorldDefinitionValidator() {}

// ********** Begin Cross Module References ********************************************************
DATAVALIDATION_API UClass* Z_Construct_UClass_UEditorValidatorBase(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformWorldEditor(ETypeConstructPhase);
GAMEPLATFORMWORLDEDITOR_API UClass* Z_Construct_UClass_UGamePlatformWorldDefinitionValidator(ETypeConstructPhase);
GAMEPLATFORMWORLDEDITOR_API UClass* Z_Construct_UClass_UGamePlatformWorldDefinitionValidator(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformWorldDefinitionValidator ************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformWorldDefinitionValidator_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x96\xe7\x95\x8c\xe9\xa2\x86\xe5\x9f\x9f\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xef\xbc\x9a\xe6\xa3\x80\xe6\x9f\xa5\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe5\x9c\xb0\xe5\x9b\xbe\xe7\x9a\x84\xe5\xad\x98\xe5\x9c\xa8\xe4\xb8\x8e\xe7\x9c\x9f\xe5\xae\x9eUWorld\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x81\xe5\x8c\xba\xe5\x9f\x9f\xe5\xae\x9a\xe4\xb9\x89\xe7\xb1\xbb\xe5\x9e\x8b\xe5\x8f\x8a\xe5\xae\x8c\xe6\x95\xb4\xe7\x88\xb6\xe9\x93\xbe\xe3\x80\x82\n * \xe5\x8f\xaa\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe5\x85\xa5\xe5\x8f\xa3\xe5\x90\x8c\xe6\xad\xa5\xe8\xaf\xbb\xe5\x8f\x96\xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xef\xbc\x8c\xe4\xb8\x8d\xe5\x88\x9b\xe5\xbb\xba\xe8\xbf\x90\xe8\xa1\x8c\xe5\xad\x90\xe7\xb3\xbb\xe7\xbb\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe7\xa7\xbb\xe5\x8a\xa8\xe8\xb5\x84\xe4\xba\xa7\xe6\x88\x96\xe6\x9b\xb4\xe6\x94\xb9\xe7\x94\xa8\xe6\x88\xb7\xe5\xaf\xb9\xe8\xb1\xa1\xe3\x80\x82\n * DataEditor\xe7\xbb\xa7\xe7\xbb\xad\xe8\xb4\x9f\xe8\xb4\xa3\xe9\x80\x9a\xe7\x94\xa8RequiredDefinitions\xe4\xbe\x9d\xe8\xb5\x96\xe5\x9b\xbe\xef\xbc\x9b\xe6\x9c\xac\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe4\xb8\x8d\xe8\xae\xbf\xe9\x97\xae\xe5\x85\xb6Private\xe7\xb1\xbb\xe6\x88\x96\xe5\xbb\xba\xe7\xab\x8b\xe7\xac\xac\xe4\xba\x8c\xe5\xa5\x97\xe5\x8a\xa0\xe8\xbd\xbd\xe5\x99\xa8\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Validation/GamePlatformWorldDefinitionValidator.h" },
		{ "ModuleRelativePath", "Private/Validation/GamePlatformWorldDefinitionValidator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x96\xe7\x95\x8c\xe9\xa2\x86\xe5\x9f\x9f\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xef\xbc\x9a\xe6\xa3\x80\xe6\x9f\xa5\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe5\x9c\xb0\xe5\x9b\xbe\xe7\x9a\x84\xe5\xad\x98\xe5\x9c\xa8\xe4\xb8\x8e\xe7\x9c\x9f\xe5\xae\x9eUWorld\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x81\xe5\x8c\xba\xe5\x9f\x9f\xe5\xae\x9a\xe4\xb9\x89\xe7\xb1\xbb\xe5\x9e\x8b\xe5\x8f\x8a\xe5\xae\x8c\xe6\x95\xb4\xe7\x88\xb6\xe9\x93\xbe\xe3\x80\x82\n\xe5\x8f\xaa\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xaa\x8c\xe8\xaf\x81\xe5\x85\xa5\xe5\x8f\xa3\xe5\x90\x8c\xe6\xad\xa5\xe8\xaf\xbb\xe5\x8f\x96\xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xef\xbc\x8c\xe4\xb8\x8d\xe5\x88\x9b\xe5\xbb\xba\xe8\xbf\x90\xe8\xa1\x8c\xe5\xad\x90\xe7\xb3\xbb\xe7\xbb\x9f\xef\xbc\x8c\xe4\xb8\x8d\xe7\xa7\xbb\xe5\x8a\xa8\xe8\xb5\x84\xe4\xba\xa7\xe6\x88\x96\xe6\x9b\xb4\xe6\x94\xb9\xe7\x94\xa8\xe6\x88\xb7\xe5\xaf\xb9\xe8\xb1\xa1\xe3\x80\x82\nDataEditor\xe7\xbb\xa7\xe7\xbb\xad\xe8\xb4\x9f\xe8\xb4\xa3\xe9\x80\x9a\xe7\x94\xa8RequiredDefinitions\xe4\xbe\x9d\xe8\xb5\x96\xe5\x9b\xbe\xef\xbc\x9b\xe6\x9c\xac\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe4\xb8\x8d\xe8\xae\xbf\xe9\x97\xae\xe5\x85\xb6Private\xe7\xb1\xbb\xe6\x88\x96\xe5\xbb\xba\xe7\xab\x8b\xe7\xac\xac\xe4\xba\x8c\xe5\xa5\x97\xe5\x8a\xa0\xe8\xbd\xbd\xe5\x99\xa8\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformWorldDefinitionValidator constinit property declarations ****
// ********** End Class UGamePlatformWorldDefinitionValidator constinit property declarations ******
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformWorldDefinitionValidator>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UEditorValidatorBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorldEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformWorldDefinitionValidator,
	"Editor",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x000000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator;
UClass* Z_Construct_UClass_UGamePlatformWorldDefinitionValidator(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformWorldDefinitionValidator;
		if (!Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformWorldDefinitionValidator"),
				Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformWorldDefinitionValidator::UGamePlatformWorldDefinitionValidator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformWorldDefinitionValidator);
UGamePlatformWorldDefinitionValidator::~UGamePlatformWorldDefinitionValidator() {}
// ********** End Class UGamePlatformWorldDefinitionValidator **************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorldEditor_Private_Validation_GamePlatformWorldDefinitionValidator_h__Script_GamePlatformWorldEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformWorldDefinitionValidator, TEXT("UGamePlatformWorldDefinitionValidator"), &Z_Registration_Info_UClass_UGamePlatformWorldDefinitionValidator, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformWorldDefinitionValidator), 1590812278U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorldEditor_Private_Validation_GamePlatformWorldDefinitionValidator_h__Script_GamePlatformWorldEditor_96c2f5df5fa66fa507d0100ea7c53bcafc7dd8fd{
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
