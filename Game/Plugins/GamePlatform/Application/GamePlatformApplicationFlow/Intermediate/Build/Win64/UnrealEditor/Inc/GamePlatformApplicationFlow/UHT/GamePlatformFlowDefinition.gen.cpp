// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "UObject/PrimaryAssetId.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformFlowDefinition() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FPrimaryAssetId(ETypeConstructPhase);
GAMEPLATFORMDATA_API UClass* Z_Construct_UClass_UGamePlatformDefinitionBase(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformApplicationFlow(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowDefinition(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowDefinition(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformFlowNodeDefinition ***********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformFlowNodeDefinition>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformFlowNodeDefinition); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaa\xe6\x8f\x8f\xe8\xbf\xb0\xe4\xb8\xad\xe7\xab\x8b\xe8\x8a\x82\xe7\x82\xb9\xe4\xb8\x8e\xe8\xb7\xaf\xe7\x94\xb1\xef\xbc\x8c\xe4\xb8\x8d\xe5\xad\x98\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x81\xe4\xb8\x96\xe7\x95\x8c\xe3\x80\x81\xe7\xbd\x91\xe7\xbb\x9c\xe5\x9c\xb0\xe5\x9d\x80\xe6\x88\x96\xe9\xa1\xb9\xe7\x9b\xae\xe4\xb8\x9a\xe5\x8a\xa1\xe4\xbb\xa3\xe7\xa0\x81\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaa\xe6\x8f\x8f\xe8\xbf\xb0\xe4\xb8\xad\xe7\xab\x8b\xe8\x8a\x82\xe7\x82\xb9\xe4\xb8\x8e\xe8\xb7\xaf\xe7\x94\xb1\xef\xbc\x8c\xe4\xb8\x8d\xe5\xad\x98\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x81\xe4\xb8\x96\xe7\x95\x8c\xe3\x80\x81\xe7\xbd\x91\xe7\xbb\x9c\xe5\x9c\xb0\xe5\x9d\x80\xe6\x88\x96\xe9\xa1\xb9\xe7\x9b\xae\xe4\xb8\x9a\xe5\x8a\xa1\xe4\xbb\xa3\xe7\xa0\x81\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NodeId_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x9b\xbe\xe5\x86\x85\xe5\x94\xaf\xe4\xb8\x80\xe8\x8a\x82\xe7\x82\xb9\xe5\x90\x8d\xe7\xa7\xb0\xef\xbc\x9b\xe4\xb8\x8d\xe5\x8f\xaf\xe4\xb8\xba\xe7\xa9\xba\xef\xbc\x8c""FName\xe5\xa4\xa7\xe5\xb0\x8f\xe5\x86\x99\xe7\xad\x89\xe4\xbb\xb7\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x9b\xbe\xe5\x86\x85\xe5\x94\xaf\xe4\xb8\x80\xe8\x8a\x82\xe7\x82\xb9\xe5\x90\x8d\xe7\xa7\xb0\xef\xbc\x9b\xe4\xb8\x8d\xe5\x8f\xaf\xe4\xb8\xba\xe7\xa9\xba\xef\xbc\x8c""FName\xe5\xa4\xa7\xe5\xb0\x8f\xe5\x86\x99\xe7\xad\x89\xe4\xbb\xb7\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExecutorId_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe7\x94\xb1\xe5\xbd\x93\xe5\x89\x8dGameInstance\xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe6\x98\xbe\xe5\xbc\x8f\xe6\xb3\xa8\xe5\x86\x8c\xe7\x9a\x84\xe6\x89\xa7\xe8\xa1\x8c\xe5\x99\xa8\xe9\x94\xae\xef\xbc\x8c\xe4\xb8\x8d\xe9\x80\x9a\xe8\xbf\x87\xe5\xad\x97\xe7\xac\xa6\xe4\xb8\xb2\xe5\x8f\x8d\xe5\xb0\x84\xe5\x88\x9b\xe5\xbb\xba\xe4\xbb\xbb\xe6\x84\x8f\xe7\xb1\xbb\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe7\x94\xb1\xe5\xbd\x93\xe5\x89\x8dGameInstance\xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe6\x98\xbe\xe5\xbc\x8f\xe6\xb3\xa8\xe5\x86\x8c\xe7\x9a\x84\xe6\x89\xa7\xe8\xa1\x8c\xe5\x99\xa8\xe9\x94\xae\xef\xbc\x8c\xe4\xb8\x8d\xe9\x80\x9a\xe8\xbf\x87\xe5\xad\x97\xe7\xac\xa6\xe4\xb8\xb2\xe5\x8f\x8d\xe5\xb0\x84\xe5\x88\x9b\xe5\xbb\xba\xe4\xbb\xbb\xe6\x84\x8f\xe7\xb1\xbb\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InputDefinitionId_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaf\xe9\x80\x89\xe8\xbe\x93\xe5\x85\xa5\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xbb\x85\xe4\xbc\xa0\xe7\xbb\x99""Context\xef\xbc\x8c\xe5\xae\x9e\xe9\x99\x85\xe8\xaf\xbb\xe5\x8f\x96\xe4\xb8\x8e\xe7\xa7\x9f\xe7\xba\xa6\xe7\x94\xb1\xe8\x8a\x82\xe7\x82\xb9\xe6\x89\x80\xe5\xb1\x9e\xe9\xa2\x86\xe5\x9f\x9f\xe8\xb4\x9f\xe8\xb4\xa3\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaf\xe9\x80\x89\xe8\xbe\x93\xe5\x85\xa5\xe4\xb8\xbb\xe8\xb5\x84\xe4\xba\xa7\xe8\xba\xab\xe4\xbb\xbd\xef\xbc\x9b\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xbb\x85\xe4\xbc\xa0\xe7\xbb\x99""Context\xef\xbc\x8c\xe5\xae\x9e\xe9\x99\x85\xe8\xaf\xbb\xe5\x8f\x96\xe4\xb8\x8e\xe7\xa7\x9f\xe7\xba\xa6\xe7\x94\xb1\xe8\x8a\x82\xe7\x82\xb9\xe6\x89\x80\xe5\xb1\x9e\xe9\xa2\x86\xe5\x9f\x9f\xe8\xb4\x9f\xe8\xb4\xa3\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TimeoutSeconds_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8d\x95\xe6\xac\xa1\xe6\x89\xa7\xe8\xa1\x8c\xe7\x9c\x9f\xe5\xae\x9e\xe7\xa7\x92\xe6\x95\xb0\xef\xbc\x8c\xe5\xbf\x85\xe9\xa1\xbb\xe6\x9c\x89\xe9\x99\x90\xe4\xb8\x94\xe5\xa4\xa7\xe4\xba\x8e\xe9\x9b\xb6\xef\xbc\x9b\xe8\xb5\x84\xe4\xba\xa7\xe6\xa8\xa1\xe5\xbc\x8f\xe4\xb8\x8d\xe9\x9a\x90\xe5\xbc\x8f\xe9\x87\x8d\xe8\xaf\x95\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8d\x95\xe6\xac\xa1\xe6\x89\xa7\xe8\xa1\x8c\xe7\x9c\x9f\xe5\xae\x9e\xe7\xa7\x92\xe6\x95\xb0\xef\xbc\x8c\xe5\xbf\x85\xe9\xa1\xbb\xe6\x9c\x89\xe9\x99\x90\xe4\xb8\x94\xe5\xa4\xa7\xe4\xba\x8e\xe9\x9b\xb6\xef\xbc\x9b\xe8\xb5\x84\xe4\xba\xa7\xe6\xa8\xa1\xe5\xbc\x8f\xe4\xb8\x8d\xe9\x9a\x90\xe5\xbc\x8f\xe9\x87\x8d\xe8\xaf\x95\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NextNodeId_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe9\xbb\x98\xe8\xae\xa4\xe6\x88\x90\xe5\x8a\x9f\xe8\xbe\xb9\xef\xbc\x8cNone\xe8\xa1\xa8\xe7\xa4\xba\xe6\x88\x90\xe5\x8a\x9f\xe7\xbb\x88\xe7\x82\xb9\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe9\xbb\x98\xe8\xae\xa4\xe6\x88\x90\xe5\x8a\x9f\xe8\xbe\xb9\xef\xbc\x8cNone\xe8\xa1\xa8\xe7\xa4\xba\xe6\x88\x90\xe5\x8a\x9f\xe7\xbb\x88\xe7\x82\xb9\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Routes_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x85\xb7\xe5\x90\x8d\xe6\x88\x90\xe5\x8a\x9f\xe4\xba\x8b\xe4\xbb\xb6\xe5\x88\xb0\xe5\x90\x8e\xe7\xbb\xa7\xe7\x9a\x84\xe6\x98\xa0\xe5\xb0\x84\xef\xbc\x9b\xe9\x94\xae\xe4\xb8\x8d\xe8\x83\xbd\xe4\xb8\xbaNone\xef\xbc\x8c\xe5\x80\xbc\xe4\xb8\xbaNone\xe8\xa1\xa8\xe7\xa4\xba\xe7\xbb\x88\xe7\x82\xb9\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x85\xb7\xe5\x90\x8d\xe6\x88\x90\xe5\x8a\x9f\xe4\xba\x8b\xe4\xbb\xb6\xe5\x88\xb0\xe5\x90\x8e\xe7\xbb\xa7\xe7\x9a\x84\xe6\x98\xa0\xe5\xb0\x84\xef\xbc\x9b\xe9\x94\xae\xe4\xb8\x8d\xe8\x83\xbd\xe4\xb8\xbaNone\xef\xbc\x8c\xe5\x80\xbc\xe4\xb8\xbaNone\xe8\xa1\xa8\xe7\xa4\xba\xe7\xbb\x88\xe7\x82\xb9\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformFlowNodeDefinition constinit property declarations ***
	static const UECodeGen_Private::FNamePropertyParams NewProp_NodeId;
	static const UECodeGen_Private::FNamePropertyParams NewProp_ExecutorId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_InputDefinitionId;
	static const UECodeGen_Private::FDoublePropertyParams NewProp_TimeoutSeconds;
	static const UECodeGen_Private::FNamePropertyParams NewProp_NextNodeId;
	static const UECodeGen_Private::FNamePropertyParams NewProp_Routes_ValueProp;
	static const UECodeGen_Private::FNamePropertyParams NewProp_Routes_Key_KeyProp;
	static const UECodeGen_Private::FMapPropertyParams NewProp_Routes;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformFlowNodeDefinition constinit property declarations *****
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformFlowNodeDefinition>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformFlowNodeDefinition Property Definitions **************
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_NodeId = { "NodeId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, NodeId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NodeId_MetaData), NewProp_NodeId_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_ExecutorId = { "ExecutorId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, ExecutorId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExecutorId_MetaData), NewProp_ExecutorId_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_InputDefinitionId = { "InputDefinitionId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, InputDefinitionId), Z_Construct_UScriptStruct_FPrimaryAssetId, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InputDefinitionId_MetaData), NewProp_InputDefinitionId_MetaData) }; // 51539104367397b403249c27cab9a0578cde1246
const UECodeGen_Private::FDoublePropertyParams UHT_STATICS::NewProp_TimeoutSeconds = { "TimeoutSeconds", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Double, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, TimeoutSeconds), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TimeoutSeconds_MetaData), NewProp_TimeoutSeconds_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_NextNodeId = { "NextNodeId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, NextNodeId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NextNodeId_MetaData), NewProp_NextNodeId_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_Routes_ValueProp = { "Routes", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 1, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_Routes_Key_KeyProp = { "Routes_Key", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FMapPropertyParams UHT_STATICS::NewProp_Routes = { "Routes", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Map, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformFlowNodeDefinition, Routes), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Routes_MetaData), NewProp_Routes_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NodeId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ExecutorId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_InputDefinitionId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_TimeoutSeconds,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NextNodeId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Routes_ValueProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Routes_Key_KeyProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Routes,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformFlowNodeDefinition Property Definitions ****************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow,
	nullptr,
	&NewStructOps,
	"GamePlatformFlowNodeDefinition",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformFlowNodeDefinition>(),
	alignof(FGamePlatformFlowNodeDefinition),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition, (UObject*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow(ETypeConstructPhase::Outer), TEXT("GamePlatformFlowNodeDefinition"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformFlowNodeDefinition *************************************

// ********** Begin Class UGamePlatformFlowDefinition **********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformFlowDefinition_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8f\xaa\xe8\xaf\xbb\xe6\xb5\x81\xe7\xa8\x8b\xe8\xb5\x84\xe4\xba\xa7\xef\xbc\x8c\xe7\xbb\x8f""Data\xe6\x88\x90\xe5\x8a\x9f\xe7\xa7\x9f\xe7\xba\xa6\xe6\xb6\x88\xe8\xb4\xb9\xef\xbc\x9b\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84\xe8\xba\xab\xe4\xbb\xbd\xe3\x80\x81\xe7\x89\x88\xe6\x9c\xac\xe5\x92\x8c\xe5\xbf\x85\xe9\x9c\x80\xe4\xbe\x9d\xe8\xb5\x96\xe4\xbb\x8d\xe7\x94\xb1""Data\xe8\xb4\x9f\xe8\xb4\xa3\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Definitions/GamePlatformFlowDefinition.h" },
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8f\xaa\xe8\xaf\xbb\xe6\xb5\x81\xe7\xa8\x8b\xe8\xb5\x84\xe4\xba\xa7\xef\xbc\x8c\xe7\xbb\x8f""Data\xe6\x88\x90\xe5\x8a\x9f\xe7\xa7\x9f\xe7\xba\xa6\xe6\xb6\x88\xe8\xb4\xb9\xef\xbc\x9b\xe7\xbb\xa7\xe6\x89\xbf\xe7\x9a\x84\xe8\xba\xab\xe4\xbb\xbd\xe3\x80\x81\xe7\x89\x88\xe6\x9c\xac\xe5\x92\x8c\xe5\xbf\x85\xe9\x9c\x80\xe4\xbe\x9d\xe8\xb5\x96\xe4\xbb\x8d\xe7\x94\xb1""Data\xe8\xb4\x9f\xe8\xb4\xa3\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_EntryNodeId_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xbf\x85\xe9\xa1\xbb\xe5\xaf\xb9\xe5\xba\x94Nodes\xe4\xb8\xad\xe7\x9a\x84\xe8\x8a\x82\xe7\x82\xb9\xef\xbc\x8c\xe6\x89\x80\xe6\x9c\x89\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe4\xbb\x8e\xe6\xad\xa4\xe5\x85\xa5\xe5\x8f\xa3\xe5\x8f\xaf\xe8\xbe\xbe\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xbf\x85\xe9\xa1\xbb\xe5\xaf\xb9\xe5\xba\x94Nodes\xe4\xb8\xad\xe7\x9a\x84\xe8\x8a\x82\xe7\x82\xb9\xef\xbc\x8c\xe6\x89\x80\xe6\x9c\x89\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe4\xbb\x8e\xe6\xad\xa4\xe5\x85\xa5\xe5\x8f\xa3\xe5\x8f\xaf\xe8\xbe\xbe\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Nodes_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\x8e\x9f\xe5\xad\x90\xe6\xa0\xa1\xe9\xaa\x8c\xe7\x9a\x84\xe5\xae\x8c\xe6\x95\xb4\xe8\x8a\x82\xe7\x82\xb9\xe9\x9b\x86\xe5\x90\x88\xef\xbc\x9b\xe5\x90\xaf\xe5\x8a\xa8\xe5\x89\x8d\xe9\xaa\x8c\xe8\xaf\x81\xe6\x89\x80\xe6\x9c\x89\xe8\xb7\xaf\xe7\x94\xb1\xe5\x92\x8c\xe6\x89\x80\xe6\x9c\x89\xe5\xb7\xa5\xe5\x8e\x82\xe6\xb3\xa8\xe5\x86\x8c\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\x8e\x9f\xe5\xad\x90\xe6\xa0\xa1\xe9\xaa\x8c\xe7\x9a\x84\xe5\xae\x8c\xe6\x95\xb4\xe8\x8a\x82\xe7\x82\xb9\xe9\x9b\x86\xe5\x90\x88\xef\xbc\x9b\xe5\x90\xaf\xe5\x8a\xa8\xe5\x89\x8d\xe9\xaa\x8c\xe8\xaf\x81\xe6\x89\x80\xe6\x9c\x89\xe8\xb7\xaf\xe7\x94\xb1\xe5\x92\x8c\xe6\x89\x80\xe6\x9c\x89\xe5\xb7\xa5\xe5\x8e\x82\xe6\xb3\xa8\xe5\x86\x8c\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAllowCycles_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x98\xbe\xe5\xbc\x8f\xe5\x85\x81\xe8\xae\xb8\xe8\xb5\x84\xe4\xba\xa7\xe5\x9b\xbe\xe5\xbe\xaa\xe7\x8e\xaf\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe6\x8b\x92\xe7\xbb\x9d\xef\xbc\x8c\xe6\x97\xa7""Configure\xe5\xa7\x8b\xe7\xbb\x88\xe4\xbf\x9d\xe7\x95\x99""DAG\xe5\x90\x88\xe5\x90\x8c\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x98\xbe\xe5\xbc\x8f\xe5\x85\x81\xe8\xae\xb8\xe8\xb5\x84\xe4\xba\xa7\xe5\x9b\xbe\xe5\xbe\xaa\xe7\x8e\xaf\xef\xbc\x9b\xe9\xbb\x98\xe8\xae\xa4\xe6\x8b\x92\xe7\xbb\x9d\xef\xbc\x8c\xe6\x97\xa7""Configure\xe5\xa7\x8b\xe7\xbb\x88\xe4\xbf\x9d\xe7\x95\x99""DAG\xe5\x90\x88\xe5\x90\x8c\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxImmediateCycleTransitions_MetaData[] = {
		{ "Category", "GamePlatform|Flow" },
		{ "ClampMin", "1" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xbf\x9e\xe7\xbb\xad\xe5\x8d\xb3\xe6\x97\xb6\xe5\xae\x8c\xe6\x88\x90\xe7\x89\x87\xe6\xae\xb5\xe4\xb8\xad\xe7\x9a\x84\xe9\x87\x8d\xe5\xa4\x8d\xe8\x8a\x82\xe7\x82\xb9\xe8\xb7\xb3\xe8\xbd\xac\xe4\xb8\x8a\xe9\x99\x90\xef\xbc\x9b\xe5\x8f\xaa\xe9\x99\x90\xe5\x88\xb6\xe5\xbe\xaa\xe7\x8e\xaf\xe9\x87\x8d\xe8\xae\xbf\xef\xbc\x8c\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6""2000\xe8\x8a\x82\xe7\x82\xb9\xe7\xad\x89\xe9\x95\xbf\xe6\x97\xa0\xe7\x8e\xaf\xe9\x93\xbe\xe3\x80\x82 */" },
#endif
		{ "ModuleRelativePath", "Public/Definitions/GamePlatformFlowDefinition.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xbf\x9e\xe7\xbb\xad\xe5\x8d\xb3\xe6\x97\xb6\xe5\xae\x8c\xe6\x88\x90\xe7\x89\x87\xe6\xae\xb5\xe4\xb8\xad\xe7\x9a\x84\xe9\x87\x8d\xe5\xa4\x8d\xe8\x8a\x82\xe7\x82\xb9\xe8\xb7\xb3\xe8\xbd\xac\xe4\xb8\x8a\xe9\x99\x90\xef\xbc\x9b\xe5\x8f\xaa\xe9\x99\x90\xe5\x88\xb6\xe5\xbe\xaa\xe7\x8e\xaf\xe9\x87\x8d\xe8\xae\xbf\xef\xbc\x8c\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6""2000\xe8\x8a\x82\xe7\x82\xb9\xe7\xad\x89\xe9\x95\xbf\xe6\x97\xa0\xe7\x8e\xaf\xe9\x93\xbe\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformFlowDefinition constinit property declarations **************
	static const UECodeGen_Private::FNamePropertyParams NewProp_EntryNodeId;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Nodes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_Nodes;
	static void NewProp_bAllowCycles_SetBit(void* Obj)
	{
		((UGamePlatformFlowDefinition*)Obj)->bAllowCycles = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAllowCycles;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxImmediateCycleTransitions;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformFlowDefinition constinit property declarations ****************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformFlowDefinition>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformFlowDefinition Property Definitions *************************
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_EntryNodeId = { "EntryNodeId", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformFlowDefinition, EntryNodeId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_EntryNodeId_MetaData), NewProp_EntryNodeId_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Nodes_Inner = { "Nodes", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition, METADATA_PARAMS(0, nullptr) }; // a21b4e175a894f5f19689d5761b36d8c7a01810f
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_Nodes = { "Nodes", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformFlowDefinition, Nodes), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Nodes_MetaData), NewProp_Nodes_MetaData) }; // a21b4e175a894f5f19689d5761b36d8c7a01810f
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bAllowCycles = { "bAllowCycles", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UGamePlatformFlowDefinition), &UHT_STATICS::NewProp_bAllowCycles_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAllowCycles_MetaData), NewProp_bAllowCycles_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_MaxImmediateCycleTransitions = { "MaxImmediateCycleTransitions", nullptr, (EPropertyFlags)0x0010000000010015, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformFlowDefinition, MaxImmediateCycleTransitions), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxImmediateCycleTransitions_MetaData), NewProp_MaxImmediateCycleTransitions_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_EntryNodeId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Nodes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Nodes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bAllowCycles,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MaxImmediateCycleTransitions,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformFlowDefinition Property Definitions ***************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformDefinitionBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformFlowDefinition,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformFlowDefinition;
UClass* Z_Construct_UClass_UGamePlatformFlowDefinition(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformFlowDefinition;
		if (!Z_Registration_Info_UClass_UGamePlatformFlowDefinition.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformFlowDefinition"),
				Z_Registration_Info_UClass_UGamePlatformFlowDefinition.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformFlowDefinition.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformFlowDefinition.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformFlowDefinition.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformFlowDefinition.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformFlowDefinition::UGamePlatformFlowDefinition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformFlowDefinition);
UGamePlatformFlowDefinition::~UGamePlatformFlowDefinition() {}
// ********** End Class UGamePlatformFlowDefinition ************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Definitions_GamePlatformFlowDefinition_h__Script_GamePlatformApplicationFlow_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition, Z_Construct_UScriptStruct_FGamePlatformFlowNodeDefinition_Statics::NewStructOps, TEXT("GamePlatformFlowNodeDefinition"),&Z_Registration_Info_UScriptStruct_FGamePlatformFlowNodeDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformFlowNodeDefinition), 2719698455U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformFlowDefinition, TEXT("UGamePlatformFlowDefinition"), &Z_Registration_Info_UClass_UGamePlatformFlowDefinition, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformFlowDefinition), 999719752U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GamePlatform_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Definitions_GamePlatformFlowDefinition_h__Script_GamePlatformApplicationFlow_e96d392c80c760b52a844daa1309490de3a36c22{
	TEXT("/Script/GamePlatformApplicationFlow"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
