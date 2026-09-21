// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformCallbackFlowNode() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformApplicationFlow(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformCallbackFlowNode(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformCallbackFlowNode(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformCallbackFlowNode ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformCallbackFlowNode_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe7\x9a\x84\xe5\x87\xbd\xe6\x95\xb0\xe5\xbc\x8f\xe8\x8a\x82\xe7\x82\xb9\xe9\x80\x82\xe9\x85\x8d\xe5\x99\xa8\xef\xbc\x8c\xe5\x8f\xaf\xe7\x9b\xb4\xe6\x8e\xa5\xe6\xb3\xa8\xe5\x85\xa5\xe7\x9c\x9f\xe5\xae\x9e\xe6\x9c\x8d\xe5\x8a\xa1\xe6\x93\x8d\xe4\xbd\x9c\xef\xbc\x8c\xe6\x97\xa0\xe9\xa1\xbb\xe4\xb8\xba\xe6\xaf\x8f\xe4\xb8\xaa\xe6\xad\xa5\xe9\xaa\xa4\xe5\x8f\xa6\xe5\xbb\xba\xe5\x8f\x8d\xe5\xb0\x84\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x82\n * \xe6\x89\xa7\xe8\xa1\x8c\xe4\xb8\x8e\xe6\xb8\x85\xe7\x90\x86\xe5\x87\xbd\xe6\x95\xb0\xe5\xbf\x85\xe9\xa1\xbb\xe6\x88\x90\xe5\xaf\xb9\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x94\xe5\x8f\xaa\xe8\x83\xbd\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x80\xe6\xac\xa1\xef\xbc\x9b\xe5\x87\xbd\xe6\x95\xb0\xe9\x97\xad\xe5\x8c\x85\xe4\xb8\x8d\xe5\xbe\x97\xe5\xbc\xba\xe6\x8c\x81\xe6\x9c\x89 GameInstance \xe5\xbd\xa2\xe6\x88\x90\xe5\xbc\x95\xe7\x94\xa8\xe7\x8e\xaf\xe3\x80\x82\n * \xe8\xb7\xa8 UObject \xe5\xbc\x95\xe7\x94\xa8\xe8\xaf\xb7\xe6\x8d\x95\xe8\x8e\xb7 TWeakObjectPtr\xef\xbc\x8c\xe5\xb9\xb6\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe6\xa3\x80\xe6\x9f\xa5\xe6\x9c\x89\xe6\x95\x88\xe6\x80\xa7\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Interfaces/GamePlatformCallbackFlowNode.h" },
		{ "ModuleRelativePath", "Public/Interfaces/GamePlatformCallbackFlowNode.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe7\x9a\x84\xe5\x87\xbd\xe6\x95\xb0\xe5\xbc\x8f\xe8\x8a\x82\xe7\x82\xb9\xe9\x80\x82\xe9\x85\x8d\xe5\x99\xa8\xef\xbc\x8c\xe5\x8f\xaf\xe7\x9b\xb4\xe6\x8e\xa5\xe6\xb3\xa8\xe5\x85\xa5\xe7\x9c\x9f\xe5\xae\x9e\xe6\x9c\x8d\xe5\x8a\xa1\xe6\x93\x8d\xe4\xbd\x9c\xef\xbc\x8c\xe6\x97\xa0\xe9\xa1\xbb\xe4\xb8\xba\xe6\xaf\x8f\xe4\xb8\xaa\xe6\xad\xa5\xe9\xaa\xa4\xe5\x8f\xa6\xe5\xbb\xba\xe5\x8f\x8d\xe5\xb0\x84\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x82\n\xe6\x89\xa7\xe8\xa1\x8c\xe4\xb8\x8e\xe6\xb8\x85\xe7\x90\x86\xe5\x87\xbd\xe6\x95\xb0\xe5\xbf\x85\xe9\xa1\xbb\xe6\x88\x90\xe5\xaf\xb9\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x94\xe5\x8f\xaa\xe8\x83\xbd\xe7\xbb\x91\xe5\xae\x9a\xe4\xb8\x80\xe6\xac\xa1\xef\xbc\x9b\xe5\x87\xbd\xe6\x95\xb0\xe9\x97\xad\xe5\x8c\x85\xe4\xb8\x8d\xe5\xbe\x97\xe5\xbc\xba\xe6\x8c\x81\xe6\x9c\x89 GameInstance \xe5\xbd\xa2\xe6\x88\x90\xe5\xbc\x95\xe7\x94\xa8\xe7\x8e\xaf\xe3\x80\x82\n\xe8\xb7\xa8 UObject \xe5\xbc\x95\xe7\x94\xa8\xe8\xaf\xb7\xe6\x8d\x95\xe8\x8e\xb7 TWeakObjectPtr\xef\xbc\x8c\xe5\xb9\xb6\xe5\x9c\xa8\xe6\xb8\xb8\xe6\x88\x8f\xe7\xba\xbf\xe7\xa8\x8b\xe6\xa3\x80\xe6\x9f\xa5\xe6\x9c\x89\xe6\x95\x88\xe6\x80\xa7\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformCallbackFlowNode constinit property declarations ************
// ********** End Class UGamePlatformCallbackFlowNode constinit property declarations **************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformCallbackFlowNode>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformFlowNode,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformCallbackFlowNode,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x001000A8u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode;
UClass* Z_Construct_UClass_UGamePlatformCallbackFlowNode(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformCallbackFlowNode;
		if (!Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformCallbackFlowNode"),
				Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformCallbackFlowNode::UGamePlatformCallbackFlowNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformCallbackFlowNode);
UGamePlatformCallbackFlowNode::~UGamePlatformCallbackFlowNode() {}
// ********** End Class UGamePlatformCallbackFlowNode **********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformCallbackFlowNode_h__Script_GamePlatformApplicationFlow_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformCallbackFlowNode, TEXT("UGamePlatformCallbackFlowNode"), &Z_Registration_Info_UClass_UGamePlatformCallbackFlowNode, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformCallbackFlowNode), 2063973205U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformCallbackFlowNode_h__Script_GamePlatformApplicationFlow_4c981090f1a5cdd17dcf4e299bd22c3277f88a10{
	TEXT("/Script/GamePlatformApplicationFlow"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
