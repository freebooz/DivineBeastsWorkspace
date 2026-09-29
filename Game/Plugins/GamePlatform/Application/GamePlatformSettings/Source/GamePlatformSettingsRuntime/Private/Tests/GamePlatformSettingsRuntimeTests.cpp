#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Features/IModularFeatures.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Migration/GamePlatformSettingsMigrationRunner.h"
#include "Registry/GamePlatformSettingsRegistry.h"
#include "Resolution/GamePlatformSettingsResolver.h"
#include "Validation/GamePlatformSettingsValidation.h"

namespace
{
    FGamePlatformSettingDescriptor MakeNumberDescriptor(
        const FName SettingId,
        const EGamePlatformSettingRuntimeScope RuntimeScope =
            EGamePlatformSettingRuntimeScope::Client)
    {
        FGamePlatformSettingDescriptor Descriptor;
        Descriptor.SettingId = SettingId;
        Descriptor.Category = FName(TEXT("Test"));
        Descriptor.ValueType = EGamePlatformSettingValueType::Number;
        Descriptor.DefaultValue = FGamePlatformSettingValue::MakeNumber(1.0);
        Descriptor.DefaultLayer = EGamePlatformSettingLayer::ProviderDefault;
        Descriptor.PersistenceScope = EGamePlatformSettingScope::User;
        Descriptor.RuntimeScope = RuntimeScope;
        Descriptor.bHasMinimum = true;
        Descriptor.MinimumValue = FGamePlatformSettingValue::MakeNumber(0.0);
        Descriptor.bHasMaximum = true;
        Descriptor.MaximumValue = FGamePlatformSettingValue::MakeNumber(10.0);
        return Descriptor;
    }

    class FTestSettingsProvider final : public IGamePlatformSettingsProvider
    {
    public:
        explicit FTestSettingsProvider(
            FName InProviderId,
            TArray<FGamePlatformSettingDescriptor> InDescriptors)
            : ProviderId(InProviderId)
            , Descriptors(MoveTemp(InDescriptors))
        {
        }

        virtual FName GetProviderId() const override
        {
            return ProviderId;
        }

        virtual void GetSettingDescriptors(
            TArray<FGamePlatformSettingDescriptor>& OutDescriptors) const override
        {
            OutDescriptors.Append(Descriptors);
        }

    private:
        FName ProviderId;
        TArray<FGamePlatformSettingDescriptor> Descriptors;
    };


    class FTestSettingsMigration final : public IGamePlatformSettingsMigration
    {
    public:
        FTestSettingsMigration(
            FName InId,
            int32 InFrom,
            int32 InTo,
            FName InSettingId,
            double InValue,
            bool bInFail = false)
            : Id(InId)
            , From(InFrom)
            , To(InTo)
            , SettingId(InSettingId)
            , Value(InValue)
            , bFail(bInFail)
        {
        }

        virtual FName GetMigrationId() const override { return Id; }
        virtual int32 GetFromVersion() const override { return From; }
        virtual int32 GetToVersion() const override { return To; }

        virtual FGamePlatformResult Migrate(
            TMap<FName, FGamePlatformSettingValue>& InOutUserValues) const override
        {
            if (bFail)
            {
                return FGamePlatformResult::Failure(
                    TEXT("TestMigrationFailed"),
                    TEXT("测试迁移故意失败。"));
            }

            InOutUserValues.Add(
                SettingId,
                FGamePlatformSettingValue::MakeNumber(Value));
            return FGamePlatformResult::Success();
        }

    private:
        FName Id;
        int32 From = 1;
        int32 To = 2;
        FName SettingId;
        double Value = 0.0;
        bool bFail = false;
    };

    class FScopedSettingsProvider final
    {
    public:
        explicit FScopedSettingsProvider(IGamePlatformSettingsProvider& InProvider)
            : Provider(InProvider)
        {
            IModularFeatures::Get().RegisterModularFeature(
                IGamePlatformSettingsProvider::GetModularFeatureName(),
                &Provider);
        }

