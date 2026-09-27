#include "Definitions/GamePlatformWorldDefinition.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Validation/GamePlatformWorldDefinitionValidator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// 真实注册表中的隔离内存定义；地图引用引擎已保存Entry，不保存或改写用户资产。
struct FWorldDefinitionEditorFixture
{
    FString Prefix = TEXT("worldtest.n") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<TStrongObjectPtr<UGamePlatformDefinitionBase>> Objects;
    template<typename T> T* Add(const TCHAR* Name)
    {
        UPackage* Package = CreatePackage(*(TEXT("/Game/__WorldDefinitionTests/") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetPackageFlags(PKG_Transient);
        T* Object = NewObject<T>(Package, TEXT("Definition"), RF_Public | RF_Standalone);
        FGamePlatformId::TryParse(Prefix + TEXT(".") + Name + TEXT("@1"), Object->LogicalId);
        if (auto* Region = Cast<UGamePlatformRegionDefinition>(Object)) { Region->RegionTypeTag = TEXT("NeutralArea"); }
        if (auto* World = Cast<UGamePlatformWorldDefinition>(Object))
        { World->MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry"))); }
        Objects.Emplace(Object);
        FAssetRegistryModule::AssetCreated(Object);
        return Object;
    }
    ~FWorldDefinitionEditorFixture()
    {
        for (auto& Object : Objects)
        {
            FAssetRegistryModule::AssetDeleted(Object.Get());
            Object->ClearFlags(RF_Public | RF_Standalone); Object->MarkAsGarbage();
        }
    }
};
EDataValidationResult ValidateWorldAsset(UGamePlatformDefinitionBase* Object, TArray<FText>* OutErrors = nullptr)
{
    TStrongObjectPtr<UGamePlatformWorldDefinitionValidator> Validator(NewObject<UGamePlatformWorldDefinitionValidator>());
    FDataValidationContext Context(false, EDataValidationUsecase::Commandlet, {});
    const auto Result = Validator->ValidateLoadedAsset(FAssetData(Object), Object, Context);
    if (OutErrors) { TArray<FText> Warnings; OutErrors->Reset(); Context.SplitIssues(Warnings, *OutErrors); }
    return Result;
}
}

// 缺失地图/非World资源绝不能因软引用字符串语法正确而成为有效世界。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEditorMapValidationTest, "GamePlatform.World.Editor.RealMapValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldEditorMapValidationTest::RunTest(const FString&)
{
    FWorldDefinitionEditorFixture Fixture;
    auto* World = Fixture.Add<UGamePlatformWorldDefinition>(TEXT("world"));
    TestTrue(TEXT("真实引擎Entry地图通过"), ValidateWorldAsset(World) == EDataValidationResult::Valid);
    World->MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/__WorldDefinitionTests/Missing.Missing")));
    TestTrue(TEXT("不存在的地图失败"), ValidateWorldAsset(World) == EDataValidationResult::Invalid);
    World->MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")));
    TArray<FText> Errors;
    TestTrue(TEXT("真实非World资产不能冒充地图"), ValidateWorldAsset(World, &Errors) == EDataValidationResult::Invalid);
    TestTrue(TEXT("负例确由真实资源错类触发而非缺少测试夹具"), Errors.ContainsByPredicate([](const FText& Error)
    { return Error.ToString().Contains(TEXT("WorldMapTypeMismatch")); }));
    return true;
}

// 错类型、未找到区域及父链间接环需读取真实源对象，不能只检查ID字符串。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEditorRegionGraphTest, "GamePlatform.World.Editor.RegionGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldEditorRegionGraphTest::RunTest(const FString&)
{
    FWorldDefinitionEditorFixture Fixture;
    auto* World = Fixture.Add<UGamePlatformWorldDefinition>(TEXT("world"));
    auto* A = Fixture.Add<UGamePlatformRegionDefinition>(TEXT("a"));
    auto* B = Fixture.Add<UGamePlatformRegionDefinition>(TEXT("b"));
    World->Regions = {A->GetPrimaryAssetId(), B->GetPrimaryAssetId()}; World->RequiredDefinitions = World->Regions;
    A->ParentRegionId = B->LogicalId;
    TestTrue(TEXT("真实区域与单向父链有效"), ValidateWorldAsset(World) == EDataValidationResult::Valid);
    B->ParentRegionId = A->LogicalId;
    TArray<FText> Errors;
    TestTrue(TEXT("世界验证检出间接父环"), ValidateWorldAsset(World, &Errors) == EDataValidationResult::Invalid);
    TestTrue(TEXT("确由父链循环而非通用依赖失败"), Errors.ContainsByPredicate([](const FText& Error)
    { return Error.ToString().Contains(TEXT("RegionParentCycle")); }));
    TestTrue(TEXT("独立区域验证也检出父环"), ValidateWorldAsset(A) == EDataValidationResult::Invalid);
    B->ParentRegionId = {};
    auto* WrongType = Fixture.Add<UGamePlatformDefinitionBase>(TEXT("notregion"));
    World->Regions = {WrongType->GetPrimaryAssetId()}; World->RequiredDefinitions = World->Regions;
    TestTrue(TEXT("其他Data定义不是Region"), ValidateWorldAsset(World) == EDataValidationResult::Invalid);
    World->Regions = {FPrimaryAssetId(TEXT("GamePlatformDefinition"), FName(*(Fixture.Prefix + TEXT(".missing@1"))))};
    World->RequiredDefinitions = World->Regions;
    TestTrue(TEXT("区域源缺失失败"), ValidateWorldAsset(World) == EDataValidationResult::Invalid);
    A->ParentRegionId = WrongType->LogicalId;
    TestTrue(TEXT("父对象也必须是Region"), ValidateWorldAsset(A) == EDataValidationResult::Invalid);
    return true;
}

// 如果以主资产去重字典替代公开源扫描，相同逻辑身份的两个不同物理区域会被漏检。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEditorRegionDuplicateTest, "GamePlatform.World.Editor.DuplicateRegionSources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldEditorRegionDuplicateTest::RunTest(const FString&)
{
    FWorldDefinitionEditorFixture Fixture;
    auto* A = Fixture.Add<UGamePlatformRegionDefinition>(TEXT("duplicate"));
    TestTrue(TEXT("唯一真实源有效"), ValidateWorldAsset(A) == EDataValidationResult::Valid);
    Fixture.Add<UGamePlatformRegionDefinition>(TEXT("duplicate"));
    TArray<FText> Errors;
    TestTrue(TEXT("两个不同源占用同一RegionId失败"), ValidateWorldAsset(A, &Errors) == EDataValidationResult::Invalid);
    TestTrue(TEXT("报告源身份歧义"), Errors.ContainsByPredicate([](const FText& Error)
    { return Error.ToString().Contains(TEXT("DuplicateLogicalId")); }));
    return true;
}

// 有界迭代必须拒绝超深父链，同时不能把128层以内的有效链一概拒绝。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEditorRegionDepthTest, "GamePlatform.World.Editor.ParentDepthLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldEditorRegionDepthTest::RunTest(const FString&)
{
    FWorldDefinitionEditorFixture Fixture;
    auto* Root = Fixture.Add<UGamePlatformRegionDefinition>(TEXT("root"));
    auto* Previous = Root;
    UGamePlatformRegionDefinition* LastAllowed = nullptr;
    for (int32 Index = 1; Index < 129; ++Index)
    {
        auto* Next = Fixture.Add<UGamePlatformRegionDefinition>(*FString::Printf(TEXT("region%d"), Index));
        Previous->ParentRegionId = Next->LogicalId;
        if (Index == 128) { LastAllowed = Previous; }
        Previous = Next;
    }
    TArray<FText> Errors;
    TestTrue(TEXT("129层父链明确失败"), ValidateWorldAsset(Root, &Errors) == EDataValidationResult::Invalid);
    TestTrue(TEXT("报告图上限而不是崩溃或错类"), Errors.ContainsByPredicate([](const FText& Error)
    { return Error.ToString().Contains(TEXT("RegionParentGraphLimit")); }));
    LastAllowed->ParentRegionId = {};
    TestTrue(TEXT("128层仍然有效"), ValidateWorldAsset(Root) == EDataValidationResult::Valid);
    return true;
}

// 若编辑器模块/自动登记断开，直接NewObject验证器的单测不能证明集成；此处必须走真实子系统。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEditorValidatorDiscoveryTest, "GamePlatform.World.Editor.ValidatorDiscovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldEditorValidatorDiscoveryTest::RunTest(const FString&)
{
    auto* Validation = GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
    if (!Validation) { AddError(TEXT("真实编辑器验证子系统不存在")); return false; }
    bool bFound = false;
    Validation->ForEachEnabledValidator([&](UEditorValidatorBase* Validator)
    { bFound |= Validator->IsA<UGamePlatformWorldDefinitionValidator>(); return true; });
    if (!TestTrue(TEXT("世界验证器由编辑器实际发现"), bFound)) { return false; }
    FWorldDefinitionEditorFixture Fixture;
    auto* World = Fixture.Add<UGamePlatformWorldDefinition>(TEXT("invalidmap"));
    World->MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/__WorldDefinitionTests/Missing.Missing")));
    TArray<FText> Errors, Warnings;
    TestTrue(TEXT("真实调度拒绝缺失地图"), Validation->IsObjectValid(World, Errors, Warnings,
        EDataValidationUsecase::Commandlet) == EDataValidationResult::Invalid);
    TestTrue(TEXT("世界验证器提供可检索原因"), Errors.ContainsByPredicate([](const FText& Error)
    { return Error.ToString().Contains(TEXT("WorldMapMissing")); }));
    return true;
}
#endif
