// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Interfaces/GamePlatformFlowNode.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformFlowNode() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformApplicationFlow(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformFlowNode ****************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformFlowNode_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe6\xb3\xa8\xe5\x85\xa5\xe7\x9a\x84 C++ \xe8\x8a\x82\xe7\x82\xb9\xe5\xa5\x91\xe7\xba\xa6\xe3\x80\x82\xe7\x99\xbb\xe5\xbd\x95\xe3\x80\x81\xe5\x87\x86\xe5\x85\xa5\xe5\x92\x8c\xe8\xb5\x84\xe6\xba\x90\xe5\xb0\xb1\xe7\xbb\xaa\xe8\x8a\x82\xe7\x82\xb9\xe7\x94\xb1\xe5\xaf\xb9\xe5\xba\x94\xe4\xb8\x8a\xe5\xb1\x82\xe5\xae\x9e\xe7\x8e\xb0\xe3\x80\x82\n * \xe5\x90\x8c\xe4\xb8\x80\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe4\xb8\x8d\xe5\x8f\xaf\xe5\x90\x8c\xe6\x97\xb6\xe6\xb3\xa8\xe5\x85\xa5\xe4\xb8\xa4\xe4\xb8\xaa\xe6\xad\xa5\xe9\xaa\xa4\xef\xbc\x9b\xe8\xb7\xa8 GameInstance \xe4\xb8\x8d\xe5\x85\xb1\xe4\xba\xab\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82\n * \xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\x8d\xe6\x8e\xa8\xe6\x96\xad\xe9\x87\x8d\xe8\xaf\x95\xe5\xae\x89\xe5\x85\xa8\xe6\x80\xa7\xef\xbc\x8c\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe7\xa1\xae\xe4\xbf\x9d\xe8\xaf\xb7\xe6\xb1\x82\xe5\xb9\x82\xe7\xad\x89\xe5\xb9\xb6\xe5\x9c\xa8 Finish \xe4\xb8\xad\xe8\xa7\xa3\xe9\x99\xa4\xe4\xb8\x9a\xe5\x8a\xa1\xe5\xa7\x94\xe6\x89\x98\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "Interfaces/GamePlatformFlowNode.h" },
		{ "ModuleRelativePath", "Public/Interfaces/GamePlatformFlowNode.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe6\xb3\xa8\xe5\x85\xa5\xe7\x9a\x84 C++ \xe8\x8a\x82\xe7\x82\xb9\xe5\xa5\x91\xe7\xba\xa6\xe3\x80\x82\xe7\x99\xbb\xe5\xbd\x95\xe3\x80\x81\xe5\x87\x86\xe5\x85\xa5\xe5\x92\x8c\xe8\xb5\x84\xe6\xba\x90\xe5\xb0\xb1\xe7\xbb\xaa\xe8\x8a\x82\xe7\x82\xb9\xe7\x94\xb1\xe5\xaf\xb9\xe5\xba\x94\xe4\xb8\x8a\xe5\xb1\x82\xe5\xae\x9e\xe7\x8e\xb0\xe3\x80\x82\n\xe5\x90\x8c\xe4\xb8\x80\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe4\xb8\x8d\xe5\x8f\xaf\xe5\x90\x8c\xe6\x97\xb6\xe6\xb3\xa8\xe5\x85\xa5\xe4\xb8\xa4\xe4\xb8\xaa\xe6\xad\xa5\xe9\xaa\xa4\xef\xbc\x9b\xe8\xb7\xa8 GameInstance \xe4\xb8\x8d\xe5\x85\xb1\xe4\xba\xab\xe8\x8a\x82\xe7\x82\xb9\xe5\xae\x9e\xe4\xbe\x8b\xe3\x80\x82\n\xe5\xb9\xb3\xe5\x8f\xb0\xe4\xb8\x8d\xe6\x8e\xa8\xe6\x96\xad\xe9\x87\x8d\xe8\xaf\x95\xe5\xae\x89\xe5\x85\xa8\xe6\x80\xa7\xef\xbc\x8c\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe7\xa1\xae\xe4\xbf\x9d\xe8\xaf\xb7\xe6\xb1\x82\xe5\xb9\x82\xe7\xad\x89\xe5\xb9\xb6\xe5\x9c\xa8 Finish \xe4\xb8\xad\xe8\xa7\xa3\xe9\x99\xa4\xe4\xb8\x9a\xe5\x8a\xa1\xe5\xa7\x94\xe6\x89\x98\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformFlowNode constinit property declarations ********************
// ********** End Class UGamePlatformFlowNode constinit property declarations **********************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformFlowNode>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UObject,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformFlowNode,
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
	0x001000A9u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformFlowNode;
UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformFlowNode;
		if (!Z_Registration_Info_UClass_UGamePlatformFlowNode.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformFlowNode"),
				Z_Registration_Info_UClass_UGamePlatformFlowNode.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformFlowNode.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformFlowNode.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformFlowNode.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformFlowNode.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformFlowNode::UGamePlatformFlowNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformFlowNode);
UGamePlatformFlowNode::~UGamePlatformFlowNode() {}
// ********** End Class UGamePlatformFlowNode ******************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h__Script_GamePlatformApplicationFlow_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformFlowNode, TEXT("UGamePlatformFlowNode"), &Z_Registration_Info_UClass_UGamePlatformFlowNode, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformFlowNode), 2220334691U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_Interfaces_GamePlatformFlowNode_h__Script_GamePlatformApplicationFlow_4d319d3d46847c7eb8841f4291fb1a0b01ce5ca9{
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
