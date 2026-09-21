#include "Validation/GamePlatformDefinitionValidator.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Validation/GamePlatformDefinitionValidation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** 隔离内存包夹具：真实UObject和真实注册表标签，不写磁盘、不修改已有资产。 */
struct FDefinitionFixture
{
    TArray<TStrongObjectPtr<UGamePlatformDefinitionBase>> Definitions;
    UGamePlatformDefinitionBase* Add(const FString& LogicalName)
    {
        UPackage* Package = CreatePackage(*(TEXT("/Game/__GamePlatformDataTests/") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetPackageFlags(PKG_Transient);
        auto* Definition = NewObject<UGamePlatformDefinitionBase>(Package, TEXT("Definition"), RF_Public | RF_Standalone);
        FGamePlatformId::TryParse(TEXT("test.") + LogicalName + TEXT("@1"), Definition->LogicalId);
        Definitions.Emplace(Definition);
        FAssetRegistryModule::AssetCreated(Definition);
        return Definition;
    }
    ~FDefinitionFixture()
    {
        for (auto& Definition : Definitions)
        {
            FAssetRegistryModule::AssetDeleted(Definition.Get());
            Definition->ClearFlags(RF_Public | RF_Standalone);
            Definition->MarkAsGarbage();
        }
    }
};
EDataValidationResult Validate(UGamePlatformDefinitionBase* Definition)
{
    TStrongObjectPtr<UGamePlatformDefinitionValidator> Validator(NewObject<UGamePlatformDefinitionValidator>());
    FDataValidationContext Context(false, EDataValidationUsecase::Commandlet, {});
    // 调用编辑器实际入口，验证器若未执行应返回NotValidated而使这些断言失败。
    return Validator->ValidateLoadedAsset(FAssetData(Definition), Definition, Context);
}
}

// 若扫描改成主资产去重字典，两个不同源对象占用同一逻辑身份的负例不能再被检出。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSourceDuplicateTest, "GamePlatform.Data.Editor.SourceDuplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSourceDuplicateTest::RunTest(const FString& Parameters)
{
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
    FDefinitionFixture Fixture;
    auto* First = Fixture.Add(TEXT("duplicate"));
    TestTrue(TEXT("唯一源有效"), Validate(First) == EDataValidationResult::Valid);
    auto* Second = Fixture.Add(TEXT("duplicate"));
    TestTrue(TEXT("不同物理源身份冲突"), Validate(First) == EDataValidationResult::Invalid);
    TestTrue(TEXT("冲突双方均失败"), Validate(Second) == EDataValidationResult::Invalid);
    return true;
}

// 若只检查RequiredDefinitions字段格式而不遍历真实注册表对象，缺失、间接环及子版本错误会漏检。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSourceDependenciesTest, "GamePlatform.Data.Editor.DependencyGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSourceDependenciesTest::RunTest(const FString& Parameters)
{
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
    FDefinitionFixture Fixture;
    auto* Root = Fixture.Add(TEXT("root"));
    auto* Child = Fixture.Add(TEXT("child"));
    Root->RequiredDefinitions = {Child->GetPrimaryAssetId()};
    TestTrue(TEXT("真实依赖对象通过"), Validate(Root) == EDataValidationResult::Valid);
    Child->RequiredDefinitions = {Root->GetPrimaryAssetId()};
    TestTrue(TEXT("间接依赖环失败"), Validate(Root) == EDataValidationResult::Invalid);
    Child->RequiredDefinitions.Empty();
    Child->DataVersion.SchemaVersion = 200;
    TestTrue(TEXT("子依赖结构版本非法"), Validate(Root) == EDataValidationResult::Invalid);
    Child->DataVersion.SchemaVersion = 1;
    Child->DataVersion.ContentRevision = 0;
    TestTrue(TEXT("子依赖内容修订非法"), Validate(Root) == EDataValidationResult::Invalid);
    Root->RequiredDefinitions = {FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("test.missing@1"))};
    TestTrue(TEXT("缺失依赖失败"), Validate(Root) == EDataValidationResult::Invalid);
    Root->RequiredDefinitions = {Root->GetPrimaryAssetId()};
    TestTrue(TEXT("自引用失败"), Validate(Root) == EDataValidationResult::Invalid);
    Root->RequiredDefinitions.Empty();
    Root->LogicalId.Name.Empty();
    TestTrue(TEXT("必填身份非法"), Validate(Root) == EDataValidationResult::Invalid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSourceDepthTest, "GamePlatform.Data.Editor.DependencyDepthLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSourceDepthTest::RunTest(const FString& Parameters)
{
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
    FDefinitionFixture Fixture;
    auto* Root = Fixture.Add(TEXT("depth0"));
    auto* Previous = Root;
    for (int32 Index = 1; Index < 129; ++Index)
    {
        auto* Next = Fixture.Add(FString::Printf(TEXT("depth%d"), Index));
        Previous->RequiredDefinitions = {Next->GetPrimaryAssetId()};
        Previous = Next;
    }
    TestTrue(TEXT("129层依赖返回Invalid而不无限递归"), Validate(Root) == EDataValidationResult::Invalid);
    return true;
}
#endif
