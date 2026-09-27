// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Settings/GamePlatformVFXSettings.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXSettings() {}

// ********** Begin Cross Module References ********************************************************
DEVELOPERSETTINGS_API UClass* Z_Construct_UClass_UDeveloperSettings(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXSettings(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXSettings(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXSettings *************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXSettings_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "DisplayName", "Game Platform VFX / \xe6\xb8\xb8\xe6\x88\x8f\xe5\xb9\xb3\xe5\x8f\xb0\xe8\xa7\x86\xe8\xa7\x89\xe7\x89\xb9\xe6\x95\x88" },
		{ "IncludePath", "Settings/GamePlatformVFXSettings.h" },
		{ "ModuleRelativePath", "Public/Settings/GamePlatformVFXSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxActiveInstances_MetaData[] = {
		{ "Category", "Budget" },
		{ "ClampMin", "1" },
		{ "ModuleRelativePath", "Public/Settings/GamePlatformVFXSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxAmbientInstances_MetaData[] = {
		{ "Category", "Budget" },
		{ "ClampMin", "0" },
		{ "ModuleRelativePath", "Public/Settings/GamePlatformVFXSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bEnableDiagnostics_MetaData[] = {
		{ "Category", "Diagnostics" },
		{ "ModuleRelativePath", "Public/Settings/GamePlatformVFXSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAllowPlatformFallback_MetaData[] = {
		{ "Category", "Fallback" },
		{ "ModuleRelativePath", "Public/Settings/GamePlatformVFXSettings.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXSettings constinit property declarations *****************
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxActiveInstances;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxAmbientInstances;
	static void NewProp_bEnableDiagnostics_SetBit(void* Obj)
	{
		((UGamePlatformVFXSettings*)Obj)->bEnableDiagnostics = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bEnableDiagnostics;
	static void NewProp_bAllowPlatformFallback_SetBit(void* Obj)
	{
		((UGamePlatformVFXSettings*)Obj)->bAllowPlatformFallback = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAllowPlatformFallback;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformVFXSettings constinit property declarations *******************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformVFXSettings Property Definitions ****************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_MaxActiveInstances = { "MaxActiveInstances", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXSettings, MaxActiveInstances), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxActiveInstances_MetaData), NewProp_MaxActiveInstances_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_MaxAmbientInstances = { "MaxAmbientInstances", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformVFXSettings, MaxAmbientInstances), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxAmbientInstances_MetaData), NewProp_MaxAmbientInstances_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bEnableDiagnostics = { "bEnableDiagnostics", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UGamePlatformVFXSettings), &UHT_STATICS::NewProp_bEnableDiagnostics_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bEnableDiagnostics_MetaData), NewProp_bEnableDiagnostics_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bAllowPlatformFallback = { "bAllowPlatformFallback", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UGamePlatformVFXSettings), &UHT_STATICS::NewProp_bAllowPlatformFallback_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAllowPlatformFallback_MetaData), NewProp_bAllowPlatformFallback_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MaxActiveInstances,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MaxAmbientInstances,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bEnableDiagnostics,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bAllowPlatformFallback,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformVFXSettings Property Definitions ******************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UDeveloperSettings,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXSettings,
	"Game",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001000A6u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXSettings;
UClass* Z_Construct_UClass_UGamePlatformVFXSettings(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXSettings;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXSettings.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXSettings"),
				Z_Registration_Info_UClass_UGamePlatformVFXSettings.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXSettings.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXSettings.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXSettings.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformVFXSettings::UGamePlatformVFXSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXSettings);
UGamePlatformVFXSettings::~UGamePlatformVFXSettings() {}
// ********** End Class UGamePlatformVFXSettings ***************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXSettings, TEXT("UGamePlatformVFXSettings"), &Z_Registration_Info_UClass_UGamePlatformVFXSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXSettings), 57273939U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h__Script_GamePlatformVFXClient_a60a96a8107cc52df3c27ce41230458e99f8afcb{
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
