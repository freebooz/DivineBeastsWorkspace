// Surface编辑器契约回归：只使用Transient对象验证参数合同，不写工程资产。
#include "Materials/MaterialParameterCollection.h"
#include "Misc/AutomationTest.h"
#include "Validation/GamePlatformSurfaceAssetContract.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSurfaceMPCContractTest,
    "GamePlatform.Surface.Editor.MPCContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSurfaceMPCContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UMaterialParameterCollection* Collection =
        NewObject<UMaterialParameterCollection>(GetTransientPackage());
    TestNotNull(TEXT("应能创建Transient MPC测试对象"), Collection);
    if (!Collection)
    {
        return false;
    }

    for (const TPair<FName, float>& Pair : FGamePlatformSurfaceAssetContract::GetRequiredScalarParameters())
    {
        FCollectionScalarParameter Parameter;
        Parameter.ParameterName = Pair.Key;
        Parameter.DefaultValue = Pair.Value;
        Collection->ScalarParameters.Add(Parameter);
    }

    FString Error;
    TestTrue(
        TEXT("完整MPC参数合同应通过"),
        FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(*Collection, Error));

    Collection->ScalarParameters.RemoveAt(Collection->ScalarParameters.Num() - 1);
    TestFalse(
        TEXT("缺少任一稳定参数时必须失败"),
        FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(*Collection, Error));
    TestTrue(TEXT("失败必须返回可读中文原因"), !Error.IsEmpty());

    return true;
}

#endif
