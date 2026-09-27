// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeGamePlatformCore_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_GamePlatformCore;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_GamePlatformCore(ETypeConstructPhase)
	{
		if (!Z_Registration_Info_UPackage__Script_GamePlatformCore.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/GamePlatformCore",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0xDA69AAEF,
			0x1E678DAF,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_GamePlatformCore.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_GamePlatformCore.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_GamePlatformCore(Z_Construct_UPackage__Script_GamePlatformCore, TEXT("/Script/GamePlatformCore"), Z_Registration_Info_UPackage__Script_GamePlatformCore, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xDA69AAEF, 0x1E678DAF));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
