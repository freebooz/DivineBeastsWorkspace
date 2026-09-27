// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Types/GamePlatformDataLease.h"

#ifdef GAMEPLATFORMDATA_GamePlatformDataLease_generated_h
#error "GamePlatformDataLease.generated.h already included, missing '#pragma once' in GamePlatformDataLease.h"
#endif
#define GAMEPLATFORMDATA_GamePlatformDataLease_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin ScriptStruct FGamePlatformDataLease ********************************************
struct Z_Construct_UScriptStruct_FGamePlatformDataLease_Statics;
GAMEPLATFORMDATA_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformDataLease(ETypeConstructPhase);

#define FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataLease_h_38_GENERATED_BODY \
	friend struct ::Z_Construct_UScriptStruct_FGamePlatformDataLease_Statics; \
	UE_NODEBUG static UScriptStruct* StaticStruct() { return Z_Construct_UScriptStruct_FGamePlatformDataLease(ETypeConstructPhase::Inner); }


struct FGamePlatformDataLease;
// ********** End ScriptStruct FGamePlatformDataLease **********************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GamePlatform_Foundation_GamePlatformData_Source_GamePlatformData_Public_Types_GamePlatformDataLease_h

// ********** Begin Enum EGamePlatformDataLifetime *************************************************
#define FOREACH_ENUM_EGAMEPLATFORMDATALIFETIME(op) \
	op(EGamePlatformDataLifetime::Instance) \
	op(EGamePlatformDataLifetime::World) 

enum class EGamePlatformDataLifetime : uint8;
template<> struct TIsUEnumClass<EGamePlatformDataLifetime> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMDATA_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformDataLifetime>();
// ********** End Enum EGamePlatformDataLifetime ***************************************************

// ********** Begin Enum EGamePlatformDataRequestState *********************************************
#define FOREACH_ENUM_EGAMEPLATFORMDATAREQUESTSTATE(op) \
	op(EGamePlatformDataRequestState::Invalid) \
	op(EGamePlatformDataRequestState::Loading) \
	op(EGamePlatformDataRequestState::Succeeded) \
	op(EGamePlatformDataRequestState::Failed) \
	op(EGamePlatformDataRequestState::Cancelled) \
	op(EGamePlatformDataRequestState::Released) 

enum class EGamePlatformDataRequestState : uint8;
template<> struct TIsUEnumClass<EGamePlatformDataRequestState> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMDATA_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformDataRequestState>();
// ********** End Enum EGamePlatformDataRequestState ***********************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
