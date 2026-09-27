// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Subsystems/GamePlatformDataSubsystem.h"
#include "Engine/GameInstance.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformDataSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UGameInstanceSubsystem(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformData(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDataSubsystem(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDataSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformDataSubsystem ***********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformDataSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xae\x9e\xe4\xbe\x8b\xe7\xa7\x81\xe6\x9c\x89\xe9\x97\xa8\xe9\x9d\xa2\xef\xbc\x9a\xe6\x8b\xa5\xe6\x9c\x89\xe8\xaf\xb7\xe6\xb1\x82\xe5\x9b\xbe\xe4\xb8\x8e\xe5\xbc\xb1\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xef\xbc\x8c\xe4\xb8\x8d\xe6\x8b\xa5\xe6\x9c\x89\xe5\x85\xb6\xe4\xbb\x96\xe5\xae\x9e\xe4\xbe\x8b\xe6\x88\x96\xe5\xa4\x96\xe9\x83\xa8\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Subsystems/GamePlatformDataSubsystem.h" },
		{ "ModuleRelativePath", "Private/Subsystems/GamePlatformDataSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xae\x9e\xe4\xbe\x8b\xe7\xa7\x81\xe6\x9c\x89\xe9\x97\xa8\xe9\x9d\xa2\xef\xbc\x9a\xe6\x8b\xa5\xe6\x9c\x89\xe8\xaf\xb7\xe6\xb1\x82\xe5\x9b\xbe\xe4\xb8\x8e\xe5\xbc\xb1\xe8\xb0\x83\xe7\x94\xa8\xe8\x80\x85\xef\xbc\x8c\xe4\xb8\x8d\xe6\x8b\xa5\xe6\x9c\x89\xe5\x85\xb6\xe4\xbb\x96\xe5\xae\x9e\xe4\xbe\x8b\xe6\x88\x96\xe5\xa4\x96\xe9\x83\xa8\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformDataSubsystem constinit property declarations ***************
// ********** End Class UGamePlatformDataSubsystem constinit property declarations *****************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformDataSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGameInstanceSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformData,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformDataSubsystem,
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
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformDataSubsystem;
UClass* Z_Construct_UClass_UGamePlatformDataSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformDataSubsystem;
		if (!Z_Registration_Info_UClass_UGamePlatformDataSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformDataSubsystem"),
				Z_Registration_Info_UClass_UGamePlatformDataSubsystem.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformDataSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformDataSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformDataSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformDataSubsystem.OuterSingleton;
}
#undef UHT_STATICS
// ********** End Class UGamePlatformDataSubsystem *************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Private_Subsystems_GamePlatformDataSubsystem_h__Script_GamePlatformData_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformDataSubsystem, TEXT("UGamePlatformDataSubsystem"), &Z_Registration_Info_UClass_UGamePlatformDataSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformDataSubsystem), 3899489935U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Private_Subsystems_GamePlatformDataSubsystem_h__Script_GamePlatformData_f4a0d72e4abe1965b45596d3a9866b5aeea8a11c{
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
