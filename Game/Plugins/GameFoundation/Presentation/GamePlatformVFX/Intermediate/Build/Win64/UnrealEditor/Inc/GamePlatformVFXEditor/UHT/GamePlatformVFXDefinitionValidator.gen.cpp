// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Validation/GamePlatformVFXDefinitionValidator.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXDefinitionValidator() {}

// ********** Begin Cross Module References ********************************************************
DATAVALIDATION_API UClass* Z_Construct_UClass_UEditorValidatorBase(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXEditor(ETypeConstructPhase);
GAMEPLATFORMVFXEDITOR_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinitionValidator(ETypeConstructPhase);
GAMEPLATFORMVFXEDITOR_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinitionValidator(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXDefinitionValidator **************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXDefinitionValidator_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "IncludePath", "Validation/GamePlatformVFXDefinitionValidator.h" },
		{ "ModuleRelativePath", "Private/Validation/GamePlatformVFXDefinitionValidator.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXDefinitionValidator constinit property declarations ******
// ********** End Class UGamePlatformVFXDefinitionValidator constinit property declarations ********
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXDefinitionValidator>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UEditorValidatorBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXDefinitionValidator,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator;
UClass* Z_Construct_UClass_UGamePlatformVFXDefinitionValidator(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXDefinitionValidator;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXDefinitionValidator"),
				Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformVFXDefinitionValidator::UGamePlatformVFXDefinitionValidator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXDefinitionValidator);
UGamePlatformVFXDefinitionValidator::~UGamePlatformVFXDefinitionValidator() {}
// ********** End Class UGamePlatformVFXDefinitionValidator ****************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXEditor_Private_Validation_GamePlatformVFXDefinitionValidator_h__Script_GamePlatformVFXEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXDefinitionValidator, TEXT("UGamePlatformVFXDefinitionValidator"), &Z_Registration_Info_UClass_UGamePlatformVFXDefinitionValidator, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXDefinitionValidator), 1730084854U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXEditor_Private_Validation_GamePlatformVFXDefinitionValidator_h__Script_GamePlatformVFXEditor_ce245ba75ab68c970481231eaba98bf86eea4bd1{
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
