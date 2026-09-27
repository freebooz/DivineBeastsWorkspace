// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Settings/GamePlatformVFXSettings.h"

#ifdef GAMEPLATFORMVFXCLIENT_GamePlatformVFXSettings_generated_h
#error "GamePlatformVFXSettings.generated.h already included, missing '#pragma once' in GamePlatformVFXSettings.h"
#endif
#define GAMEPLATFORMVFXCLIENT_GamePlatformVFXSettings_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UGamePlatformVFXSettings *************************************************
struct Z_Construct_UClass_UGamePlatformVFXSettings_Statics;
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXSettings(ETypeConstructPhase);

#define FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_10_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_UGamePlatformVFXSettings_Statics; \
	friend GAMEPLATFORMVFXCLIENT_API UClass* ::Z_Construct_UClass_UGamePlatformVFXSettings(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(UGamePlatformVFXSettings, UDeveloperSettings, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config), CASTCLASS_None, TEXT("/Script/GamePlatformVFXClient"), Z_Construct_UClass_UGamePlatformVFXSettings) \
	DECLARE_SERIALIZER(UGamePlatformVFXSettings) \
	static constexpr const TCHAR* StaticConfigName() {return TEXT("Game");} \



#define FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_10_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGamePlatformVFXSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UGamePlatformVFXSettings(UGamePlatformVFXSettings&&) = delete; \
	UGamePlatformVFXSettings(const UGamePlatformVFXSettings&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGamePlatformVFXSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGamePlatformVFXSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGamePlatformVFXSettings) \
	NO_API virtual ~UGamePlatformVFXSettings();


#define FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_7_PROLOG
#define FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_10_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_10_INCLASS_NO_PURE_DECLS \
	FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h_10_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UGamePlatformVFXSettings;

// ********** End Class UGamePlatformVFXSettings ***************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Settings_GamePlatformVFXSettings_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
