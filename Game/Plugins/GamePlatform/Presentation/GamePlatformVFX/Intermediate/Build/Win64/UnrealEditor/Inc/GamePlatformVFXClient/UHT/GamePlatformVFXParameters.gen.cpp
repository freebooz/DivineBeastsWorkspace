// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXParameters.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXParameters() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FLinearColor(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXParameters(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGamePlatformVFXParameters ****************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGamePlatformVFXParameters_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGamePlatformVFXParameters>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGamePlatformVFXParameters); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe9\x80\x9a\xe7\x94\xa8 Niagara User Parameter \xe5\x8f\x82\xe6\x95\xb0\xe9\x9b\x86\xe5\x90\x88\xe3\x80\x82\n * \xe5\x8f\x82\xe6\x95\xb0\xe5\x90\x8d\xe5\xbb\xba\xe8\xae\xae\xe7\xbb\x9f\xe4\xb8\x80\xe4\xbd\xbf\xe7\x94\xa8 User.VFX.* \xe5\x91\xbd\xe5\x90\x8d\xe7\xa9\xba\xe9\x97\xb4\xe3\x80\x82\n */" },
#endif
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXParameters.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe9\x80\x9a\xe7\x94\xa8 Niagara User Parameter \xe5\x8f\x82\xe6\x95\xb0\xe9\x9b\x86\xe5\x90\x88\xe3\x80\x82\n\xe5\x8f\x82\xe6\x95\xb0\xe5\x90\x8d\xe5\xbb\xba\xe8\xae\xae\xe7\xbb\x9f\xe4\xb8\x80\xe4\xbd\xbf\xe7\x94\xa8 User.VFX.* \xe5\x91\xbd\xe5\x90\x8d\xe7\xa9\xba\xe9\x97\xb4\xe3\x80\x82" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FloatValues_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXParameters.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_IntValues_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXParameters.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VectorValues_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXParameters.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ColorValues_MetaData[] = {
		{ "Category", "VFX" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXParameters.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGamePlatformVFXParameters constinit property declarations ********
	static const UECodeGen_Private::FFloatPropertyParams NewProp_FloatValues_ValueProp;
	static const UECodeGen_Private::FNamePropertyParams NewProp_FloatValues_Key_KeyProp;
	static const UECodeGen_Private::FMapPropertyParams NewProp_FloatValues;
	static const UECodeGen_Private::FIntPropertyParams NewProp_IntValues_ValueProp;
	static const UECodeGen_Private::FNamePropertyParams NewProp_IntValues_Key_KeyProp;
	static const UECodeGen_Private::FMapPropertyParams NewProp_IntValues;
	static const UECodeGen_Private::FStructPropertyParams NewProp_VectorValues_ValueProp;
	static const UECodeGen_Private::FNamePropertyParams NewProp_VectorValues_Key_KeyProp;
	static const UECodeGen_Private::FMapPropertyParams NewProp_VectorValues;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ColorValues_ValueProp;
	static const UECodeGen_Private::FNamePropertyParams NewProp_ColorValues_Key_KeyProp;
	static const UECodeGen_Private::FMapPropertyParams NewProp_ColorValues;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGamePlatformVFXParameters constinit property declarations **********
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGamePlatformVFXParameters>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGamePlatformVFXParameters Property Definitions *******************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_FloatValues_ValueProp = { "FloatValues", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, 1, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_FloatValues_Key_KeyProp = { "FloatValues_Key", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FMapPropertyParams UHT_STATICS::NewProp_FloatValues = { "FloatValues", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Map, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXParameters, FloatValues), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FloatValues_MetaData), NewProp_FloatValues_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_IntValues_ValueProp = { "IntValues", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, 1, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_IntValues_Key_KeyProp = { "IntValues_Key", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FMapPropertyParams UHT_STATICS::NewProp_IntValues = { "IntValues", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Map, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXParameters, IntValues), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_IntValues_MetaData), NewProp_IntValues_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_VectorValues_ValueProp = { "VectorValues", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 1, Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_VectorValues_Key_KeyProp = { "VectorValues_Key", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FMapPropertyParams UHT_STATICS::NewProp_VectorValues = { "VectorValues", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Map, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXParameters, VectorValues), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VectorValues_MetaData), NewProp_VectorValues_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ColorValues_ValueProp = { "ColorValues", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 1, Z_Construct_UScriptStruct_FLinearColor, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_ColorValues_Key_KeyProp = { "ColorValues_Key", nullptr, (EPropertyFlags)0x0000000000000001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FMapPropertyParams UHT_STATICS::NewProp_ColorValues = { "ColorValues", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Map, nullptr, nullptr, 1, STRUCT_OFFSET(FGamePlatformVFXParameters, ColorValues), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ColorValues_MetaData), NewProp_ColorValues_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FloatValues_ValueProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FloatValues_Key_KeyProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FloatValues,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_IntValues_ValueProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_IntValues_Key_KeyProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_IntValues,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_VectorValues_ValueProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_VectorValues_Key_KeyProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_VectorValues,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ColorValues_ValueProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ColorValues_Key_KeyProp,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ColorValues,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGamePlatformVFXParameters Property Definitions *********************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	&NewStructOps,
	"GamePlatformVFXParameters",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGamePlatformVFXParameters>(),
	alignof(FGamePlatformVFXParameters),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters;
UScriptStruct* Z_Construct_UScriptStruct_FGamePlatformVFXParameters(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGamePlatformVFXParameters, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("GamePlatformVFXParameters"));
		}
		return Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGamePlatformVFXParameters ******************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXParameters_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGamePlatformVFXParameters, Z_Construct_UScriptStruct_FGamePlatformVFXParameters_Statics::NewStructOps, TEXT("GamePlatformVFXParameters"),&Z_Registration_Info_UScriptStruct_FGamePlatformVFXParameters, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGamePlatformVFXParameters), 4035017266U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXParameters_h__Script_GamePlatformVFXClient_4b6554ea028cd23bc88b51b142faf30866c3851a{
	TEXT("/Script/GamePlatformVFXClient"),
	nullptr, 0,
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
