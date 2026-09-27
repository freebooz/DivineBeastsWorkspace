#include "Registry/GamePlatformDebugRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace
{
    class FGamePlatformDebugUnitProvider final
        : public IGamePlatformDebugStateProvider
    {
    public:
        virtual FName GetProviderId() const override
        {
            return TEXT("Unit.Provider");
        }

        virtual bool CanCollect(
            const FGamePlatformDebugCollectContext&) const override
        {
            return true;
        }

        virtual bool CollectSnapshot(
            const FGamePlatformDebugCollectContext&,
            FGamePlatformDebugSnapshot& OutSnapshot) const override
        {
            OutSnapshot.AddField(
                TEXT("Unit"),
                TEXT("单元测试"),
                TEXT("ok"));
            return true;
        }

        virtual EGamePlatformDebugProviderCost GetEstimatedCost() const override
        {
            return EGamePlatformDebugProviderCost::Cheap;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDebugSanitizerTest,
    "GamePlatform.Debug.Core.SensitiveFilter",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformDebugSanitizerTest::RunTest(const FString&)
{
    FGamePlatformDebugSnapshot Snapshot;
    Snapshot.AddField(
        TEXT("Map"),
        TEXT("地图"),
        TEXT("Village"));
    Snapshot.AddField(
        TEXT("AccessToken"),
        TEXT("访问令牌"),
        TEXT("must-not-leak"));
    Snapshot.AddField(
        TEXT("PaymentReceipt"),
        TEXT("支付回执"),
        TEXT("must-not-leak"));

    FGamePlatformDebugRegistry::Get().SanitizeAndBoundSnapshot(Snapshot);

    TestEqual(TEXT("仅保留非敏感字段"), Snapshot.Fields.Num(), 1);
    TestEqual(TEXT("保留Map"), Snapshot.Fields[0].Key, FName(TEXT("Map")));
    TestTrue(TEXT("过滤行为留下截断/清理标记"), Snapshot.bTruncated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDebugSnapshotBoundsTest,
    "GamePlatform.Debug.Core.SnapshotBounds",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformDebugSnapshotBoundsTest::RunTest(const FString&)
{
    FGamePlatformDebugSnapshot Snapshot;
    for (int32 Index = 0;
         Index < GamePlatformDebugLimits::MaxFieldsPerSnapshot + 32;
         ++Index)
    {
        Snapshot.AddField(
            FName(*FString::Printf(TEXT("Field%d"), Index)),
            TEXT("字段"),
            FString::ChrN(
                GamePlatformDebugLimits::MaxStringCharacters + 64,
                TEXT('X')));
    }

    FGamePlatformDebugRegistry::Get().SanitizeAndBoundSnapshot(Snapshot);

    TestTrue(
        TEXT("字段数量有界"),
        Snapshot.Fields.Num() <=
            GamePlatformDebugLimits::MaxFieldsPerSnapshot);
    TestTrue(TEXT("超限时标记截断"), Snapshot.bTruncated);

    for (const FGamePlatformDebugField& Field : Snapshot.Fields)
    {
        TestTrue(
            TEXT("字符串长度有界"),
            Field.Value.Len() <=
                GamePlatformDebugLimits::MaxStringCharacters + 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDebugProviderRegistryTest,
    "GamePlatform.Debug.Core.ProviderRegistry",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformDebugProviderRegistryTest::RunTest(const FString&)
{
    FGamePlatformDebugRegistry& Registry =
        FGamePlatformDebugRegistry::Get();
    Registry.UnregisterStateProvider(TEXT("Unit.Provider"));

    TSharedRef<IGamePlatformDebugStateProvider> Provider =
        MakeShared<FGamePlatformDebugUnitProvider>();

    TestTrue(TEXT("首次Provider注册成功"), Registry.RegisterStateProvider(Provider));
    TestFalse(TEXT("重复Provider被拒绝"), Registry.RegisterStateProvider(Provider));
    TestTrue(TEXT("Provider默认启用"), Registry.IsProviderEnabled(TEXT("Unit.Provider")));
    TestTrue(TEXT("可显式关闭Provider"), Registry.SetProviderEnabled(TEXT("Unit.Provider"), false));
    TestFalse(TEXT("Provider已关闭"), Registry.IsProviderEnabled(TEXT("Unit.Provider")));

    Registry.UnregisterStateProvider(TEXT("Unit.Provider"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDebugCommandRegistryTest,
    "GamePlatform.Debug.Core.CommandRegistry",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformDebugCommandRegistryTest::RunTest(const FString&)
{
    FGamePlatformDebugRegistry& Registry =
        FGamePlatformDebugRegistry::Get();
    const FName CommandName(TEXT("gp.Debug.Unit.Command"));
    Registry.UnregisterCommandDescriptor(CommandName);

    FGamePlatformDebugCommandDescriptor Descriptor;
    Descriptor.Name = CommandName;
    Descriptor.Help = TEXT("Unit command");
    Descriptor.Category = TEXT("Unit");
    Descriptor.Kind = EGamePlatformDebugCommandKind::ReadOnly;
    Descriptor.RequiredPrivilege = TEXT("Developer");
    Descriptor.AllowedBuilds = { TEXT("Development"), TEXT("Test") };

    TestTrue(TEXT("首次命令注册成功"), Registry.RegisterCommandDescriptor(Descriptor));
    TestFalse(TEXT("重复命令被拒绝"), Registry.RegisterCommandDescriptor(Descriptor));

    Descriptor.Name = TEXT("Unit.Command.WithoutPrefix");
    TestFalse(TEXT("无gp.Debug前缀被拒绝"), Registry.RegisterCommandDescriptor(Descriptor));

    Registry.UnregisterCommandDescriptor(CommandName);
    return true;
}

#endif
