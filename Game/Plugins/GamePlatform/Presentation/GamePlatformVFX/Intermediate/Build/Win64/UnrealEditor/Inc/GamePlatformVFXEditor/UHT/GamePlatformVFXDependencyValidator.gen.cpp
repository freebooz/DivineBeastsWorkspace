// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Validation/GamePlatformVFXDependencyValidator.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXDependencyValidator() {}

// ********** Begin Cross Module References ********************************************************
DATAVALIDATION_API UClass* Z_Construct_UClass_UEditorValidatorBase(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXEditor(ETypeConstructPhase);
GAMEPLATFORMVFXEDITOR_API UClass* Z_Construct_UClass_UGamePlatformVFXDependencyValidator(ETypeConstructPhase);
GAMEPLATFORMVFXEDITOR_API UClass* Z_Construct_UClass_UGamePlatformVFXDependencyValidator(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXDependencyValidator **************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXDependencyValidator_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe8\xb7\xa8\xe5\xb1\x82\xe4\xbe\x9d\xe8\xb5\x96\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe9\xaa\xa8\xe6\x9e\xb6\xe3\x80\x82\n * \xe7\x9c\x9f\xe5\xae\x9e\xe7\xa6\x81\xe7\x94\xa8\xe8\xb7\xaf\xe5\xbe\x84\xe9\x9c\x80\xe8\xa6\x81\xe8\xaf\xbb\xe5\x8f\x96\xe7\xa5\x9e\xe5\x85\xbd\xe8\x81\x94\xe7\x9b\x9f\xe4\xbb\x93\xe5\xba\x93\xe5\xae\x9e\xe9\x99\x85\xe6\x8f\x92\xe4\xbb\xb6\xe6\x8c\x82\xe8\xbd\xbd\xe7\x82\xb9\xe5\x90\x8e\xe9\x85\x8d\xe7\xbd\xae\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe5\x9c\xa8\xe7\x8b\xac\xe7\xab\x8b\xe6\xba\x90\xe7\xa0\x81\xe5\x8c\x85\xe4\xb8\xad\xe8\x87\x86\xe9\x80\xa0\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Validation/GamePlatformVFXDependencyValidator.h" },
		{ "ModuleRelativePath", "Private/Validation/GamePlatformVFXDependencyValidator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xb7\xa8\xe5\xb1\x82\xe4\xbe\x9d\xe8\xb5\x96\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xe9\xaa\xa8\xe6\x9e\xb6\xe3\x80\x82\n\xe7\x9c\x9f\xe5\xae\x9e\xe7\xa6\x81\xe7\x94\xa8\xe8\xb7\xaf\xe5\xbe\x84\xe9\x9c\x80\xe8\xa6\x81\xe8\xaf\xbb\xe5\x8f\x96\xe7\xa5\x9e\xe5\x85\xbd\xe8\x81\x94\xe7\x9b\x9f\xe4\xbb\x93\xe5\xba\x93\xe5\xae\x9e\xe9\x99\x85\xe6\x8f\x92\xe4\xbb\xb6\xe6\x8c\x82\xe8\xbd\xbd\xe7\x82\xb9\xe5\x90\x8e\xe9\x85\x8d\xe7\xbd\xae\xef\xbc\x8c\xe4\xb8\x8d\xe8\x83\xbd\xe5\x9c\xa8\xe7\x8b\xac\xe7\xab\x8b\xe6\xba\x90\xe7\xa0\x81\xe5\x8c\x85\xe4\xb8\xad\xe8\x87\x86\xe9\x80\xa0\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXDependencyValidator constinit property declarations ******
// ********** End Class UGamePlatformVFXDependencyValidator constinit property declarations ********
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXDependencyValidator>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UEditorValidatorBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXDependencyValidator,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator;
UClass* Z_Construct_UClass_UGamePlatformVFXDependencyValidator(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXDependencyValidator;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXDependencyValidator"),
				Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformVFXDependencyValidator::UGamePlatformVFXDependencyValidator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXDependencyValidator);
UGamePlatformVFXDependencyValidator::~UGamePlatformVFXDependencyValidator() {}
// ********** End Class UGamePlatformVFXDependencyValidator ****************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXEditor_Private_Validation_GamePlatformVFXDependencyValidator_h__Script_GamePlatformVFXEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXDependencyValidator, TEXT("UGamePlatformVFXDependencyValidator"), &Z_Registration_Info_UClass_UGamePlatformVFXDependencyValidator, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXDependencyValidator), 3770315532U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXEditor_Private_Validation_GamePlatformVFXDependencyValidator_h__Script_GamePlatformVFXEditor_bcf360e853f3c92e1aa6c17f87b685f62da89dc5{
	TEXT("/Script/GamePlatformVFXEditor"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
