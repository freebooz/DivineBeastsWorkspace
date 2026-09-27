// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGamePlatformLoading_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_GamePlatformLoading;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_GamePlatformLoading(ETypeConstructPhase)
	{
		if (!Z_Registration_Info_UPackage__Script_GamePlatformLoading.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/GamePlatformLoading",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0xA1ADF207,
			0x812EB132,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_GamePlatformLoading.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_GamePlatformLoading.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_GamePlatformLoading(Z_Construct_UPackage__Script_GamePlatformLoading, TEXT("/Script/GamePlatformLoading"), Z_Registration_Info_UPackage__Script_GamePlatformLoading, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xA1ADF207, 0x812EB132));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
