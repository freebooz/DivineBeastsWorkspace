// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/GamePlatformVFXTypes.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGamePlatformVFXTypes() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode(ETypeConstructPhase);
GAMEPLATFORMVFXCLIENT_API UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EGamePlatformVFXBehavior **************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXBehavior>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Area.DisplayName", "Area / \xe5\x8c\xba\xe5\x9f\x9f\xe5\x9e\x8b" },
		{ "Area.Name", "EGamePlatformVFXBehavior::Area" },
		{ "Attached.DisplayName", "Attached / \xe9\x99\x84\xe7\x9d\x80\xe5\x9e\x8b" },
		{ "Attached.Name", "EGamePlatformVFXBehavior::Attached" },
		{ "Beam.DisplayName", "Beam / \xe5\x85\x89\xe6\x9d\x9f\xe5\x9e\x8b" },
		{ "Beam.Name", "EGamePlatformVFXBehavior::Beam" },
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** VFX \xe8\xa1\x8c\xe4\xb8\xba\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x82\xe5\xae\x83\xe6\x8f\x8f\xe8\xbf\xb0\xe2\x80\x9c\xe5\xa6\x82\xe4\xbd\x95\xe8\xbf\x90\xe8\xa1\x8c\xe2\x80\x9d\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa3\xe8\xa1\xa8\xe4\xb8\x9a\xe5\x8a\xa1\xe5\x86\x85\xe5\xae\xb9\xe5\x88\x86\xe7\xb1\xbb\xe3\x80\x82 */" },
#endif
		{ "Composite.DisplayName", "Composite / \xe5\xa4\x8d\xe5\x90\x88\xe5\x9e\x8b" },
		{ "Composite.Name", "EGamePlatformVFXBehavior::Composite" },
		{ "Instant.DisplayName", "Instant / \xe7\x9e\xac\xe6\x97\xb6\xe5\x9e\x8b" },
		{ "Instant.Name", "EGamePlatformVFXBehavior::Instant" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "Portal.DisplayName", "Portal / \xe4\xbc\xa0\xe9\x80\x81\xe9\x97\xa8\xe8\xa1\xa8\xe7\x8e\xb0\xe5\x9e\x8b" },
		{ "Portal.Name", "EGamePlatformVFXBehavior::Portal" },
		{ "Projectile.DisplayName", "Projectile / \xe8\xa7\x86\xe8\xa7\x89\xe6\x8a\x95\xe5\xb0\x84\xe5\x9e\x8b" },
		{ "Projectile.Name", "EGamePlatformVFXBehavior::Projectile" },
		{ "Shield.DisplayName", "Shield / \xe6\x8a\xa4\xe7\x9b\xbe\xe8\xa1\xa8\xe7\x8e\xb0\xe5\x9e\x8b" },
		{ "Shield.Name", "EGamePlatformVFXBehavior::Shield" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe8\xa1\x8c\xe4\xb8\xba\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x82\xe5\xae\x83\xe6\x8f\x8f\xe8\xbf\xb0\xe2\x80\x9c\xe5\xa6\x82\xe4\xbd\x95\xe8\xbf\x90\xe8\xa1\x8c\xe2\x80\x9d\xef\xbc\x8c\xe4\xb8\x8d\xe4\xbb\xa3\xe8\xa1\xa8\xe4\xb8\x9a\xe5\x8a\xa1\xe5\x86\x85\xe5\xae\xb9\xe5\x88\x86\xe7\xb1\xbb\xe3\x80\x82" },
#endif
		{ "Trail.DisplayName", "Trail / \xe6\x8b\x96\xe5\xb0\xbe\xe5\x9e\x8b" },
		{ "Trail.Name", "EGamePlatformVFXBehavior::Trail" },
		{ "World.DisplayName", "World / \xe4\xb8\x96\xe7\x95\x8c\xe7\x8e\xaf\xe5\xa2\x83\xe5\x9e\x8b" },
		{ "World.Name", "EGamePlatformVFXBehavior::World" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXBehavior::Instant", (int64)EGamePlatformVFXBehavior::Instant },
		{ "EGamePlatformVFXBehavior::Attached", (int64)EGamePlatformVFXBehavior::Attached },
		{ "EGamePlatformVFXBehavior::Projectile", (int64)EGamePlatformVFXBehavior::Projectile },
		{ "EGamePlatformVFXBehavior::Beam", (int64)EGamePlatformVFXBehavior::Beam },
		{ "EGamePlatformVFXBehavior::Area", (int64)EGamePlatformVFXBehavior::Area },
		{ "EGamePlatformVFXBehavior::Shield", (int64)EGamePlatformVFXBehavior::Shield },
		{ "EGamePlatformVFXBehavior::Portal", (int64)EGamePlatformVFXBehavior::Portal },
		{ "EGamePlatformVFXBehavior::Trail", (int64)EGamePlatformVFXBehavior::Trail },
		{ "EGamePlatformVFXBehavior::World", (int64)EGamePlatformVFXBehavior::World },
		{ "EGamePlatformVFXBehavior::Composite", (int64)EGamePlatformVFXBehavior::Composite },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXBehavior",
	"EGamePlatformVFXBehavior",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXBehavior;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXBehavior.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXBehavior.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXBehavior"));
		}
		return ZRIE_EGamePlatformVFXBehavior.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXBehavior.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXBehavior.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXBehavior.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXBehavior ****************************************************

// ********** Begin Enum EGamePlatformVFXCatalogScope **********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXCatalogScope>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Catalog \xe6\x9d\xa5\xe6\xba\x90\xe4\xbd\x9c\xe7\x94\xa8\xe5\x9f\x9f\xef\xbc\x9b\xe7\x94\xa8\xe4\xba\x8e\xe7\xa1\xae\xe5\xae\x9a\xe6\x80\xa7\xe8\xa6\x86\xe7\x9b\x96\xe8\xa7\x84\xe5\x88\x99\xef\xbc\x8c\xe4\xb8\x8d\xe7\xad\x89\xe4\xba\x8e\xe6\x8f\x92\xe4\xbb\xb6\xe7\x89\xa9\xe7\x90\x86\xe8\xb7\xaf\xe5\xbe\x84\xe3\x80\x82 */" },
#endif
		{ "ContentPack.DisplayName", "ContentPack / \xe5\x86\x85\xe5\xae\xb9\xe5\x8c\x85" },
		{ "ContentPack.Name", "EGamePlatformVFXCatalogScope::ContentPack" },
		{ "Moba.DisplayName", "Moba / MOBA\xe9\x80\x9a\xe7\x94\xa8" },
		{ "Moba.Name", "EGamePlatformVFXCatalogScope::Moba" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "Platform.DisplayName", "Platform / \xe5\xb9\xb3\xe5\x8f\xb0\xe9\xbb\x98\xe8\xae\xa4" },
		{ "Platform.Name", "EGamePlatformVFXCatalogScope::Platform" },
		{ "Project.DisplayName", "Project / \xe9\xa1\xb9\xe7\x9b\xae\xe9\xbb\x98\xe8\xae\xa4" },
		{ "Project.Name", "EGamePlatformVFXCatalogScope::Project" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Catalog \xe6\x9d\xa5\xe6\xba\x90\xe4\xbd\x9c\xe7\x94\xa8\xe5\x9f\x9f\xef\xbc\x9b\xe7\x94\xa8\xe4\xba\x8e\xe7\xa1\xae\xe5\xae\x9a\xe6\x80\xa7\xe8\xa6\x86\xe7\x9b\x96\xe8\xa7\x84\xe5\x88\x99\xef\xbc\x8c\xe4\xb8\x8d\xe7\xad\x89\xe4\xba\x8e\xe6\x8f\x92\xe4\xbb\xb6\xe7\x89\xa9\xe7\x90\x86\xe8\xb7\xaf\xe5\xbe\x84\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXCatalogScope::Platform", (int64)EGamePlatformVFXCatalogScope::Platform },
		{ "EGamePlatformVFXCatalogScope::Moba", (int64)EGamePlatformVFXCatalogScope::Moba },
		{ "EGamePlatformVFXCatalogScope::Project", (int64)EGamePlatformVFXCatalogScope::Project },
		{ "EGamePlatformVFXCatalogScope::ContentPack", (int64)EGamePlatformVFXCatalogScope::ContentPack },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXCatalogScope",
	"EGamePlatformVFXCatalogScope",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXCatalogScope;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXCatalogScope.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXCatalogScope.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXCatalogScope"));
		}
		return ZRIE_EGamePlatformVFXCatalogScope.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXCatalogScope.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXCatalogScope.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXCatalogScope.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXCatalogScope ************************************************

// ********** Begin Enum EGamePlatformVFXLifecycleState ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXLifecycleState>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Active.Name", "EGamePlatformVFXLifecycleState::Active" },
		{ "BlueprintType", "true" },
		{ "Cancelled.Name", "EGamePlatformVFXLifecycleState::Cancelled" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** VFX \xe7\x94\x9f\xe5\x91\xbd\xe5\x91\xa8\xe6\x9c\x9f\xe3\x80\x82 */" },
#endif
		{ "Completed.Name", "EGamePlatformVFXLifecycleState::Completed" },
		{ "Failed.Name", "EGamePlatformVFXLifecycleState::Failed" },
		{ "Invalid.Name", "EGamePlatformVFXLifecycleState::Invalid" },
		{ "Loading.Name", "EGamePlatformVFXLifecycleState::Loading" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "Requested.Name", "EGamePlatformVFXLifecycleState::Requested" },
		{ "Spawning.Name", "EGamePlatformVFXLifecycleState::Spawning" },
		{ "Stopping.Name", "EGamePlatformVFXLifecycleState::Stopping" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe7\x94\x9f\xe5\x91\xbd\xe5\x91\xa8\xe6\x9c\x9f\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXLifecycleState::Invalid", (int64)EGamePlatformVFXLifecycleState::Invalid },
		{ "EGamePlatformVFXLifecycleState::Requested", (int64)EGamePlatformVFXLifecycleState::Requested },
		{ "EGamePlatformVFXLifecycleState::Loading", (int64)EGamePlatformVFXLifecycleState::Loading },
		{ "EGamePlatformVFXLifecycleState::Spawning", (int64)EGamePlatformVFXLifecycleState::Spawning },
		{ "EGamePlatformVFXLifecycleState::Active", (int64)EGamePlatformVFXLifecycleState::Active },
		{ "EGamePlatformVFXLifecycleState::Stopping", (int64)EGamePlatformVFXLifecycleState::Stopping },
		{ "EGamePlatformVFXLifecycleState::Completed", (int64)EGamePlatformVFXLifecycleState::Completed },
		{ "EGamePlatformVFXLifecycleState::Cancelled", (int64)EGamePlatformVFXLifecycleState::Cancelled },
		{ "EGamePlatformVFXLifecycleState::Failed", (int64)EGamePlatformVFXLifecycleState::Failed },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXLifecycleState",
	"EGamePlatformVFXLifecycleState",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXLifecycleState;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXLifecycleState.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXLifecycleState.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXLifecycleState"));
		}
		return ZRIE_EGamePlatformVFXLifecycleState.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXLifecycleState.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXLifecycleState.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXLifecycleState.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXLifecycleState **********************************************

// ********** Begin Enum EGamePlatformVFXPlayResultCode ********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXPlayResultCode>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Accepted.Name", "EGamePlatformVFXPlayResultCode::Accepted" },
		{ "BlueprintType", "true" },
		{ "BudgetRejected.Name", "EGamePlatformVFXPlayResultCode::BudgetRejected" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** VFX \xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe5\x90\x8c\xe6\xad\xa5\xe6\x8e\xa5\xe6\x94\xb6\xe7\xbb\x93\xe6\x9e\x9c\xe3\x80\x82\xe5\xbc\x82\xe6\xad\xa5\xe5\x8a\xa0\xe8\xbd\xbd\xe5\xa4\xb1\xe8\xb4\xa5\xe9\x80\x9a\xe8\xbf\x87\xe5\xae\x9e\xe4\xbe\x8b\xe7\x8a\xb6\xe6\x80\x81\xe5\x92\x8c\xe8\xaf\x8a\xe6\x96\xad\xe7\xb3\xbb\xe7\xbb\x9f\xe4\xbd\x93\xe7\x8e\xb0\xe3\x80\x82 */" },
#endif
		{ "InvalidRequest.Name", "EGamePlatformVFXPlayResultCode::InvalidRequest" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "ResolveFailed.Name", "EGamePlatformVFXPlayResultCode::ResolveFailed" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "VFX \xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe5\x90\x8c\xe6\xad\xa5\xe6\x8e\xa5\xe6\x94\xb6\xe7\xbb\x93\xe6\x9e\x9c\xe3\x80\x82\xe5\xbc\x82\xe6\xad\xa5\xe5\x8a\xa0\xe8\xbd\xbd\xe5\xa4\xb1\xe8\xb4\xa5\xe9\x80\x9a\xe8\xbf\x87\xe5\xae\x9e\xe4\xbe\x8b\xe7\x8a\xb6\xe6\x80\x81\xe5\x92\x8c\xe8\xaf\x8a\xe6\x96\xad\xe7\xb3\xbb\xe7\xbb\x9f\xe4\xbd\x93\xe7\x8e\xb0\xe3\x80\x82" },
#endif
		{ "WorldUnavailable.Name", "EGamePlatformVFXPlayResultCode::WorldUnavailable" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXPlayResultCode::Accepted", (int64)EGamePlatformVFXPlayResultCode::Accepted },
		{ "EGamePlatformVFXPlayResultCode::InvalidRequest", (int64)EGamePlatformVFXPlayResultCode::InvalidRequest },
		{ "EGamePlatformVFXPlayResultCode::ResolveFailed", (int64)EGamePlatformVFXPlayResultCode::ResolveFailed },
		{ "EGamePlatformVFXPlayResultCode::WorldUnavailable", (int64)EGamePlatformVFXPlayResultCode::WorldUnavailable },
		{ "EGamePlatformVFXPlayResultCode::BudgetRejected", (int64)EGamePlatformVFXPlayResultCode::BudgetRejected },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXPlayResultCode",
	"EGamePlatformVFXPlayResultCode",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXPlayResultCode;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXPlayResultCode.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXPlayResultCode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXPlayResultCode"));
		}
		return ZRIE_EGamePlatformVFXPlayResultCode.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXPlayResultCode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXPlayResultCode.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXPlayResultCode.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXPlayResultCode **********************************************

// ********** Begin Enum EGamePlatformVFXImportance ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXImportance>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Ambient.Name", "EGamePlatformVFXImportance::Ambient" },
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe8\xa1\xa8\xe7\x8e\xb0\xe9\x87\x8d\xe8\xa6\x81\xe7\xad\x89\xe7\xba\xa7\xef\xbc\x9b\xe7\x94\xa8\xe4\xba\x8e\xe6\xa1\xa5\xe6\x8e\xa5 Niagara Scalability\xef\xbc\x8c\xe8\x80\x8c\xe4\xb8\x8d\xe6\x98\xaf\xe5\x8f\xa6\xe9\x80\xa0\xe4\xb8\x80\xe5\xa5\x97\xe6\xb8\xb2\xe6\x9f\x93\xe9\xa2\x84\xe7\xae\x97\xe7\xb3\xbb\xe7\xbb\x9f\xe3\x80\x82 */" },
#endif
		{ "Critical.Name", "EGamePlatformVFXImportance::Critical" },
		{ "High.Name", "EGamePlatformVFXImportance::High" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "Normal.Name", "EGamePlatformVFXImportance::Normal" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe8\xa1\xa8\xe7\x8e\xb0\xe9\x87\x8d\xe8\xa6\x81\xe7\xad\x89\xe7\xba\xa7\xef\xbc\x9b\xe7\x94\xa8\xe4\xba\x8e\xe6\xa1\xa5\xe6\x8e\xa5 Niagara Scalability\xef\xbc\x8c\xe8\x80\x8c\xe4\xb8\x8d\xe6\x98\xaf\xe5\x8f\xa6\xe9\x80\xa0\xe4\xb8\x80\xe5\xa5\x97\xe6\xb8\xb2\xe6\x9f\x93\xe9\xa2\x84\xe7\xae\x97\xe7\xb3\xbb\xe7\xbb\x9f\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXImportance::Critical", (int64)EGamePlatformVFXImportance::Critical },
		{ "EGamePlatformVFXImportance::High", (int64)EGamePlatformVFXImportance::High },
		{ "EGamePlatformVFXImportance::Normal", (int64)EGamePlatformVFXImportance::Normal },
		{ "EGamePlatformVFXImportance::Ambient", (int64)EGamePlatformVFXImportance::Ambient },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXImportance",
	"EGamePlatformVFXImportance",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXImportance;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXImportance.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXImportance.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXImportance"));
		}
		return ZRIE_EGamePlatformVFXImportance.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXImportance.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXImportance.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXImportance.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXImportance **************************************************

// ********** Begin Enum EGamePlatformVFXPoolingMode ***********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode_Statics
template<> GAMEPLATFORMVFXCLIENT_NON_ATTRIBUTED_API UEnum* StaticEnum<EGamePlatformVFXPoolingMode>()
{
	return Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "AutoRelease.Name", "EGamePlatformVFXPoolingMode::AutoRelease" },
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe5\xb9\xb3\xe5\x8f\xb0\xe6\x8a\xbd\xe8\xb1\xa1\xe6\xb1\xa0\xe5\x8c\x96\xe6\xa8\xa1\xe5\xbc\x8f\xef\xbc\x8c\xe9\x81\xbf\xe5\x85\x8d\xe5\xb0\x86 Niagara \xe7\x9a\x84\xe5\x85\xb7\xe4\xbd\x93\xe6\x9e\x9a\xe4\xb8\xbe\xe6\xb3\x84\xe6\xbc\x8f\xe5\x88\xb0\xe9\xab\x98\xe5\xb1\x82\xe8\xaf\xb7\xe6\xb1\x82\xe7\xbb\x93\xe6\x9e\x84\xe3\x80\x82 */" },
#endif
		{ "ManualRelease.Name", "EGamePlatformVFXPoolingMode::ManualRelease" },
		{ "ModuleRelativePath", "Public/Types/GamePlatformVFXTypes.h" },
		{ "None.Name", "EGamePlatformVFXPoolingMode::None" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe5\xb9\xb3\xe5\x8f\xb0\xe6\x8a\xbd\xe8\xb1\xa1\xe6\xb1\xa0\xe5\x8c\x96\xe6\xa8\xa1\xe5\xbc\x8f\xef\xbc\x8c\xe9\x81\xbf\xe5\x85\x8d\xe5\xb0\x86 Niagara \xe7\x9a\x84\xe5\x85\xb7\xe4\xbd\x93\xe6\x9e\x9a\xe4\xb8\xbe\xe6\xb3\x84\xe6\xbc\x8f\xe5\x88\xb0\xe9\xab\x98\xe5\xb1\x82\xe8\xaf\xb7\xe6\xb1\x82\xe7\xbb\x93\xe6\x9e\x84\xe3\x80\x82" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGamePlatformVFXPoolingMode::None", (int64)EGamePlatformVFXPoolingMode::None },
		{ "EGamePlatformVFXPoolingMode::AutoRelease", (int64)EGamePlatformVFXPoolingMode::AutoRelease },
		{ "EGamePlatformVFXPoolingMode::ManualRelease", (int64)EGamePlatformVFXPoolingMode::ManualRelease },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GamePlatformVFXClient,
	nullptr,
	"EGamePlatformVFXPoolingMode",
	"EGamePlatformVFXPoolingMode",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGamePlatformVFXPoolingMode;
UEnum* Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGamePlatformVFXPoolingMode.OuterSingleton)
		{
			ZRIE_EGamePlatformVFXPoolingMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode, (UObject*)Z_Construct_UPackage__Script_GamePlatformVFXClient(ETypeConstructPhase::Outer), TEXT("EGamePlatformVFXPoolingMode"));
		}
		return ZRIE_EGamePlatformVFXPoolingMode.OuterSingleton;
	}
	if (!ZRIE_EGamePlatformVFXPoolingMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGamePlatformVFXPoolingMode.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGamePlatformVFXPoolingMode.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGamePlatformVFXPoolingMode *************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXTypes_h__Script_GamePlatformVFXClient_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXBehavior, TEXT("EGamePlatformVFXBehavior"), &ZRIE_EGamePlatformVFXBehavior, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4198316035U) },
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXCatalogScope, TEXT("EGamePlatformVFXCatalogScope"), &ZRIE_EGamePlatformVFXCatalogScope, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1807514498U) },
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXLifecycleState, TEXT("EGamePlatformVFXLifecycleState"), &ZRIE_EGamePlatformVFXLifecycleState, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 900685002U) },
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPlayResultCode, TEXT("EGamePlatformVFXPlayResultCode"), &ZRIE_EGamePlatformVFXPlayResultCode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 11419189U) },
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXImportance, TEXT("EGamePlatformVFXImportance"), &ZRIE_EGamePlatformVFXImportance, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1030792000U) },
		{ Z_Construct_UEnum_GamePlatformVFXClient_EGamePlatformVFXPoolingMode, TEXT("EGamePlatformVFXPoolingMode"), &ZRIE_EGamePlatformVFXPoolingMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3791695518U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Game_Plugins_GameFoundation_Presentation_GamePlatformVFX_Source_GamePlatformVFXClient_Public_Types_GamePlatformVFXTypes_h__Script_GamePlatformVFXClient_68b75218d03e82234b97ca779f4086f49e447d68{
	TEXT("/Script/GamePlatformVFXClient"),
	nullptr, 0,
	nullptr, 0,
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
