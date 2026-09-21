#include "Definitions/GamePlatformWorldDefinition.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FGamePlatformId WorldTestId(const TCHAR* Text)
{
    FGamePlatformId Id;
    FGamePlatformId::TryParse(Text, Id);
    return Id;
}
void ConfigureWorldDefinition(UGamePlatformWorldDefinition& Definition)
{
    Definition.LogicalId = WorldTestId(TEXT("test.world@1"));
    Definition.MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/World.World")));
}
}

// 若派生验证遗漏Super，非法身份/结构版本将被错误接受；这里只创建真实瞬态定义，不伪造地图存在。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDefinitionBaseValidationTest, "GamePlatform.World.Definitions.BaseValidation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FWorldDefinitionBaseValidationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGamePlatformWorldDefinition> World(NewObject<UGamePlatformWorldDefinition>());
    ConfigureWorldDefinition(*World);
    TestTrue(TEXT("字段完整的定义通过无加载校验"), World->ValidateDefinition().IsSuccess());
    World->DataVersion.SchemaVersion = 999;
    TestEqual(TEXT("世界沿用Data版本门禁"), World->ValidateDefinition().Code, FName(TEXT("UnsupportedDataVersion")));
    World->DataVersion.SchemaVersion = 1;
    World->LogicalId = {};
    TestEqual(TEXT("世界身份只能来自LogicalId"), World->ValidateDefinition().Code, FName(TEXT("InvalidLogicalId")));
    TStrongObjectPtr<UGamePlatformRegionDefinition> Region(NewObject<UGamePlatformRegionDefinition>());
    Region->LogicalId = WorldTestId(TEXT("test.region@1")); Region->RegionTypeTag = TEXT("NeutralArea");
    TestTrue(TEXT("中立区域有效"), Region->ValidateDefinition().IsSuccess());
    Region->DataVersion.ContentRevision = 0;
    TestEqual(TEXT("区域沿用内容修订门禁"), Region->ValidateDefinition().Code, FName(TEXT("UnsupportedDataVersion")));
    return true;
}

// 删除地图路径、有限正数截止或可选ID校验，分别会让以下边界误报成功。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDefinitionMapAndTimeoutTest, "GamePlatform.World.Definitions.MapAndTimeout",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FWorldDefinitionMapAndTimeoutTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGamePlatformWorldDefinition> World(NewObject<UGamePlatformWorldDefinition>());
    ConfigureWorldDefinition(*World);
    World->MapIdentity.Reset();
    TestEqual(TEXT("缺少地图身份"), World->ValidateDefinition().Code, FName(TEXT("InvalidMapIdentity")));
    World->MapIdentity = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/World.World:PersistentLevel")));
    TestEqual(TEXT("不能把地图子对象当世界资产"), World->ValidateDefinition().Code, FName(TEXT("InvalidMapIdentity")));
    ConfigureWorldDefinition(*World);
    for (float Timeout : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        World->ReadinessTimeoutSeconds = Timeout;
        TestEqual(TEXT("非法截止拒绝"), World->ValidateDefinition().Code, FName(TEXT("InvalidReadinessTimeout")));
    }
    World->ReadinessTimeoutSeconds = 60.f;
    World->DefaultExperienceId.Name = TEXT("partial");
    TestEqual(TEXT("半填体验不是可选空值"), World->ValidateDefinition().Code, FName(TEXT("InvalidDefaultExperienceId")));
    World->DefaultExperienceId = {};
    TestTrue(TEXT("体验可明确留空"), World->ValidateDefinition().IsSuccess());
    return true;
}

// Regions必须进入真实Data依赖闭包；只写区域列表但不登记租约需求不能通过。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDefinitionRegionDependenciesTest, "GamePlatform.World.Definitions.RegionDependencies",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FWorldDefinitionRegionDependenciesTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGamePlatformWorldDefinition> World(NewObject<UGamePlatformWorldDefinition>());
    ConfigureWorldDefinition(*World);
    const FPrimaryAssetId Region(TEXT("GamePlatformDefinition"), TEXT("test.region@1"));
    World->Regions = {Region};
    TestEqual(TEXT("区域未纳入Data依赖"), World->ValidateDefinition().Code, FName(TEXT("RegionDependencyMissing")));
    World->RequiredDefinitions = {Region};
    TestTrue(TEXT("纳入租约闭包后通过"), World->ValidateDefinition().IsSuccess());
    World->Regions.Add(Region);
    TestEqual(TEXT("重复区域拒绝"), World->ValidateDefinition().Code, FName(TEXT("DuplicateRegion")));
    World->Regions = {FPrimaryAssetId(TEXT("OtherType"), TEXT("test.region@1"))};
    TestEqual(TEXT("区域必须使用Data主资产类型"), World->ValidateDefinition().Code, FName(TEXT("InvalidRegionId")));
    World->Regions = {World->GetPrimaryAssetId()};
    TestEqual(TEXT("世界不能把自己当区域"), World->ValidateDefinition().Code, FName(TEXT("InvalidRegionId")));
    return true;
}

// 当前只实现明确的一种边界/激活策略；未知序列化枚举不能悄悄退回默认策略。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegionDefinitionPolicyTest, "GamePlatform.World.Definitions.RegionPolicies",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRegionDefinitionPolicyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGamePlatformRegionDefinition> Region(NewObject<UGamePlatformRegionDefinition>());
    Region->LogicalId = WorldTestId(TEXT("test.region@1"));
    TestEqual(TEXT("区域语义必填"), Region->ValidateDefinition().Code, FName(TEXT("RegionTypeMissing")));
    Region->RegionTypeTag = TEXT("NeutralArea");
    Region->ParentRegionId = Region->LogicalId;
    TestEqual(TEXT("区域不能自为父级"), Region->ValidateDefinition().Code, FName(TEXT("RegionParentSelfReference")));
    Region->ParentRegionId = {}; Region->ParentRegionId.Name = TEXT("partial");
    TestEqual(TEXT("半填父身份非法"), Region->ValidateDefinition().Code, FName(TEXT("InvalidParentRegionId")));
    Region->ParentRegionId = {};
    Region->BoundsPolicy = static_cast<EGamePlatformRegionBoundsPolicy>(255);
    TestEqual(TEXT("未知边界不假装支持"), Region->ValidateDefinition().Code, FName(TEXT("UnsupportedRegionBoundsPolicy")));
    Region->BoundsPolicy = EGamePlatformRegionBoundsPolicy::AxisAlignedBox;
    Region->ActivationPolicy = static_cast<EGamePlatformRegionActivationPolicy>(255);
    TestEqual(TEXT("未知激活不假装支持"), Region->ValidateDefinition().Code, FName(TEXT("UnsupportedRegionActivationPolicy")));
    Region->ActivationPolicy = EGamePlatformRegionActivationPolicy::AlwaysRegistered;
    TestTrue(TEXT("已实现策略通过"), Region->ValidateDefinition().IsSuccess());
    return true;
}
#endif
