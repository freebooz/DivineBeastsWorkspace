// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXWorldSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UWorldSubsystem(ETypeConstructPhase);
NIAGARA_API UClass* Z_Construct_UClass_UNiagaraComponent(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXWorldSubsystem(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UClass* Z_Construct_UClass_UGamePlatformVFXWorldSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UGamePlatformVFXWorldSubsystem Function HandleNiagaraSystemFinished ******
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UGamePlatformVFXWorldSubsystem_HandleNiagaraSystemFinished_Statics
struct UHT_STATICS
{
	struct GamePlatformVFXWorldSubsystem_eventHandleNiagaraSystemFinished_Parms
	{
		UNiagaraComponent* FinishedComponent;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "ModuleRelativePath", "Private/Subsystems/GamePlatformVFXWorldSubsystem.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FinishedComponent_MetaData[] = {
		{ "EditInline", "true" },
	};
#endif // WITH_METADATA

// ********** Begin Function HandleNiagaraSystemFinished constinit property declarations ***********
	static const UECodeGen_Private::FObjectPropertyParams NewProp_FinishedComponent;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function HandleNiagaraSystemFinished constinit property declarations *************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function HandleNiagaraSystemFinished Property Definitions **********************
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_FinishedComponent = { "FinishedComponent", nullptr, (EPropertyFlags)0x0010000000080080, UECodeGen_Private::EPropertyGenFlags::Object, nullptr, nullptr, 1, STRUCT_OFFSET(GamePlatformVFXWorldSubsystem_eventHandleNiagaraSystemFinished_Parms, FinishedComponent), Z_Construct_UClass_UNiagaraComponent, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FinishedComponent_MetaData), NewProp_FinishedComponent_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FinishedComponent,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function HandleNiagaraSystemFinished Property Definitions ************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UGamePlatformVFXWorldSubsystem, nullptr, "HandleNiagaraSystemFinished", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::GamePlatformVFXWorldSubsystem_eventHandleNiagaraSystemFinished_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00040401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::GamePlatformVFXWorldSubsystem_eventHandleNiagaraSystemFinished_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGamePlatformVFXWorldSubsystem_HandleNiagaraSystemFinished(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UGamePlatformVFXWorldSubsystem::execHandleNiagaraSystemFinished)
{
	P_GET_OBJECT(UNiagaraComponent,Z_Param_FinishedComponent);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->HandleNiagaraSystemFinished(Z_Param_FinishedComponent);
	P_NATIVE_END;
}
// ********** End Class UGamePlatformVFXWorldSubsystem Function HandleNiagaraSystemFinished ********

// ********** Begin Class UGamePlatformVFXWorldSubsystem *******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGamePlatformVFXWorldSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe4\xb8\x96\xe7\x95\x8c\xe7\xba\xa7 VFX \xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82\xe8\xaf\xa5\xe7\xb1\xbb\xe4\xbd\x8d\xe4\xba\x8e Private\xef\xbc\x8c\xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe5\x8f\xaa\xe4\xbe\x9d\xe8\xb5\x96 IGamePlatformVFXService\xe3\x80\x82 */" },
#endif
		{ "IncludePath", "Subsystems/GamePlatformVFXWorldSubsystem.h" },
		{ "ModuleRelativePath", "Private/Subsystems/GamePlatformVFXWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xb8\x96\xe7\x95\x8c\xe7\xba\xa7 VFX \xe8\xbf\x90\xe8\xa1\x8c\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82\xe8\xaf\xa5\xe7\xb1\xbb\xe4\xbd\x8d\xe4\xba\x8e Private\xef\xbc\x8c\xe8\xb0\x83\xe7\x94\xa8\xe6\x96\xb9\xe5\x8f\xaa\xe4\xbe\x9d\xe8\xb5\x96 IGamePlatformVFXService\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGamePlatformVFXWorldSubsystem constinit property declarations ***********
// ********** End Class UGamePlatformVFXWorldSubsystem constinit property declarations *************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("HandleNiagaraSystemFinished"), .Pointer = &UGamePlatformVFXWorldSubsystem::execHandleNiagaraSystemFinished },
	};
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UGamePlatformVFXWorldSubsystem_HandleNiagaraSystemFinished, "HandleNiagaraSystemFinished" }, // a430ce4357af57bdb5ea3843295b19a1414dfd16
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGamePlatformVFXWorldSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UWorldSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGamePlatformVFXWorldSubsystem,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static void UGamePlatformVFXWorldSubsystem_StaticRegisterNativesUGamePlatformVFXWorldSubsystem()
{
	UClass* Class = UGamePlatformVFXWorldSubsystem::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, 		MakeConstArrayView(UHT_STATICS::Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem;
UClass* Z_Construct_UClass_UGamePlatformVFXWorldSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGamePlatformVFXWorldSubsystem;
		if (!Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GamePlatformVFXWorldSubsystem"),
				Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.InnerSingleton,
				UGamePlatformVFXWorldSubsystem_StaticRegisterNativesUGamePlatformVFXWorldSubsystem,
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
		return Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem.OuterSingleton;
}
#undef UHT_STATICS
UGamePlatformVFXWorldSubsystem::UGamePlatformVFXWorldSubsystem(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGamePlatformVFXWorldSubsystem);
UGamePlatformVFXWorldSubsystem::~UGamePlatformVFXWorldSubsystem() {}
// ********** End Class UGamePlatformVFXWorldSubsystem *********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Private_Subsystems_GamePlatformVFXWorldSubsystem_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGamePlatformVFXWorldSubsystem, TEXT("UGamePlatformVFXWorldSubsystem"), &Z_Registration_Info_UClass_UGamePlatformVFXWorldSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGamePlatformVFXWorldSubsystem), 3217770980U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Private_Subsystems_GamePlatformVFXWorldSubsystem_h__Script_GamePlatformVFXClient_e1015027cbbeba80e36fcd2c35e16f699a6eeb1f{
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
