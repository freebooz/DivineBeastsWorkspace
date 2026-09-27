// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Definitions/GamePlatformWorldDefinition.h"

#ifdef GAMEPLATFORMWORLD_GamePlatformWorldDefinition_generated_h
#error "GamePlatformWorldDefinition.generated.h already included, missing '#pragma once' in GamePlatformWorldDefinition.h"
#endif
#define GAMEPLATFORMWORLD_GamePlatformWorldDefinition_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UGamePlatformWorldDefinition *********************************************
struct Z_Construct_UClass_UGamePlatformWorldDefinition_Statics;
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformWorldDefinition(ETypeConstructPhase);

#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_15_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_UGamePlatformWorldDefinition_Statics; \
	friend GAMEPLATFORMWORLD_API UClass* ::Z_Construct_UClass_UGamePlatformWorldDefinition(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(UGamePlatformWorldDefinition, UGamePlatformDefinitionBase, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/GamePlatformWorld"), Z_Construct_UClass_UGamePlatformWorldDefinition) \
	DECLARE_SERIALIZER(UGamePlatformWorldDefinition)


#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_15_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGamePlatformWorldDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UGamePlatformWorldDefinition(UGamePlatformWorldDefinition&&) = delete; \
	UGamePlatformWorldDefinition(const UGamePlatformWorldDefinition&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGamePlatformWorldDefinition); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGamePlatformWorldDefinition); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGamePlatformWorldDefinition) \
	NO_API virtual ~UGamePlatformWorldDefinition();


#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_12_PROLOG
#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_15_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_15_INCLASS_NO_PURE_DECLS \
	FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h_15_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UGamePlatformWorldDefinition;

// ********** End Class UGamePlatformWorldDefinition ***********************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformWorldDefinition_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
