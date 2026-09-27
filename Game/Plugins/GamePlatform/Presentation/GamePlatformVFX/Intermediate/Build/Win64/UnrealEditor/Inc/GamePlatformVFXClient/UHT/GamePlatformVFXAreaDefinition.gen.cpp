// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformVFXAreaDefinition.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXAreaDefinition() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXAreaDefinition(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinition(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXAreaDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXAreaDefinition *******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXAreaDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "Definitions/GamePlatformVFXAreaDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXAreaDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VisualRadius_MetaData[] = {
		{ "Category", "Area" },
		{ "ClampMin", "0.0" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXAreaDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RadiusParameter_MetaData[] = {
		{ "Category", "Area" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXAreaDefinition.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXAreaDefinition constinit property declarations ***********
	static const UECodeGen_Private::FFloatPropertyParams NewProp_VisualRadius;
	static const UECodeGen_Private::FNamePropertyParams NewProp_RadiusParameter;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformVFXAreaDefinition constinit property declarations *************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXAreaDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformVFXAreaDefinition Property Definitions **********************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_VisualRadius = { "VisualRadius", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXAreaDefinition, VisualRadius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VisualRadius_MetaData), NewProp_VisualRadius_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_RadiusParameter = { "RadiusParameter", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXAreaDefinition, RadiusParameter), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RadiusParameter_MetaData), NewProp_RadiusParameter_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_VisualRadius,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RadiusParameter,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformVFXAreaDefinition Property Definitions ************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformVFXDefinition,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXAreaDefinition,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition;
UClass* Z_Construct_UClass_UGamePlatformVFXAreaDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXAreaDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXAreaDefinition"),
				Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXAreaDefinition);
UGamePlatformVFXAreaDefinition::~UGamePlatformVFXAreaDefinition() {}
// ********** End Class UGamePlatformVFXAreaDefinition *********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXAreaDefinition_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXAreaDefinition, TEXT("UGamePlatformVFXAreaDefinition"), &Z_Registration_Info_UClass_UGamePlatformVFXAreaDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXAreaDefinition), 1264291310U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXAreaDefinition_h__Script_GamePlatformVFXClient_60289fa61c44322fdb13ee0d1453016671b65d6a{
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
