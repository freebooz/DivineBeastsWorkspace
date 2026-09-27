// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Subsystems/GamePlatformLoadingSubsystem.h"
#include "Engine/GameInstance.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformLoadingSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UGameInstanceSubsystem(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformLoading(ETypeConstructPhase);
GAMEPLATFORMLOADING_API UClass* Z_Construct_UClass_UGamePlatformLoadingSubsystem(ETypeConstructPhase);
GAMEPLATFORMLOADING_API UClass* Z_Construct_UClass_UGamePlatformLoadingSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformLoadingSubsystem ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformLoadingSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x86\x85\xe9\x83\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe6\x89\xa7\xe8\xa1\x8c\xe5\x99\xa8\xef\xbc\x9b\xe5\x8f\xaa\xe5\x85\xac\xe5\xbc\x80\xe6\x9c\x8d\xe5\x8a\xa1\xe9\x97\xa8\xe9\x9d\xa2\xef\xbc\x8c\xe4\xb8\x8d\xe5\x85\xac\xe5\xbc\x80\xe6\xb3\xa8\xe5\x86\x8c\xe8\xa1\xa8\xe3\x80\x81\xe8\xb0\x83\xe5\xba\xa6\xe5\x99\xa8\xe6\x88\x96\xe7\xa7\x9f\xe7\xba\xa6\xe9\x9b\x86\xe5\x90\x88\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Subsystems/GamePlatformLoadingSubsystem.h" },
		{ "ModuleRelativePath", "Private/Subsystems/GamePlatformLoadingSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x86\x85\xe9\x83\xa8\xe5\xae\x9e\xe4\xbe\x8b\xe6\x89\xa7\xe8\xa1\x8c\xe5\x99\xa8\xef\xbc\x9b\xe5\x8f\xaa\xe5\x85\xac\xe5\xbc\x80\xe6\x9c\x8d\xe5\x8a\xa1\xe9\x97\xa8\xe9\x9d\xa2\xef\xbc\x8c\xe4\xb8\x8d\xe5\x85\xac\xe5\xbc\x80\xe6\xb3\xa8\xe5\x86\x8c\xe8\xa1\xa8\xe3\x80\x81\xe8\xb0\x83\xe5\xba\xa6\xe5\x99\xa8\xe6\x88\x96\xe7\xa7\x9f\xe7\xba\xa6\xe9\x9b\x86\xe5\x90\x88\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformLoadingSubsystem constinit property declarations ************
// ********** End Class UGamePlatformLoadingSubsystem constinit property declarations **************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformLoadingSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGameInstanceSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformLoading,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformLoadingSubsystem,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem;
UClass* Z_Construct_UClass_UGamePlatformLoadingSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformLoadingSubsystem;
		if (!Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformLoadingSubsystem"),
				Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem.OuterSingleton;
}
#undef UHT_STATICS
// ********** End Class UGamePlatformLoadingSubsystem **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Application_GamePlatformLoading_Source_GamePlatformLoading_Private_Subsystems_GamePlatformLoadingSubsystem_h__Script_GamePlatformLoading_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformLoadingSubsystem, TEXT("UGamePlatformLoadingSubsystem"), &Z_Registration_Info_UClass_UGamePlatformLoadingSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformLoadingSubsystem), 3031785576U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Application_GamePlatformLoading_Source_GamePlatformLoading_Private_Subsystems_GamePlatformLoadingSubsystem_h__Script_GamePlatformLoading_350d6e9f35dfc1cbe89835cd34457eaf5c980962{
	TEXT("/Script/GamePlatformLoading"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
