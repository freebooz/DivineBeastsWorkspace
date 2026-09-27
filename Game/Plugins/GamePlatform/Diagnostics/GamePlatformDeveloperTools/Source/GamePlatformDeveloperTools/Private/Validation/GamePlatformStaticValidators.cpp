#include "Validation/GamePlatformStaticValidators.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
    FString WorkspaceRoot()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
    }

    FString LayerFromPath(const FString& Path)
    {
        const FString Normalized = Path.Replace(TEXT("\\"), TEXT("/"));
        // 按唯一三层源码根分类；竞技已物理归入MobaCommon，不再保留旧路径特判。
        if (Normalized.Contains(TEXT("/Game/Plugins/DivineBeasts/"))) return TEXT("DivineBeasts");
        if (Normalized.Contains(TEXT("/Game/Plugins/MobaCommon/"))) return TEXT("MobaCommon");
        if (Normalized.Contains(TEXT("/Game/Plugins/GamePlatform/"))) return TEXT("GamePlatform");
        return FString();
    }

    int32 LayerIndex(const FString& Layer)
    {
        if (Layer == TEXT("GamePlatform")) return 0;
        if (Layer == TEXT("MobaCommon")) return 1;
        if (Layer == TEXT("DivineBeasts")) return 2;
        return INDEX_NONE;
    }

    FGamePlatformValidationResult MakeResult(
        FName RuleId,
        const FString& Target,
        EGamePlatformValidationStatus Status,
        const FString& Message,
        const FString& Evidence = FString())
    {
        FGamePlatformValidationResult Result;
        Result.RuleId = RuleId;
        Result.Target = Target;
        Result.Status = Status;
        Result.Message = Message;
        Result.Evidence = Evidence;
        return Result;
    }

    void FindFiles(const FString& Root, const TCHAR* Pattern, TArray<FString>& OutFiles)
    {
        IFileManager::Get().FindFilesRecursive(OutFiles, *Root, Pattern, true, false, false);
    }

    bool ScanTextFilesForTokens(
        const FString& Root,
        const TArray<FString>& Tokens,
        FString& OutEvidence)
    {
        TArray<FString> Files;
        for (const TCHAR* Pattern : { TEXT("*.modules"), TEXT("*.target"), TEXT("*.json"), TEXT("*.txt"), TEXT("*.receipt") })
        {
            FindFiles(Root, Pattern, Files);
        }

        for (const FString& Path : Files)
        {
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *Path))
            {
                continue;
            }

            for (const FString& Token : Tokens)
            {
                if (Text.Contains(Token, ESearchCase::CaseSensitive))
                {
                    OutEvidence = FString::Printf(TEXT("%s -> %s"), *Path, *Token);
                    return true;
                }
            }
        }

        return false;
    }
}

