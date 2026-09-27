// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Execution/GamePlatformVFXHostActor.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXHostActor() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_AGamePlatformVFXHostActor(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_AGamePlatformVFXHostActor(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class AGamePlatformVFXHostActor ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_AGamePlatformVFXHostActor_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xbb\x85\xe5\xa4\x8d\xe6\x9d\x82\xe7\x8b\xac\xe7\xab\x8b\xe7\xa9\xba\xe9\x97\xb4 VFX \xe4\xbd\xbf\xe7\x94\xa8\xef\xbc\x9b\xe6\x99\xae\xe9\x80\x9a VFX \xe5\xba\x94\xe4\xbc\x98\xe5\x85\x88\xe7\x9b\xb4\xe6\x8e\xa5\xe4\xbd\xbf\xe7\x94\xa8 UNiagaraComponent\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Execution/GamePlatformVFXHostActor.h" },
		{ "IsBlueprintBase", "false" },
		{ "ModuleRelativePath", "Private/Execution/GamePlatformVFXHostActor.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xbb\x85\xe5\xa4\x8d\xe6\x9d\x82\xe7\x8b\xac\xe7\xab\x8b\xe7\xa9\xba\xe9\x97\xb4 VFX \xe4\xbd\xbf\xe7\x94\xa8\xef\xbc\x9b\xe6\x99\xae\xe9\x80\x9a VFX \xe5\xba\x94\xe4\xbc\x98\xe5\x85\x88\xe7\x9b\xb4\xe6\x8e\xa5\xe4\xbd\xbf\xe7\x94\xa8 UNiagaraComponent\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class AGamePlatformVFXHostActor constinit property declarations ****************
// ********** End Class AGamePlatformVFXHostActor constinit property declarations ******************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AGamePlatformVFXHostActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_AActor,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_AGamePlatformVFXHostActor,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x008000ACu,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_AGamePlatformVFXHostActor;
UClass* Z_Construct_UClass_AGamePlatformVFXHostActor(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = AGamePlatformVFXHostActor;
		if (!Z_Registration_Info_UClass_AGamePlatformVFXHostActor.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXHostActor"),
				Z_Registration_Info_UClass_AGamePlatformVFXHostActor.InnerSingleton,
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
		return Z_Registration_Info_UClass_AGamePlatformVFXHostActor.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_AGamePlatformVFXHostActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AGamePlatformVFXHostActor.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_AGamePlatformVFXHostActor.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AGamePlatformVFXHostActor);
AGamePlatformVFXHostActor::~AGamePlatformVFXHostActor() {}
// ********** End Class AGamePlatformVFXHostActor **************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Private_Execution_GamePlatformVFXHostActor_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AGamePlatformVFXHostActor, TEXT("AGamePlatformVFXHostActor"), &Z_Registration_Info_UClass_AGamePlatformVFXHostActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AGamePlatformVFXHostActor), 482330519U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Private_Execution_GamePlatformVFXHostActor_h__Script_GamePlatformVFXClient_2376f7aa75f5e1957b8399bbd1601a6863f0db50{
	TEXT("/Script/GamePlatformVFXClient"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
