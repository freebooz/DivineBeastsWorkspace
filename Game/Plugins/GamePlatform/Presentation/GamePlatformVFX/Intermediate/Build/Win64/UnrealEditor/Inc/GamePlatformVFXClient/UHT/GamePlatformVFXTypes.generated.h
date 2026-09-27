// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Types/GamePlatformVFXTypes.h"

#ifdef GAMEPLATFORMVFXCLIENT_GamePlatformVFXTypes_generated_h
#error "GamePlatformVFXTypes.generated.h already included, missing '#pragma once' in GamePlatformVFXTypes.h"
#endif
#define GAMEPLATFORMVFXCLIENT_GamePlatformVFXTypes_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/IsUEnumClass.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXTypes_h

// ********** Begin Enum EGamePlatformVFXBehavior **************************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXBEHAVIOR(op) \
	op(EGamePlatformVFXBehavior::Instant) \
	op(EGamePlatformVFXBehavior::Attached) \
	op(EGamePlatformVFXBehavior::Projectile) \
	op(EGamePlatformVFXBehavior::Beam) \
	op(EGamePlatformVFXBehavior::Area) \
	op(EGamePlatformVFXBehavior::Shield) \
	op(EGamePlatformVFXBehavior::Portal) \
	op(EGamePlatformVFXBehavior::Trail) \
	op(EGamePlatformVFXBehavior::World) \
	op(EGamePlatformVFXBehavior::Composite) 

enum class EGamePlatformVFXBehavior : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXBehavior> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXBehavior>();
// ********** End Enum EGamePlatformVFXBehavior ****************************************************

// ********** Begin Enum EGamePlatformVFXCatalogScope **********************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXCATALOGSCOPE(op) \
	op(EGamePlatformVFXCatalogScope::Platform) \
	op(EGamePlatformVFXCatalogScope::Moba) \
	op(EGamePlatformVFXCatalogScope::Project) \
	op(EGamePlatformVFXCatalogScope::ContentPack) 

enum class EGamePlatformVFXCatalogScope : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXCatalogScope> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXCatalogScope>();
// ********** End Enum EGamePlatformVFXCatalogScope ************************************************

// ********** Begin Enum EGamePlatformVFXLifecycleState ********************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXLIFECYCLESTATE(op) \
	op(EGamePlatformVFXLifecycleState::Invalid) \
	op(EGamePlatformVFXLifecycleState::Requested) \
	op(EGamePlatformVFXLifecycleState::Loading) \
	op(EGamePlatformVFXLifecycleState::Spawning) \
	op(EGamePlatformVFXLifecycleState::Active) \
	op(EGamePlatformVFXLifecycleState::Stopping) \
	op(EGamePlatformVFXLifecycleState::Completed) \
	op(EGamePlatformVFXLifecycleState::Cancelled) \
	op(EGamePlatformVFXLifecycleState::Failed) 

enum class EGamePlatformVFXLifecycleState : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXLifecycleState> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXLifecycleState>();
// ********** End Enum EGamePlatformVFXLifecycleState **********************************************

// ********** Begin Enum EGamePlatformVFXPlayResultCode ********************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXPLAYRESULTCODE(op) \
	op(EGamePlatformVFXPlayResultCode::Accepted) \
	op(EGamePlatformVFXPlayResultCode::InvalidRequest) \
	op(EGamePlatformVFXPlayResultCode::ResolveFailed) \
	op(EGamePlatformVFXPlayResultCode::WorldUnavailable) \
	op(EGamePlatformVFXPlayResultCode::BudgetRejected) 

enum class EGamePlatformVFXPlayResultCode : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXPlayResultCode> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXPlayResultCode>();
// ********** End Enum EGamePlatformVFXPlayResultCode **********************************************

// ********** Begin Enum EGamePlatformVFXImportance ************************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXIMPORTANCE(op) \
	op(EGamePlatformVFXImportance::Critical) \
	op(EGamePlatformVFXImportance::High) \
	op(EGamePlatformVFXImportance::Normal) \
	op(EGamePlatformVFXImportance::Ambient) 

enum class EGamePlatformVFXImportance : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXImportance> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXImportance>();
// ********** End Enum EGamePlatformVFXImportance **************************************************

// ********** Begin Enum EGamePlatformVFXPoolingMode ***********************************************
#define FOREACH_ENUM_EGAMEPLATFORMVFXPOOLINGMODE(op) \
	op(EGamePlatformVFXPoolingMode::None) \
	op(EGamePlatformVFXPoolingMode::AutoRelease) \
	op(EGamePlatformVFXPoolingMode::ManualRelease) 

enum class EGamePlatformVFXPoolingMode : uint8;
template<> struct TIsUEnumClass<EGamePlatformVFXPoolingMode> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXPoolingMode>();
// ********** End Enum EGamePlatformVFXPoolingMode *************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
