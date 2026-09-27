// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Validation/GamePlatformDefinitionValidator.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformDefinitionValidator() {}

// ********** Begin Cross Module References ********************************************************
DATAVALIDATION_API UClass* Z_Construct_UClass_UEditorValidatorBase(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformDataEditor(ETypeConstructPhase);
GAMEPLATFORMDATAEDITOR_API UClass* Z_Construct_UClass_UGamePlatformDefinitionValidator(ETypeConstructPhase);
GAMEPLATFORMDATAEDITOR_API UClass* Z_Construct_UClass_UGamePlatformDefinitionValidator(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformDefinitionValidator *****************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformDefinitionValidator_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xef\xbc\x9a\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe5\x8f\x8a""DataValidation\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c\xe5\x85\xa5\xe5\x8f\xa3\xe8\xb0\x83\xe7\x94\xa8\xef\xbc\x8c\xe9\x94\x99\xe8\xaf\xaf\xe8\xbf\x94\xe5\x9b\x9eInvalid\xe5\xb9\xb6\xe8\xbf\x9b\xe5\x85\xa5\xe9\xaa\x8c\xe8\xaf\x81\xe8\xaf\x8a\xe6\x96\xad\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Validation/GamePlatformDefinitionValidator.h" },
		{ "ModuleRelativePath", "Private/Validation/GamePlatformDefinitionValidator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xba\x90\xe8\xb5\x84\xe4\xba\xa7\xe9\xaa\x8c\xe8\xaf\x81\xe5\x99\xa8\xef\xbc\x9a\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe5\x8f\x8a""DataValidation\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c\xe5\x85\xa5\xe5\x8f\xa3\xe8\xb0\x83\xe7\x94\xa8\xef\xbc\x8c\xe9\x94\x99\xe8\xaf\xaf\xe8\xbf\x94\xe5\x9b\x9eInvalid\xe5\xb9\xb6\xe8\xbf\x9b\xe5\x85\xa5\xe9\xaa\x8c\xe8\xaf\x81\xe8\xaf\x8a\xe6\x96\xad\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformDefinitionValidator constinit property declarations *********
// ********** End Class UGamePlatformDefinitionValidator constinit property declarations ***********
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformDefinitionValidator>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UEditorValidatorBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformDataEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformDefinitionValidator,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformDefinitionValidator;
UClass* Z_Construct_UClass_UGamePlatformDefinitionValidator(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformDefinitionValidator;
		if (!Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformDefinitionValidator"),
				Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformDefinitionValidator.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformDefinitionValidator::UGamePlatformDefinitionValidator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformDefinitionValidator);
UGamePlatformDefinitionValidator::~UGamePlatformDefinitionValidator() {}
// ********** End Class UGamePlatformDefinitionValidator *******************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformDataEditor_Private_Validation_GamePlatformDefinitionValidator_h__Script_GamePlatformDataEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformDefinitionValidator, TEXT("UGamePlatformDefinitionValidator"), &Z_Registration_Info_UClass_UGamePlatformDefinitionValidator, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformDefinitionValidator), 4070236760U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformDataEditor_Private_Validation_GamePlatformDefinitionValidator_h__Script_GamePlatformDataEditor_c0f5bc999bed3323580c1e142e8cde28960630bd{
	TEXT("/Script/GamePlatformDataEditor"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
