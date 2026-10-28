// SSLM证据拒绝与有效写入回归；仅新RunId开发者夹具，始终待审/未验证，无人工结果。
#if WITH_DEV_AUTOMATION_TESTS
#include "Evidence/DivineBeastsDemoEvidenceWriter.h"
#include "Evidence/DivineBeastsDemoRuntimeOrigin.h"
#include "Misc/AutomationTest.h"
#include "Hash/GamePlatformSHA256.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#if WITH_EDITOR
#include "Review/GamePlatformReviewTypes.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoEvidenceTest, "DivineBeasts.PluginDemos.Evidence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoEvidenceTest::RunTest(const FString&)
{
    FDivineBeastsDemoRunEvidence Empty; FString Path, Error;
    TestFalse(TEXT("无实际运行不写证据"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    TestTrue(TEXT("拒绝含具名诊断"), !Error.IsEmpty()); TestTrue(TEXT("失败不返回伪路径"), Path.IsEmpty());
    // 仅用内存合同验证写前拒绝；不生成冒充实际运行的文件或人工签收记录。
    Empty.Run.Token.RunId = FGuid::NewGuid(); Empty.Run.Token.WorldId = FGuid::NewGuid(); Empty.Run.Token.Generation = 1;
    Empty.Run.ScenarioId = TEXT("Sample.Normal"); Empty.Scenario.ScenarioId = Empty.Run.ScenarioId;
    Empty.ReviewCases.AddDefaulted();
    Empty.SourceFingerprint = FString::ChrN(64, TEXT('z'));
    TestFalse(TEXT("64字符非SHA256拒绝，不能仅检查长度"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    Empty.SourceFingerprint = FString::ChrN(64, TEXT('a')); Empty.Run.CompletedOperations = 9007199254740992LL;
    TestFalse(TEXT("JSON精度外操作计数写前拒绝"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    // 只测试受控来源参数的解析合同，不读取假DLL，不把内存参数视作实际加载证明。
    FGuid LaunchId; FString ContentHash;
    const FString ValidArgs = TEXT("-DBAPluginDemoLaunchId=20261013-1234-4000-8000-000000000001 -DBAPluginDemoContentFingerprint=") + FString::ChrN(64, TEXT('a')) + TEXT(" -LiveCoding=false -NoLiveCoding -NoHotReload");
    TestTrue(TEXT("受控来源完整参数可解析"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs, LaunchId, ContentHash).IsSuccess());
    TestTrue(TEXT("来源身份非零"), LaunchId.IsValid());
    TestEqual(TEXT("内容摘要不被替换"), ContentHash, FString::ChrN(64, TEXT('a')));
    TestFalse(TEXT("普通手工启动不具机器验证来源"), DivineBeasts::Demo::ParseRuntimeOriginArguments(TEXT(""), LaunchId, ContentHash).IsSuccess());
    TestFalse(TEXT("缺热重载隔离拒绝"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs.Replace(TEXT("-NoHotReload"), TEXT("")), LaunchId, ContentHash).IsSuccess());
    TestFalse(TEXT("只有构建NoLiveCoding参数不等于运行禁用"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs.Replace(TEXT("-LiveCoding=false"), TEXT("")), LaunchId, ContentHash).IsSuccess());
    // 新RunId自动化开发者夹具；作者字节合同见事件，不代表游戏源码/构建或人工通过。
    FDivineBeastsDemoRunEvidence Fixture;
    Fixture.Run.Token.RunId = FGuid::NewGuid(); Fixture.Run.Token.WorldId = FGuid::NewGuid(); Fixture.Run.Token.Generation = 1;
    Fixture.Run.ScenarioId = TEXT("SHA256.DeveloperFixture"); Fixture.Run.FeatureId = TEXT("SHA256.FixtureFeature");
    Fixture.Scenario.ScenarioId = Fixture.Run.ScenarioId; Fixture.Run.State = EDivineBeastsDemoRunState::Running;
    Fixture.Scenario.Preconditions = {TEXT("Developer fixture; pending/unverified; no gameplay or human acceptance")};
    Fixture.Run.ObservationText = TEXT("Developer SHA256 fixture, pending/unverified");
    const ANSICHAR Author[] = "Developer SHA256 evidence writer fixture; pending/unverified";
    TestTrue(TEXT("夹具作者原始字节摘要"), GamePlatform::Hash::TrySha256Hex(Author, sizeof(Author) - 1, Fixture.SourceFingerprint));
    Fixture.EventRecords = {UTF8_TO_TCHAR(Author)};
    FDivineBeastsDemoFeatureEntry Feature; Feature.FeatureId = Fixture.Run.FeatureId;
    Feature.Purpose = TEXT("Developer SHA256 append regression; pending/unverified"); Fixture.Features.Add(Feature);
    FDivineBeastsDemoReviewCaseDefinition Frozen; Frozen.ReviewCaseId = TEXT("SHA256.DeveloperReview");
    Frozen.FeatureId = Feature.FeatureId; Frozen.ScenarioId = Fixture.Run.ScenarioId;
    Frozen.Steps = {TEXT("Re-read only this fresh developer fixture")}; Frozen.ExpectedResult = TEXT("Exact digests, pending/unverified");
    Frozen.RequiredEvidence = {TEXT("state_dump"), TEXT("log")}; Fixture.ReviewCases.Add(Frozen);
    if (!TestTrue(TEXT("有效输入真实写证据，覆盖原崩溃路径"), DivineBeasts::Demo::WriteRunEvidence(Fixture, Path, Error))) return false;
    FString Text; TSharedPtr<FJsonObject> Index;
    if (!TestTrue(TEXT("索引实际重读"), FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Index) && Index.IsValid())) return false;
    TestFalse(TEXT("二进制仍未验证"), Index->GetBoolField(TEXT("binary_verified")));
    TestFalse(TEXT("Running零操作未完成"), Index->GetBoolField(TEXT("runtime_completed")));
    TestEqual(TEXT("构建未验证"), Index->GetStringField(TEXT("build_version")), FString(TEXT("unverified")));
    TestEqual(TEXT("真实夹具作者摘要"), Index->GetStringField(TEXT("source_fingerprint")), Fixture.SourceFingerprint);
    FString ChangedPath;
    const auto& Entries = Index->GetArrayField(TEXT("evidence")); TestEqual(TEXT("真实证据数量"), Entries.Num(), 2);
    for (const auto& Value : Entries)
    {
        const auto Entry = Value->AsObject(); const FString File = FPaths::Combine(FPaths::GetPath(Path), Entry->GetStringField(TEXT("path")));
        TArray<uint8> Bytes; FString Hash;
        if (!TestTrue(TEXT("落盘字节可散列"), FFileHelper::LoadFileToArray(Bytes, *File) && GamePlatform::Hash::TrySha256Hex(Bytes.GetData(), Bytes.Num(), Hash))) return false;
        TestEqual(TEXT("保存摘要等于实际字节"), Entry->GetStringField(TEXT("sha256")), Hash);
        TestEqual(TEXT("来源文件端点实际摘要"), DivineBeasts::Demo::HashRuntimeOriginFile(File), Hash); ChangedPath = File;
    }
    TArray<uint8> IndexBytes; FString IndexHash;
    if (!TestTrue(TEXT("实际索引摘要"), FFileHelper::LoadFileToArray(IndexBytes, *Path) && GamePlatform::Hash::TrySha256Hex(IndexBytes.GetData(), IndexBytes.Num(), IndexHash))) return false;
    FString Next;
    if (!TestTrue(TEXT("同Run二次追加"), DivineBeasts::Demo::WriteRunEvidence(Fixture, Next, Error))) return false;
    TestNotEqual(TEXT("NoReplace新索引"), Path, Next);
    TArray<uint8> Again; FString AgainHash;
    TestTrue(TEXT("旧索引保留"), FFileHelper::LoadFileToArray(Again, *Path)); GamePlatform::Hash::TrySha256Hex(Again.GetData(), Again.Num(), AgainHash);
    TestEqual(TEXT("追加不覆盖旧字节"), IndexHash, AgainHash);
    TestTrue(TEXT("缺失来源文件拒绝"), DivineBeasts::Demo::HashRuntimeOriginFile(Path + TEXT(".missing")).IsEmpty());
    const FString EmptyPath = FPaths::Combine(FPaths::GetPath(Path), TEXT("DeveloperEmptyFixture.bin")); TArray<uint8> EmptyBytes;
    TestTrue(TEXT("仅创建本Run空夹具"), FFileHelper::SaveArrayToFile(EmptyBytes, *EmptyPath));
    TestTrue(TEXT("空来源文件拒绝"), DivineBeasts::Demo::HashRuntimeOriginFile(EmptyPath).IsEmpty());
#if WITH_EDITOR
    auto* Case = NewObject<UGamePlatformReviewCase>(); Case->CaseId = Frozen.ReviewCaseId; Case->CaseVersion = Frozen.Version;
    Case->FeatureId = Frozen.FeatureId; Case->ScenarioId = Frozen.ScenarioId; Case->Category = TEXT("PluginDemo"); Case->Description = Feature.Purpose;
    Case->Preconditions = Fixture.Scenario.Preconditions; Case->Steps = Frozen.Steps; Case->ExpectedResult = Frozen.ExpectedResult;
    Case->RequiredEvidence = {TEXT("state_dump"), TEXT("log")}; auto* Review = NewObject<UGamePlatformReviewSubsystem>();
    auto* Pending = Review->CreatePendingReport(Case, Fixture.Run.Token.RunId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("unverified"), TEXT("unverified"));
    if (!TestNotNull(TEXT("创建待审夹具"), Pending)) return false;
    if (!TestTrue(TEXT("审核实际导入有效文件"), Review->BindRunEvidence(Pending, Path, Fixture.SourceFingerprint, TEXT("unverified"), Error))) return false;
    TestEqual(TEXT("审核索引摘要等于实际字节"), Pending->RunEvidenceHash, IndexHash);
    const auto ImportedBefore = Pending->ImportedEvidenceHashes;
    TestFalse(TEXT("外来Run路径拒绝，不改已有待审"), Review->BindRunEvidence(Pending,
        FPaths::Combine(FPaths::GetPath(FPaths::GetPath(FPaths::GetPath(Path))), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens), TEXT("Review"), TEXT("foreign.json")),
        Fixture.SourceFingerprint, TEXT("unverified"), Error));
    TestEqual(TEXT("外来路径拒绝后索引摘要保留"), Pending->RunEvidenceHash, IndexHash);
    TestTrue(TEXT("待审无人工修订"), Pending->Status == EGamePlatformReviewStatus::NotRun && Pending->Reviewer.IsEmpty() && Pending->Revisions.IsEmpty());
    TestFalse(TEXT("不授二进制资格"), Pending->bBinaryVerified);
    TestFalse(TEXT("夹具不能通过真实玩法/二进制门禁"), Review->ValidateReviewEvidence(*Case, *Pending, Error));
    TestTrue(TEXT("故意修改本Run夹具字节"), FFileHelper::SaveStringToFile(TEXT("Developer fixture intentionally changed"), *ChangedPath));
    TestFalse(TEXT("改动文件重新导入拒绝"), Review->BindRunEvidence(Pending, Path, Fixture.SourceFingerprint, TEXT("unverified"), Error));
    TestEqual(TEXT("失败保留原报告"), Pending->RunEvidenceHash, IndexHash);
    TestEqual(TEXT("失败保留导入摘要数量"), Pending->ImportedEvidenceHashes.Num(), ImportedBefore.Num());
    for (const auto& Pair : ImportedBefore)
        TestEqual(TEXT("失败保留每项导入摘要"), Pending->ImportedEvidenceHashes.FindRef(Pair.Key), Pair.Value);
    TestTrue(TEXT("失败仍无人工修订"), Pending->Status == EGamePlatformReviewStatus::NotRun && Pending->Reviewer.IsEmpty() && Pending->Revisions.IsEmpty());
#endif
    return true;
}
#endif
