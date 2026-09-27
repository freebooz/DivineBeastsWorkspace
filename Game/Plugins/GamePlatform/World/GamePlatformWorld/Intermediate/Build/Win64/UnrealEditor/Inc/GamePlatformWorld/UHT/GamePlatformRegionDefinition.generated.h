// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Definitions/GamePlatformRegionDefinition.h"

#ifdef GAMEPLATFORMWORLD_GamePlatformRegionDefinition_generated_h
#error "GamePlatformRegionDefinition.generated.h already included, missing '#pragma once' in GamePlatformRegionDefinition.h"
#endif
#define GAMEPLATFORMWORLD_GamePlatformRegionDefinition_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UGamePlatformRegionDefinition ********************************************
struct Z_Construct_UClass_UGamePlatformRegionDefinition_Statics;
GAMEPLATFORMWORLD_API UClass* Z_Construct_UClass_UGamePlatformRegionDefinition(ETypeConstructPhase);

#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_29_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_UGamePlatformRegionDefinition_Statics; \
	friend GAMEPLATFORMWORLD_API UClass* ::Z_Construct_UClass_UGamePlatformRegionDefinition(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(UGamePlatformRegionDefinition, UGamePlatformDefinitionBase, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/GamePlatformWorld"), Z_Construct_UClass_UGamePlatformRegionDefinition) \
	DECLARE_SERIALIZER(UGamePlatformRegionDefinition)


#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_29_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGamePlatformRegionDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UGamePlatformRegionDefinition(UGamePlatformRegionDefinition&&) = delete; \
	UGamePlatformRegionDefinition(const UGamePlatformRegionDefinition&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGamePlatformRegionDefinition); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGamePlatformRegionDefinition); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGamePlatformRegionDefinition) \
	NO_API virtual ~UGamePlatformRegionDefinition();


#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_26_PROLOG
#define FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_29_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_29_INCLASS_NO_PURE_DECLS \
	FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h_29_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UGamePlatformRegionDefinition;

// ********** End Class UGamePlatformRegionDefinition **********************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GamePlatform_World_GamePlatformWorld_Source_GamePlatformWorld_Public_Definitions_GamePlatformRegionDefinition_h

// ********** Begin Enum EGamePlatformRegionBoundsPolicy *******************************************
#define FOREACH_ENUM_EGAMEPLATFORMREGIONBOUNDSPOLICY(op) \
	op(EGamePlatformRegionBoundsPolicy::AxisAlignedBox) 

enum class EGamePlatformRegionBoundsPolicy : uint8;
template<> struct TIsUEnumClass<EGamePlatformRegionBoundsPolicy> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMWORLD_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformRegionBoundsPolicy>();
// ********** End Enum EGamePlatformRegionBoundsPolicy *********************************************

// ********** Begin Enum EGamePlatformRegionActivationPolicy ***************************************
#define FOREACH_ENUM_EGAMEPLATFORMREGIONACTIVATIONPOLICY(op) \
	op(EGamePlatformRegionActivationPolicy::AlwaysRegistered) 

enum class EGamePlatformRegionActivationPolicy : uint8;
template<> struct TIsUEnumClass<EGamePlatformRegionActivationPolicy> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMWORLD_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformRegionActivationPolicy>();
// ********** End Enum EGamePlatformRegionActivationPolicy *****************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
