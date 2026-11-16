#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Screens/Inventory/DivineBeastsInventoryTextCatalog.h"

/** 状态与机器错误码只展示本地化文字，测试不发起物品操作；本轮不执行。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsInventoryTextCatalogTest,
    "DivineBeasts.UI.Inventory.LocalizedStatusCatalog",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsInventoryTextCatalogTest::RunTest(const FString&)
{
    TestFalse(TEXT("加载状态文本非空"),
        FDivineBeastsInventoryTextCatalog::ForState(
            EGamePlatformInventoryClientState::Loading).IsEmpty());
    TestFalse(TEXT("网络错误状态有安全本地化提示"),
        FDivineBeastsInventoryTextCatalog::ForState(
            EGamePlatformInventoryClientState::Error).IsEmpty());
    TestTrue(TEXT("没有错误时不展示假错误"),
        FDivineBeastsInventoryTextCatalog::ForError(
            EGamePlatformInventoryError::None).IsEmpty());
    TestFalse(TEXT("重复操作有稳定消息"),
        FDivineBeastsInventoryTextCatalog::ForError(
            EGamePlatformInventoryError::DuplicateOperation).IsEmpty());
    TestFalse(TEXT("结果未知必须提示核对"),
        FDivineBeastsInventoryTextCatalog::ForError(
            EGamePlatformInventoryError::OutcomeUnknown).IsEmpty());
    TestFalse(TEXT("未知机器错误码使用安全兜底信息"),
        FDivineBeastsInventoryTextCatalog::ForError(
            static_cast<EGamePlatformInventoryError>(255)).IsEmpty());
    return true;
}

#endif