        ~FScopedSettingsProvider()
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformSettingsProvider::GetModularFeatureName(),
                &Provider);
        }

    private:
        IGamePlatformSettingsProvider& Provider;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsDescriptorValidationTest,
    "GamePlatform.Settings.Runtime.DescriptorValidation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsDescriptorValidationTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingDescriptor Descriptor =
        MakeNumberDescriptor(FName(TEXT("Test.Valid")));
    TestTrue(
        TEXT("合法Descriptor必须通过"),
        FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor)
            .IsSuccess());

    Descriptor.MinimumValue =
        FGamePlatformSettingValue::MakeNumber(11.0);
    TestFalse(
        TEXT("最小值大于最大值必须失败"),
        FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor)
            .IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsResolverPriorityTest,
    "GamePlatform.Settings.Runtime.ClientLayerPriority",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsResolverPriorityTest::RunTest(
    const FString& Parameters)
{
    const FName Id(TEXT("Test.Priority"));
    TMap<FName, FGamePlatformSettingDescriptor> Descriptors;
    Descriptors.Add(Id, MakeNumberDescriptor(Id));

    FGamePlatformSettingsLayers Layers;
    Layers.FindOrAdd(EGamePlatformSettingLayer::ProviderDefault)
        .Add(Id, FGamePlatformSettingValue::MakeNumber(1.0));
    Layers.FindOrAdd(EGamePlatformSettingLayer::User)
        .Add(Id, FGamePlatformSettingValue::MakeNumber(2.0));
    Layers.FindOrAdd(EGamePlatformSettingLayer::Session)
        .Add(Id, FGamePlatformSettingValue::MakeNumber(3.0));

    FGamePlatformSettingsSnapshot Previous;
    FGamePlatformSettingsSnapshot Snapshot;
    FGamePlatformSettingsChangeSet Changes;
    const FGamePlatformResult Result =
        FGamePlatformSettingsResolver::Resolve(
            Descriptors,
            Layers,
            EGamePlatformSettingRuntimeScope::Client,
            Previous,
            EGamePlatformSettingsChangeReason::Apply,
            1,
            true,
            Snapshot,
            Changes);

    TestTrue(TEXT("客户端分层解析必须成功"), Result.IsSuccess());
    const FGamePlatformResolvedSetting* Resolved = Snapshot.Values.Find(Id);
    TestNotNull(TEXT("快照必须包含SettingId"), Resolved);
    if (Resolved)
    {
        TestEqual(
            TEXT("Session覆盖优先于User与Default"),
            Resolved->Value.NumberValue,
            3.0);
        TestEqual(
            TEXT("来源层必须可诊断"),
            Resolved->SourceLayer,
            EGamePlatformSettingLayer::Session);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsDuplicateIdTest,
    "GamePlatform.Settings.Runtime.RejectsDuplicateSettingId",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsDuplicateIdTest::RunTest(
    const FString& Parameters)
{
    const FName DuplicateId(TEXT("Test.Duplicate"));
    FTestSettingsProvider ProviderA(
        FName(TEXT("ProviderA")),
        { MakeNumberDescriptor(DuplicateId) });
    FTestSettingsProvider ProviderB(
        FName(TEXT("ProviderB")),
        { MakeNumberDescriptor(DuplicateId) });

    FScopedSettingsProvider RegisterA(ProviderA);
    FScopedSettingsProvider RegisterB(ProviderB);

    FGamePlatformSettingsRegistry Registry;
    const FGamePlatformResult Result =
        Registry.Rebuild(
            8,
            32,
            EGamePlatformSettingRuntimeScope::Client);

    TestFalse(TEXT("重复SettingId必须Fail Closed"), Result.IsSuccess());
    TestEqual(
        TEXT("重复SettingId错误码稳定"),
        Result.Code,
        FName(TEXT("SettingsSettingIdDuplicate")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsTypeSafeParseTest,
    "GamePlatform.Settings.Runtime.TypeSafeParsing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsTypeSafeParseTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingValue Parsed;
    TestTrue(
        TEXT("有限Number文本可以解析"),
        FGamePlatformSettingValue::TryParse(
            EGamePlatformSettingValueType::Number,
            TEXT("1.25"),
            Parsed));
    TestEqual(TEXT("解析值正确"), Parsed.NumberValue, 1.25);

    TestFalse(
        TEXT("NaN不得进入Number设置"),
        FGamePlatformSettingValue::TryParse(
            EGamePlatformSettingValueType::Number,
            TEXT("nan"),
            Parsed));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsMigrationChainTest,
    "GamePlatform.Settings.Runtime.MigrationChainCommitsAtomically",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsMigrationChainTest::RunTest(
    const FString& Parameters)
{
    TMap<FName, FGamePlatformSettingValue> Values;
    Values.Add(
        FName(TEXT("Test.Legacy")),
        FGamePlatformSettingValue::MakeNumber(1.0));
    int32 Version = 1;

    FTestSettingsMigration V1ToV2(
        FName(TEXT("TestV1ToV2")),
        1,
        2,
        FName(TEXT("Test.Legacy")),
        2.0);
    FTestSettingsMigration V2ToV3(
        FName(TEXT("TestV2ToV3")),
        2,
        3,
        FName(TEXT("Test.New")),
        3.0);

    TArray<IGamePlatformSettingsMigration*> Migrations{
        &V1ToV2,
        &V2ToV3
    };

    int32 Applied = 0;
    const FGamePlatformResult Result =
        FGamePlatformSettingsMigrationRunner::Run(
            Values,
            Version,
            3,
            Migrations,
            Applied);

    TestTrue(TEXT("连续Migration链应成功"), Result.IsSuccess());
    TestEqual(TEXT("版本应升级到3"), Version, 3);
    TestEqual(TEXT("应执行两个Migration"), Applied, 2);
    TestEqual(
        TEXT("第一步变更应提交"),
        Values.FindChecked(FName(TEXT("Test.Legacy"))).NumberValue,
        2.0);
    TestEqual(
        TEXT("第二步新增值应提交"),
        Values.FindChecked(FName(TEXT("Test.New"))).NumberValue,
        3.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsMigrationRollbackTest,
    "GamePlatform.Settings.Runtime.MigrationFailurePreservesOriginal",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsMigrationRollbackTest::RunTest(
    const FString& Parameters)
{
    const FName LegacyId(TEXT("Test.Legacy"));
    TMap<FName, FGamePlatformSettingValue> Values;
    Values.Add(
        LegacyId,
        FGamePlatformSettingValue::MakeNumber(1.0));
    int32 Version = 1;

    FTestSettingsMigration V1ToV2(
        FName(TEXT("TestV1ToV2")),
        1,
        2,
        LegacyId,
        2.0);
    FTestSettingsMigration V2ToV3Fails(
        FName(TEXT("TestV2ToV3Fails")),
        2,
        3,
        FName(TEXT("Test.New")),
        3.0,
        true);

    TArray<IGamePlatformSettingsMigration*> Migrations{
        &V1ToV2,
        &V2ToV3Fails
    };

    int32 Applied = 0;
    const FGamePlatformResult Result =
        FGamePlatformSettingsMigrationRunner::Run(
            Values,
            Version,
            3,
            Migrations,
            Applied);

    TestFalse(TEXT("迁移链中任一步失败必须整体失败"), Result.IsSuccess());
    TestEqual(TEXT("失败后版本必须保持1"), Version, 1);
    TestEqual(TEXT("失败后不报告已提交Migration"), Applied, 0);
    TestEqual(
        TEXT("失败后原值必须保持不变"),
        Values.FindChecked(LegacyId).NumberValue,
        1.0);
    TestFalse(
        TEXT("失败后不得留下半迁移新增值"),
        Values.Contains(FName(TEXT("Test.New"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsSensitivePersistenceTest,
    "GamePlatform.Settings.Runtime.RejectsSensitivePersistence",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsSensitivePersistenceTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingDescriptor Descriptor =
        MakeNumberDescriptor(FName(TEXT("Test.Sensitive")));
    Descriptor.bSensitive = true;
    Descriptor.PersistenceScope = EGamePlatformSettingScope::User;

    const FGamePlatformResult Result =
        FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor);
    TestFalse(TEXT("敏感设置不得进入User持久化层"), Result.IsSuccess());
    TestEqual(
        TEXT("敏感持久化错误码稳定"),
        Result.Code,
        FName(TEXT("SettingsSensitivePersistenceUnsupported")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsStringCapacityTest,
    "GamePlatform.Settings.Runtime.RejectsOversizedString",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsStringCapacityTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingValue Parsed;
    const FString Oversized = FString::ChrN(
        GamePlatformSettingsLimits::MaxStringLength + 1,
        TEXT('A'));

    TestFalse(
        TEXT("超长字符串不得从持久化或部署文本边界进入设置层"),
        FGamePlatformSettingValue::TryParse(
            EGamePlatformSettingValueType::String,
            Oversized,
            Parsed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsServerPersistenceScopeTest,
    "GamePlatform.Settings.Runtime.RejectsServerPersistenceOnClient",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsServerPersistenceScopeTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingDescriptor Descriptor =
        MakeNumberDescriptor(
            FName(TEXT("Test.ServerPersistenceLeak")),
            EGamePlatformSettingRuntimeScope::Client);
    Descriptor.PersistenceScope = EGamePlatformSettingScope::Server;

    const FGamePlatformResult Result =
        FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor);
    TestFalse(TEXT("Server持久化不得声明为Client运行设置"), Result.IsSuccess());
    TestEqual(
        TEXT("Server持久化端侧隔离错误码应稳定"),
        Result.Code,
        FName(TEXT("SettingsServerScopeRuntimeInvalid")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsServerDefaultScopeTest,
    "GamePlatform.Settings.Runtime.RejectsServerDefaultWithoutServerScope",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsServerDefaultScopeTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformSettingDescriptor Descriptor =
        MakeNumberDescriptor(
            FName(TEXT("Test.ServerDefaultLeak")),
            EGamePlatformSettingRuntimeScope::Server);
    Descriptor.PersistenceScope = EGamePlatformSettingScope::Project;
    Descriptor.DefaultLayer = EGamePlatformSettingLayer::ServerDefault;

    const FGamePlatformResult Result =
        FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor);
    TestFalse(TEXT("ServerDefault必须同时使用Server持久化作用域"), Result.IsSuccess());
    TestEqual(
        TEXT("ServerDefault作用域错误码应稳定"),
        Result.Code,
        FName(TEXT("SettingsServerDefaultScopeInvalid")));
    return true;
}

#endif
