// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Engine/GameInstance.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformApplicationFlowSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UGameInstanceSubsystem(ETypeConstructPhase);
COREUOBJECT_API UClass* Z_Construct_UClass_UObject(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformApplicationFlow(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem(ETypeConstructPhase);
GAMEPLATFORMAPPLICATIONFLOW_API UClass* Z_Construct_UClass_UGamePlatformFlowNode(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformApplicationFlowSubsystem ************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe6\xaf\x8f\xe4\xb8\xaa GameInstance \xe5\x94\xaf\xe4\xb8\x80\xe4\xb8\xbb\xe6\xb5\x81\xe7\xa8\x8b\xe6\x9c\x8d\xe5\x8a\xa1\xef\xbc\x9b\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xef\xbc\x8f\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x8f\xaf\xe7\x94\xa8\xef\xbc\x8c\xe4\xb8\x8d\xe5\x8c\x85\xe5\x90\xab\xe5\x85\xb7\xe4\xbd\x93\xe4\xb8\x9a\xe5\x8a\xa1\xe8\x8a\x82\xe7\x82\xb9\xe3\x80\x82\n * C++ \xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe9\x80\x9a\xe8\xbf\x87 GameInstance->GetSubsystem \xe8\x8e\xb7\xe5\x8f\x96\xef\xbc\x8c\xe5\x9b\xa0\xe6\xad\xa4\xe5\x8f\xaa\xe5\x85\xac\xe5\xbc\x80\xe7\x94\x9f\xe5\x91\xbd\xe5\x91\xa8\xe6\x9c\x9f\xe5\x85\xa5\xe5\x8f\xa3\xe5\x8f\x8a\xe7\xa8\xb3\xe5\xae\x9a\xe5\xa5\x91\xe7\xba\xa6\xe3\x80\x82\n * \xe8\xb7\xa8\xe5\x9c\xb0\xe5\x9b\xbe\xe6\x8c\x81\xe7\xbb\xad\xe5\xad\x98\xe5\x9c\xa8\xef\xbc\x9b\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe8\x87\xaa\xe8\xa1\x8c\xe9\x87\x8d\xe6\x96\xb0\xe5\x8f\x96\xe5\xbe\x97 World\xe3\x80\x82\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xa2\x84\xe8\xa7\x88\xef\xbc\x8f""Commandlet \xe4\xb8\x8d\xe5\x88\x9b\xe5\xbb\xba\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82\n */" },
#endif
		{ "IncludePath", "API/GamePlatformApplicationFlowSubsystem.h" },
		{ "ModuleRelativePath", "Public/API/GamePlatformApplicationFlowSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xaf\x8f\xe4\xb8\xaa GameInstance \xe5\x94\xaf\xe4\xb8\x80\xe4\xb8\xbb\xe6\xb5\x81\xe7\xa8\x8b\xe6\x9c\x8d\xe5\x8a\xa1\xef\xbc\x9b\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xef\xbc\x8f\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x8f\xaf\xe7\x94\xa8\xef\xbc\x8c\xe4\xb8\x8d\xe5\x8c\x85\xe5\x90\xab\xe5\x85\xb7\xe4\xbd\x93\xe4\xb8\x9a\xe5\x8a\xa1\xe8\x8a\x82\xe7\x82\xb9\xe3\x80\x82\nC++ \xe7\xbb\x84\xe5\x90\x88\xe6\xa0\xb9\xe9\x80\x9a\xe8\xbf\x87 GameInstance->GetSubsystem \xe8\x8e\xb7\xe5\x8f\x96\xef\xbc\x8c\xe5\x9b\xa0\xe6\xad\xa4\xe5\x8f\xaa\xe5\x85\xac\xe5\xbc\x80\xe7\x94\x9f\xe5\x91\xbd\xe5\x91\xa8\xe6\x9c\x9f\xe5\x85\xa5\xe5\x8f\xa3\xe5\x8f\x8a\xe7\xa8\xb3\xe5\xae\x9a\xe5\xa5\x91\xe7\xba\xa6\xe3\x80\x82\n\xe8\xb7\xa8\xe5\x9c\xb0\xe5\x9b\xbe\xe6\x8c\x81\xe7\xbb\xad\xe5\xad\x98\xe5\x9c\xa8\xef\xbc\x9b\xe8\x8a\x82\xe7\x82\xb9\xe5\xbf\x85\xe9\xa1\xbb\xe8\x87\xaa\xe8\xa1\x8c\xe9\x87\x8d\xe6\x96\xb0\xe5\x8f\x96\xe5\xbe\x97 World\xe3\x80\x82\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe9\xa2\x84\xe8\xa7\x88\xef\xbc\x8f""Commandlet \xe4\xb8\x8d\xe5\x88\x9b\xe5\xbb\xba\xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ActivePayload_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Execute/Finish \xe9\x87\x8d\xe5\x85\xa5\xe6\x8e\xa7\xe5\x88\xb6\xe5\x9c\xa8\xe9\x80\x82\xe9\x85\x8d\xe5\xb1\x82\xe5\x90\x8c\xe6\xa0\xb7\xe6\x8b\x92\xe7\xbb\x9d\xe3\x80\x82\n" },
#endif
		{ "ModuleRelativePath", "Public/API/GamePlatformApplicationFlowSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Execute/Finish \xe9\x87\x8d\xe5\x85\xa5\xe6\x8e\xa7\xe5\x88\xb6\xe5\x9c\xa8\xe9\x80\x82\xe9\x85\x8d\xe5\xb1\x82\xe5\x90\x8c\xe6\xa0\xb7\xe6\x8b\x92\xe7\xbb\x9d\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OwnedNodes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// GC \xe5\x8f\xaf\xe8\xbf\xbd\xe8\xb8\xaa\xe5\xbc\xba\xe5\xbc\x95\xe7\x94\xa8\xef\xbc\x8c\xe9\x81\xbf\xe5\x85\x8d shared_ptr \xe6\x8c\x81\xe6\x9c\x89\xe9\x9a\x90\xe5\xbc\x8f UObject \xe6\xa0\xb9\xe9\x80\xa0\xe6\x88\x90 GameInstance \xe5\xbc\x95\xe7\x94\xa8\xe7\x8e\xaf\xe3\x80\x82\n" },
#endif
		{ "ModuleRelativePath", "Public/API/GamePlatformApplicationFlowSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "GC \xe5\x8f\xaf\xe8\xbf\xbd\xe8\xb8\xaa\xe5\xbc\xba\xe5\xbc\x95\xe7\x94\xa8\xef\xbc\x8c\xe9\x81\xbf\xe5\x85\x8d shared_ptr \xe6\x8c\x81\xe6\x9c\x89\xe9\x9a\x90\xe5\xbc\x8f UObject \xe6\xa0\xb9\xe9\x80\xa0\xe6\x88\x90 GameInstance \xe5\xbc\x95\xe7\x94\xa8\xe7\x8e\xaf\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformApplicationFlowSubsystem constinit property declarations ****
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ActivePayload;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_OwnedNodes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_OwnedNodes;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UGamePlatformApplicationFlowSubsystem constinit property declarations ******
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformApplicationFlowSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UGamePlatformApplicationFlowSubsystem Property Definitions ***************
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_ActivePayload = { "ActivePayload", nullptr, (EPropertyFlags)0x0144000000002000, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformApplicationFlowSubsystem, ActivePayload), Z_Construct_UClass_UObject, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ActivePayload_MetaData), NewProp_ActivePayload_MetaData) };
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_OwnedNodes_Inner = { "OwnedNodes", nullptr, (EPropertyFlags)0x0104000000000000, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, nullptr, nullptr, 1, 0, Z_Construct_UClass_UGamePlatformFlowNode, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_OwnedNodes = { "OwnedNodes", nullptr, (EPropertyFlags)0x0144000000002000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UGamePlatformApplicationFlowSubsystem, OwnedNodes), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OwnedNodes_MetaData), NewProp_OwnedNodes_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ActivePayload,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_OwnedNodes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_OwnedNodes,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UGamePlatformApplicationFlowSubsystem Property Definitions *****************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGameInstanceSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformApplicationFlow,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem,
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
	0x001000A8u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem;
UClass* Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformApplicationFlowSubsystem;
		if (!Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformApplicationFlowSubsystem"),
				Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.InnerSingleton,
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
		return Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem.OuterSingleton;
}
#undef UHT_STATICS
// ********** End Class UGamePlatformApplicationFlowSubsystem **************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_API_GamePlatformApplicationFlowSubsystem_h__Script_GamePlatformApplicationFlow_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformApplicationFlowSubsystem, TEXT("UGamePlatformApplicationFlowSubsystem"), &Z_Registration_Info_UClass_UGamePlatformApplicationFlowSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformApplicationFlowSubsystem), 4221928007U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Application_GamePlatformApplicationFlow_Source_GamePlatformApplicationFlow_Public_API_GamePlatformApplicationFlowSubsystem_h__Script_GamePlatformApplicationFlow_8a728da9639cd25fcedc962a53bcf75867f5316b{
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
