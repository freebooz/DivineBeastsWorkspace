// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformVFXTrailDefinition.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXTrailDefinition() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXDefinition(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXTrailDefinition(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXTrailDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXTrailDefinition ******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXTrailDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "Definitions/GamePlatformVFXTrailDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXTrailDefinition.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DefaultWidth_MetaData[] = {
		{ "Category", "Trail" },
		{ "ClampMin", "0.0" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformVFXTrailDefinition.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXTrailDefinition constinit property declarations **********
	static const UECodeGen_Private::FFloatPropertyParams NewProp_DefaultWidth;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformVFXTrailDefinition constinit property declarations ************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXTrailDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformVFXTrailDefinition Property Definitions *********************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_DefaultWidth = { "DefaultWidth", nullptr, (EPropertyFlags)0x0010000000000015, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXTrailDefinition, DefaultWidth), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DefaultWidth_MetaData), NewProp_DefaultWidth_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DefaultWidth,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformVFXTrailDefinition Property Definitions ***********************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformVFXDefinition,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXTrailDefinition,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition;
UClass* Z_Construct_UClass_UGamePlatformVFXTrailDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXTrailDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXTrailDefinition"),
				Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXTrailDefinition);
UGamePlatformVFXTrailDefinition::~UGamePlatformVFXTrailDefinition() {}
// ********** End Class UGamePlatformVFXTrailDefinition ********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXTrailDefinition_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXTrailDefinition, TEXT("UGamePlatformVFXTrailDefinition"), &Z_Registration_Info_UClass_UGamePlatformVFXTrailDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXTrailDefinition), 3135737695U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Definitions_GamePlatformVFXTrailDefinition_h__Script_GamePlatformVFXClient_acd7fe72f4fd3d9a7ae7dd9d71b2d35dafb5271b{
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
