// 平台PCG Editor金标准创作输入回归：只调用真实路径校验和失败命令行，不创建、加载或保存资产。
// 测试不依赖项目内容包；UE自动化实际执行结果必须独立记录，静态源码检查不代表通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/GamePlatformPCGGoldMapAuthoring.h"
#include "Commands/GamePlatformPCGGoldAssetsCommandlet.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

// 目录尾斜杠、Windows分隔符和首尾空白是等价输入；成功必须清空旧错误并生成唯一尾斜杠。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGGoldBlueprintRootNormalizationTest,
    "GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.Normalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGGoldBlueprintRootNormalizationTest::RunTest(const FString& Parameters)
{
    struct FRootCase
    {
        const TCHAR* Input;
        const TCHAR* Expected;
    };
    const FRootCase Cases[] = {
        {TEXT("/Game/PCG/Blueprints/"), TEXT("/Game/PCG/Blueprints/")},
        {TEXT("/Game/PCG/Blueprints"), TEXT("/Game/PCG/Blueprints/")},
        {TEXT(" \t/Game/PCG/Blueprints/ \t"), TEXT("/Game/PCG/Blueprints/")},
        {TEXT("\\Game\\PCG\\Blueprints\\"), TEXT("/Game/PCG/Blueprints/")},
        {TEXT("/Game/Alternate/PlacementBlueprints"), TEXT("/Game/Alternate/PlacementBlueprints/")}
    };
    for (const FRootCase& Case : Cases)
    {
        FString Root = TEXT("旧输出");
        FString Error = TEXT("旧错误");
        if (TestTrue(*FString::Printf(TEXT("规范化合法包目录：%s"), Case.Input),
            GamePlatformPCGGoldMap::NormalizeBlueprintRoot(Case.Input, Root, Error)))
        {
            TestEqual(TEXT("规范目录保留调用者根且只有一个尾斜杠"), Root, FString(Case.Expected));
            TestTrue(TEXT("成功清空旧错误"), Error.IsEmpty());
            FString Repeated;
            TestTrue(TEXT("规范结果可再次校验"),
                GamePlatformPCGGoldMap::NormalizeBlueprintRoot(Root, Repeated, Error));
            TestEqual(TEXT("规范化具有幂等性"), Repeated, Root);
        }
    }
    return true;
}

// 缺失、相对路径、磁盘路径、对象路径、非法字符、重复斜杠、未挂载根都不得成为可加载类路径。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGGoldBlueprintRootInvalidInputsTest,
    "GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.InvalidInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGGoldBlueprintRootInvalidInputsTest::RunTest(const FString& Parameters)
{
    const TCHAR* InvalidRoots[] = {
        TEXT(""), TEXT(" \t "),
        TEXT("Game/PCG/Blueprints"), TEXT("C:/PCG/Blueprints"),
        TEXT("/Game/PCG/../Blueprints"), TEXT("/Game/PCG/BP_Test.BP_Test_C"),
        TEXT("/Game/PCG/Blue prints"), TEXT("/Game//PCG/Blueprints"),
        TEXT("/Game/PCG/Blueprints//"), TEXT("/PCGGoldUnregisteredRoot/PCG/Blueprints"),
        TEXT("/Script/PCG/Blueprints")
    };
    for (const TCHAR* Input : InvalidRoots)
    {
        FString Root = TEXT("旧输出");
        FString Error;
        TestFalse(*FString::Printf(TEXT("拒绝非法创作目录：%s"), Input),
            GamePlatformPCGGoldMap::NormalizeBlueprintRoot(Input, Root, Error));
        TestTrue(TEXT("失败清空旧输出，不能继续加载旧根"), Root.IsEmpty());
        TestFalse(TEXT("失败提供可定位的参数错误"), Error.IsEmpty());
    }
    return true;
}

// 实际Commandlet负向路由：缺参、空参及非法参数在进入创作函数前以参数错误退出；不触及已有Gold资产。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGGoldBlueprintRootRequiredStagesTest,
    "GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.RequiredStages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGGoldBlueprintRootRequiredStagesTest::RunTest(const FString& Parameters)
{
    UGamePlatformPCGGoldAssetsCommandlet* Commandlet = NewObject<UGamePlatformPCGGoldAssetsCommandlet>();
    if (!TestNotNull(TEXT("创建真实平台Gold命令对象"), Commandlet)) { return false; }
    for (const TCHAR* Stage : {TEXT("Map"), TEXT("RepairMap"), TEXT("mAp"), TEXT("repairmap")})
    {
        for (const TCHAR* Argument : {TEXT(""), TEXT(" -BlueprintRoot=\"\""),
            TEXT(" -BlueprintRoot=Relative/PCG/Blueprints"), TEXT(" -BlueprintRoot=/Game/PCG/BP_Test.BP_Test_C")})
        {
            AddExpectedError(TEXT("PCG Gold地图蓝图目录参数无效"), EAutomationExpectedErrorFlags::Contains, 1);
            const FString Params = FString::Printf(TEXT("-Stage=%s%s"), Stage, Argument);
            TestEqual(*FString::Printf(TEXT("写入阶段在参数校验失败后退出：%s"), *Params),
                Commandlet->Main(Params), 1);
        }
    }
    return true;
}
#endif