void UGamePlatformDependencyValidator::ValidateWorkspace(
    TArray<FGamePlatformValidationResult>& OutResults) const
{
    const FString Root = WorkspaceRoot();
    const FString PluginRoot = Root / TEXT("Game/Plugins");

    TArray<FString> BuildFiles;
    FindFiles(PluginRoot, TEXT("*.Build.cs"), BuildFiles);

    TMap<FString, FString> ModuleLayer;
    TMap<FString, FString> ModuleBuildFile;

    for (const FString& BuildFile : BuildFiles)
    {
        FString ModuleName = FPaths::GetCleanFilename(BuildFile);
        ModuleName.RemoveFromEnd(TEXT(".Build.cs"));

        const FString Layer = LayerFromPath(BuildFile);
        if (!ModuleName.IsEmpty() && !Layer.IsEmpty())
        {
            ModuleLayer.Add(ModuleName, Layer);
            ModuleBuildFile.Add(ModuleName, BuildFile);
        }
    }

    TMap<FString, TArray<FString>> Edges;
    TArray<FString> Violations;

    const FRegexPattern QuotedIdentifier(TEXT("\"([A-Za-z_][A-Za-z0-9_]*)\""));

    for (const TPair<FString, FString>& Pair : ModuleBuildFile)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Pair.Value))
        {
            continue;
        }

        FRegexMatcher Matcher(QuotedIdentifier, Text);
        while (Matcher.FindNext())
        {
            const FString Dependency = Matcher.GetCaptureGroup(1);
            if (!ModuleLayer.Contains(Dependency))
            {
                continue;
            }

            Edges.FindOrAdd(Pair.Key).AddUnique(Dependency);

            const int32 FromIndex = LayerIndex(ModuleLayer[Pair.Key]);
            const int32 ToIndex = LayerIndex(ModuleLayer[Dependency]);
            if (FromIndex != INDEX_NONE && ToIndex > FromIndex)
            {
                Violations.Add(FString::Printf(
                    TEXT("%s(%s) -> %s(%s)"),
                    *Pair.Key,
                    *ModuleLayer[Pair.Key],
                    *Dependency,
                    *ModuleLayer[Dependency]));
            }
        }
    }

    if (!Violations.IsEmpty())
    {
        OutResults.Add(MakeResult(
            TEXT("GP.Dependency"),
            TEXT("ModuleGraph"),
            EGamePlatformValidationStatus::Failed,
            TEXT("发现反向三层模块依赖。"),
            FString::Join(Violations, TEXT("; "))));
        return;
    }

    TMap<FString, uint8> VisitState;
    TArray<FString> Stack;
    FString CycleEvidence;

    TFunction<bool(const FString&)> Visit = [&](const FString& Module)
    {
        VisitState.FindOrAdd(Module) = 1;
        Stack.Add(Module);

        for (const FString& Dependency : Edges.FindRef(Module))
        {
            const uint8 State = VisitState.FindRef(Dependency);
            if (State == 0)
            {
                if (Visit(Dependency))
                {
                    return true;
                }
            }
            else if (State == 1)
            {
                const int32 Start = Stack.Find(Dependency);
                TArray<FString> Cycle;
                if (Start != INDEX_NONE)
                {
                    for (int32 Index = Start; Index < Stack.Num(); ++Index)
                    {
                        Cycle.Add(Stack[Index]);
                    }
                }
                Cycle.Add(Dependency);
                CycleEvidence = FString::Join(Cycle, TEXT(" -> "));
                return true;
            }
        }

        Stack.Pop();
        VisitState.FindOrAdd(Module) = 2;
        return false;
    };

    for (const TPair<FString, FString>& Pair : ModuleLayer)
    {
        if (VisitState.FindRef(Pair.Key) == 0 && Visit(Pair.Key))
        {
            OutResults.Add(MakeResult(
                TEXT("GP.Dependency"),
                TEXT("ModuleGraph"),
                EGamePlatformValidationStatus::Failed,
                TEXT("发现模块循环依赖。"),
                CycleEvidence));
            return;
        }
    }

    OutResults.Add(MakeResult(
        TEXT("GP.Dependency"),
        TEXT("ModuleGraph"),
        EGamePlatformValidationStatus::Passed,
        TEXT("未发现三层反向依赖或模块循环。"),
        FString::Printf(TEXT("Scanned %d Build.cs files."), BuildFiles.Num())));
}

