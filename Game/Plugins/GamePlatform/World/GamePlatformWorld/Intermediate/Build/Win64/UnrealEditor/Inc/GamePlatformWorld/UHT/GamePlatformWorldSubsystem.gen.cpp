// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Subsystems/GamePlatformWorldSubsystem.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformWorldSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UWorldSubsystem(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformWorld(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformWorldSubsystem(ETypeConstructPhase);
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformWorldSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformWorldSubsystem **********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformWorldSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe7\xa7\x81\xe6\x9c\x89\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xef\xbc\x9a\xe4\xbb\x85Game/PIE\xe9\x9d\x9e""Commandlet\xe5\x88\x9b\xe5\xbb\xba\xef\xbc\x8c\xe5\xae\x9e\xe4\xbe\x8b\xe9\x97\xb4\xe6\x97\xa0\xe5\x85\xb1\xe4\xba\xab\xe5\x8f\xaf\xe5\x8f\x98\xe7\x99\xbb\xe8\xae\xb0\xe8\xa1\xa8\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Subsystems/GamePlatformWorldSubsystem.h" },
		{ "ModuleRelativePath", "Private/Subsystems/GamePlatformWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe7\xa7\x81\xe6\x9c\x89\xe4\xb8\x96\xe7\x95\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xef\xbc\x9a\xe4\xbb\x85Game/PIE\xe9\x9d\x9e""Commandlet\xe5\x88\x9b\xe5\xbb\xba\xef\xbc\x8c\xe5\xae\x9e\xe4\xbe\x8b\xe9\x97\xb4\xe6\x97\xa0\xe5\x85\xb1\xe4\xba\xab\xe5\x8f\xaf\xe5\x8f\x98\xe7\x99\xbb\xe8\xae\xb0\xe8\xa1\xa8\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformWorldSubsystem constinit property declarations **************
// ********** End Class UGamePlatformWorldSubsystem constinit property declarations ****************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformWorldSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UWorldSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformWorld,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformWorldSubsystem,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformWorldSubsystem;
UClass* Z_Construct_UClass_UGamePlatformWorldSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformWorldSubsystem;
		if (!Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformWorldSubsystem"),
				Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformWorldSubsystem.OuterSingleton;
}
#undef UHT_STATICS
// ********** End Class UGamePlatformWorldSubsystem ************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Private_Subsystems_GamePlatformWorldSubsystem_h__Script_GamePlatformWorld_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformWorldSubsystem, TEXT("UGamePlatformWorldSubsystem"), &Z_Registration_Info_UClass_UGamePlatformWorldSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformWorldSubsystem), 1985465518U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Private_Subsystems_GamePlatformWorldSubsystem_h__Script_GamePlatformWorld_a1cc142e91a1eddac610de0926b1277791383023{
	TEXT("/Script/GamePlatformWorld"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
