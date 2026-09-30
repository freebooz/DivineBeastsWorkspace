// 设置作用域回归：同一工厂创建A/B独占Provider并交错异步写入随机测试槽；不读写真实用户键。
// 此用例要求ClientContext真实本地持久化环境；命令行/PIE限制依旧生效，不能把未执行写入记为通过。
#include "Persistence/GamePlatformSettingsClientPersistenceProvider.h"
#include "Settings/GamePlatformSettingsProjectSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// 异步信号独立于潜伏命令寿命；超时后仍等待原生回调再删除本轮随机槽，避免悬空this和与写入竞争。
struct FSettingsSaveSignals
{
    int32 PendingCount = 0;
    bool bACompleted = false, bBCompleted = false, bASucceeded = false, bBSucceeded = false, bCleanupRequested = false;
    FString TestBase;
    void TryCleanup()
    {
        if (!bCleanupRequested || PendingCount != 0 || TestBase.IsEmpty()) { return; }
        for (const FString& User : {FString(TEXT("Fixture-A")), FString(TEXT("Fixture-B"))})
        {
            FTCHARToUTF8 Utf8(*User); uint8 Digest[FSHA1::DigestSize] = {}; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
            UGameplayStatics::DeleteGameInSlot(TestBase + TEXT("_") + BytesToHex(Digest, UE_ARRAY_COUNT(Digest)), 0);
        }
        TestBase.Reset();
    }
};
class FSettingsInterleavedSaveCommand final : public IAutomationLatentCommand
{
public:
    explicit FSettingsInterleavedSaveCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FSettingsInterleavedSaveCommand() override
    {
        if (ProjectSettings)
        {
            Signals->bCleanupRequested = true;
            Signals->TryCleanup();
            ProjectSettings->LocalProfileSlotName = OriginalBase;
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 15.0) { Test->AddError(TEXT("A/B设置异步写入超时")); return true; }
        if (!bStarted)
        {
            ProjectSettings = GetMutableDefault<UGamePlatformSettingsProjectSettings>(); OriginalBase = ProjectSettings->LocalProfileSlotName;
            TestBase = TEXT("AutomationSettings_") + FGuid::NewGuid().ToString(EGuidFormats::Digits); ProjectSettings->LocalProfileSlotName = TestBase; Signals->TestBase = TestBase;
            FGamePlatformSettingsClientPersistenceProvider Factory;
            A = Factory.CreateScopedProvider(); B = Factory.CreateScopedProvider();
            if (!A || !B || A.Get() == B.Get()) { Test->AddError(TEXT("工厂未创建独立作用域")); return true; }
            A->SetUserContext(TEXT("Fixture-A")); B->SetUserContext(TEXT("Fixture-B"));
            TMap<FName, FGamePlatformSettingValue> ValuesA, ValuesB;
            ValuesA.Add(TEXT("Value"), FGamePlatformSettingValue::MakeNumber(1.0)); ValuesB.Add(TEXT("Value"), FGamePlatformSettingValue::MakeNumber(2.0));
            Signals->PendingCount = 2;
            auto StartedA = A->BeginSave(ValuesA, 1, [State = Signals](const auto& Result)
            { State->bACompleted = true; State->bASucceeded = Result.IsSuccess(); --State->PendingCount; State->TryCleanup(); });
            if (!StartedA.IsSuccess()) { --Signals->PendingCount; }
            auto StartedB = B->BeginSave(ValuesB, 1, [State = Signals](const auto& Result)
            { State->bBCompleted = true; State->bBSucceeded = Result.IsSuccess(); --State->PendingCount; State->TryCleanup(); });
            if (!StartedB.IsSuccess()) { --Signals->PendingCount; }
            if (!StartedA.IsSuccess() || !StartedB.IsSuccess()) { Test->AddError(TEXT("ClientContext未提供本地持久化环境，交错保存未执行")); return true; }
            bStarted = true; return false;
        }
        if (!Signals->bACompleted || !Signals->bBCompleted) { return false; }
        Test->TestTrue(TEXT("A异步落盘成功"), Signals->bASucceeded); Test->TestTrue(TEXT("B异步落盘成功"), Signals->bBSucceeded);
        FGamePlatformSettingsPersistencePayload PayloadA, PayloadB;
        Test->TestTrue(TEXT("读取A档案"), A->Load({}, PayloadA).IsSuccess()); Test->TestTrue(TEXT("读取B档案"), B->Load({}, PayloadB).IsSuccess());
        const auto* LayerA = PayloadA.Layers.Find(EGamePlatformSettingLayer::User); const auto* LayerB = PayloadB.Layers.Find(EGamePlatformSettingLayer::User);
        const auto* ValueA = LayerA ? LayerA->Find(TEXT("Value")) : nullptr; const auto* ValueB = LayerB ? LayerB->Find(TEXT("Value")) : nullptr;
        if (!ValueA || !ValueB) { Test->AddError(TEXT("独立档案未保存预期用户层")); return true; }
        Test->TestEqual(TEXT("A保存不受B切换影响"), ValueA->NumberValue, 1.0); Test->TestEqual(TEXT("B使用自身上下文"), ValueB->NumberValue, 2.0);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    UGamePlatformSettingsProjectSettings* ProjectSettings = nullptr;
    FString OriginalBase, TestBase;
    TUniquePtr<IGamePlatformSettingsPersistenceProvider> A, B;
    TSharedRef<FSettingsSaveSignals> Signals = MakeShared<FSettingsSaveSignals>();
    bool bStarted = false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsInterleavedSaveTest, "GamePlatform.Settings.Client.InterleavedSave", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSettingsInterleavedSaveTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FSettingsInterleavedSaveCommand(this)); return true; }
#endif
