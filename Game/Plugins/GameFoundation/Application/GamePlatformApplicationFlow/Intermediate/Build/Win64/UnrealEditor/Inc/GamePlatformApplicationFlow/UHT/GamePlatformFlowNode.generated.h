// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Interfaces/GamePlatformFlowNode.h"

#ifdef GAMEPLATFORMAPPLICATIONFLOW_GamePlatformFlowNode_generated_h
#error "GamePlatformFlowNode.generated.h already included, missing '#pragma once' in GamePlatformFlowNode.h"
#endif
#define GAMEPLATFORMAPPLICATIONFLOW_GamePlatformFlowNode_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UGamePlatformFlowNode ****************************************************
struct Z_Construct_UClass_UGamePlatformFlowNode_Statics;
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase);

#define FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_16_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_UGamePlatformFlowNode_Statics; \
	friend GAMEPLATFORMAPPLICATIONFLOW_API UClass* ::Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(UGamePlatformFlowNode, UObject, COMPILED_IN_FLAGS(CLASS_Abstract | CLASS_Transient), CASTCLASS_None, TEXT("/Script/GamePlatformApplicationFlow"), Z_Construct_UClass_UGamePlatformFlowNode) \
	DECLARE_SERIALIZER(UGamePlatformFlowNode)


#define FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_16_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGamePlatformFlowNode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UGamePlatformFlowNode(UGamePlatformFlowNode&&) = delete; \
	UGamePlatformFlowNode(const UGamePlatformFlowNode&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGamePlatformFlowNode); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGamePlatformFlowNode); \
	DEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGamePlatformFlowNode) \
	NO_API virtual ~UGamePlatformFlowNode();


#define FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_13_PROLOG
#define FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_16_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_16_INCLASS_NO_PURE_DECLS \
	FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h_16_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UGamePlatformFlowNode;

// ********** End Class UGamePlatformFlowNode ******************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
