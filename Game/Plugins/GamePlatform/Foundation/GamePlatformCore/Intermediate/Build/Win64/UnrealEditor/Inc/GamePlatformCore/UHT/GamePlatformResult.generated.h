// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Types/GamePlatformResult.h"

#ifdef GAMEPLATFORMCORE_GamePlatformResult_generated_h
#error "GamePlatformResult.generated.h already included, missing '#pragma once' in GamePlatformResult.h"
#endif
#define GAMEPLATFORMCORE_GamePlatformResult_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin ScriptStruct FGamePlatformResult ***********************************************
struct Z_Construct_UScriptStruct_FGamePlatformResult_Statics;
GAMEPLATFORMCORE_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformResult(ETypeConstructPhase);

#define FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformResult_h_32_GENERATED_BODY \
	friend struct ::Z_Construct_UScriptStruct_FGamePlatformResult_Statics; \
	UE_NODEBUG static UScriptStruct* StaticStruct() { return Z_Construct_UScriptStruct_FGamePlatformResult(ETypeConstructPhase::Inner); }


struct FGamePlatformResult;
// ********** End ScriptStruct FGamePlatformResult *************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GamePlatform_Foundation_GamePlatformCore_Source_GamePlatformCore_Public_Types_GamePlatformResult_h

// ********** Begin Enum EGamePlatformResultStatus *************************************************
#define FOREACH_ENUM_EGAMEPLATFORMRESULTSTATUS(op) \
	op(EGamePlatformResultStatus::NotExecuted) \
	op(EGamePlatformResultStatus::Succeeded) \
	op(EGamePlatformResultStatus::Failed) \
	op(EGamePlatformResultStatus::Cancelled) \
	op(EGamePlatformResultStatus::Unsupported) 

enum class EGamePlatformResultStatus : uint8;
template<> struct TIsUEnumClass<EGamePlatformResultStatus> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMCORE_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformResultStatus>();
// ********** End Enum EGamePlatformResultStatus ***************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