bool UGamePlatformDependencyValidator::WriteDependencyGraphJson(
    const FString& OutputPath,
    FString& OutError) const
{
    const FString PluginRoot = WorkspaceRoot() / TEXT("Game/Plugins");

    TArray<FString> BuildFiles;
    FindFiles(PluginRoot, TEXT("*.Build.cs"), BuildFiles);

    TMap<FString, FString> ModuleLayer;
    TMap<FString, FString> ModuleBuildFile;
    for (const FString& BuildFile : BuildFiles)
    {
        FString ModuleName = FPaths::GetCleanFilename(BuildFile);
        ModuleName.RemoveFromEnd(TEXT(".Build.cs"));

        const FString Layer = LayerFromPath(BuildFile);
        if (!ModuleName.IsEmpty() && !Layer.IsEmpty())
        {
            ModuleLayer.Add(ModuleName, Layer);
            ModuleBuildFile.Add(ModuleName, BuildFile);
        }
    }

    const FRegexPattern QuotedIdentifier(TEXT("\"([A-Za-z_][A-Za-z0-9_]*)\""));
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Modules;

    for (const TPair<FString, FString>& Pair : ModuleBuildFile)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Pair.Value))
        {
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> Dependencies;
        TSet<FString> UniqueDependencies;
        FRegexMatcher Matcher(QuotedIdentifier, Text);
        while (Matcher.FindNext())
        {
            const FString Dependency = Matcher.GetCaptureGroup(1);
            if (ModuleLayer.Contains(Dependency))
            {
                UniqueDependencies.Add(Dependency);
            }
        }

        TArray<FString> SortedDependencies = UniqueDependencies.Array();
        SortedDependencies.Sort();
        for (const FString& Dependency : SortedDependencies)
        {
            Dependencies.Add(MakeShared<FJsonValueString>(Dependency));
        }

        TSharedRef<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
        ModuleObject->SetStringField(TEXT("module"), Pair.Key);
        ModuleObject->SetStringField(TEXT("layer"), ModuleLayer.FindRef(Pair.Key));
        ModuleObject->SetStringField(TEXT("buildFile"), Pair.Value);
        ModuleObject->SetArrayField(TEXT("dependencies"), MoveTemp(Dependencies));
        Modules.Add(MakeShared<FJsonValueObject>(ModuleObject));
    }

    RootObject->SetArrayField(TEXT("modules"), MoveTemp(Modules));

    FString JsonText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
    if (!FJsonSerializer::Serialize(RootObject, Writer))
    {
        OutError = TEXT("dependency-graph.json序列化失败。");
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonText, *OutputPath))
    {
        OutError = FString::Printf(
            TEXT("dependency-graph.json写入失败：%s"),
            *OutputPath);
        return false;
    }

    return true;
}

FGamePlatformValidationResult UGamePlatformClientLeakValidator::ValidateArtifactRoot(
    const FString& ArtifactRoot) const
{
    if (ArtifactRoot.IsEmpty() || !IFileManager::Get().DirectoryExists(*ArtifactRoot))
    {
        return MakeResult(
            TEXT("GP.ClientLeak"),
            TEXT("ClientArtifact"),
            EGamePlatformValidationStatus::NotRun,
            TEXT("未提供真实Client构建/Cook工件，不能推测通过。"));
    }

    FString Evidence;
    const bool bLeak = ScanTextFilesForTokens(
        ArtifactRoot,
        {
            TEXT("GamePlatformAIServer"),
            TEXT("GamePlatformNavigationServer"),
            TEXT("GamePlatformQuestServer"),
            TEXT("GamePlatformEquipmentServer"),
            TEXT("DBAServer")
        },
        Evidence);

    return MakeResult(
        TEXT("GP.ClientLeak"),
        ArtifactRoot,
        bLeak ? EGamePlatformValidationStatus::Failed : EGamePlatformValidationStatus::Passed,
        bLeak ? TEXT("Client工件发现ServerOnly模块标识。") : TEXT("Client工件文本清单未发现已知ServerOnly模块标识。"),
        Evidence);
}

FGamePlatformValidationResult UGamePlatformServerLeakValidator::ValidateArtifactRoot(
    const FString& ArtifactRoot) const
{
    if (ArtifactRoot.IsEmpty() || !IFileManager::Get().DirectoryExists(*ArtifactRoot))
    {
        return MakeResult(
            TEXT("GP.ServerLeak"),
            TEXT("ServerArtifact"),
            EGamePlatformValidationStatus::NotRun,
            TEXT("未提供真实Server构建/Cook工件，不能推测通过。"));
    }

    FString Evidence;
    const bool bLeak = ScanTextFilesForTokens(
        ArtifactRoot,
        {
            TEXT("GamePlatformDebugClient"),
            TEXT("GamePlatformUIClient"),
            TEXT("GamePlatformVFXClient"),
            TEXT("GamePlatformCommerceUI"),
            TEXT("DBAClient"),
            TEXT("GamePlatformDeveloperTools")
        },
        Evidence);

    return MakeResult(
        TEXT("GP.ServerLeak"),
        ArtifactRoot,
        bLeak ? EGamePlatformValidationStatus::Failed : EGamePlatformValidationStatus::Passed,
        bLeak ? TEXT("Server工件发现Client/Editor模块标识。") : TEXT("Server工件文本清单未发现已知Client/Editor模块标识。"),
        Evidence);
}

void UGamePlatformRPCAndAuthorityValidator::ValidateWorkspace(
    TArray<FGamePlatformValidationResult>& OutResults) const
{
    const FString Root = WorkspaceRoot() / TEXT("Game");
    TArray<FString> Headers;
    TArray<FString> Sources;
    FindFiles(Root, TEXT("*.h"), Headers);
    FindFiles(Root, TEXT("*.cpp"), Sources);
    Headers.Append(Sources);

    int32 RpcDeclarations = 0;
    int32 ReliableDeclarations = 0;
    int32 UngatedDebugServerRpc = 0;

    for (const FString& Path : Headers)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            continue;
        }

        RpcDeclarations += Text.Contains(TEXT("UFUNCTION(Server")) ? 1 : 0;
        RpcDeclarations += Text.Contains(TEXT("UFUNCTION(Client")) ? 1 : 0;
        RpcDeclarations += Text.Contains(TEXT("NetMulticast")) ? 1 : 0;
        ReliableDeclarations += Text.Contains(TEXT("Reliable")) ? 1 : 0;

        if (Path.Contains(TEXT("GamePlatformDebug"))
            && Text.Contains(TEXT("UFUNCTION(Server"))
            && !Text.Contains(TEXT("UE_BUILD_SHIPPING")))
        {
            ++UngatedDebugServerRpc;
        }
    }

    if (UngatedDebugServerRpc > 0)
    {
        OutResults.Add(MakeResult(
            TEXT("GP.RPCAuthority"),
            TEXT("Game source"),
            EGamePlatformValidationStatus::Failed,
            TEXT("发现未见Shipping条件门禁的Debug Server RPC。"),
            FString::Printf(TEXT("count=%d"), UngatedDebugServerRpc)));
        return;
    }

    OutResults.Add(MakeResult(
        TEXT("GP.RPCAuthority"),
        TEXT("Game source"),
        EGamePlatformValidationStatus::Warning,
        TEXT("Static validation only：已完成RPC声明与Debug Shipping gate静态扫描，授权正确性仍需Integration/Security tests。"),
        FString::Printf(TEXT("rpcFiles=%d reliableFiles=%d"), RpcDeclarations, ReliableDeclarations)));
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformStaticLayerMappingTest,
    "GamePlatform.DeveloperTools.DependencyLayerMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformStaticLayerMappingTest::RunTest(const FString& Parameters)
{
    // 只校验真实目录映射；模块名称前缀不能让竞技错误地下沉为平台能力。
    TestEqual(
        TEXT("规范目录下的平台插件归GamePlatform层"),
        LayerFromPath(TEXT("E:/Workspace/Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/GamePlatformData.Build.cs")),
        FString(TEXT("GamePlatform")));
    TestEqual(
        TEXT("GamePlatformArena仍归MobaCommon层"),
        LayerFromPath(TEXT("E:/Workspace/Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/GamePlatformArena.Build.cs")),
        FString(TEXT("MobaCommon")));
    TestEqual(
        TEXT("项目插件仍归DivineBeasts层"),
        LayerFromPath(TEXT("E:/Workspace/Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/DBAServer.Build.cs")),
        FString(TEXT("DivineBeasts")));
    TestEqual(
        TEXT("插件根目录之外的模块不参与项目分层"),
        LayerFromPath(TEXT("E:/Workspace/ThirdParty/Plugins/Other/Source/Other/Other.Build.cs")),
        FString());
    return true;
}
#endif
