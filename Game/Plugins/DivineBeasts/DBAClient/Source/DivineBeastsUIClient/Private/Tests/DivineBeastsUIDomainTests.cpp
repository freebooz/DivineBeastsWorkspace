#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "HUD/DivineBeastsOpenWorldHUD.h"
#include "HUD/DivineBeastsVillageHUD.h"
#include "HUD/DivineBeastsTutorialHUD.h"
#include "HUD/DivineBeastsTrainingHUD.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"
#include "Screens/Account/DivineBeastsAccountScreenBase.h"
#include "Screens/Characters/DivineBeastsCharacterScreenBase.h"
#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"
#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"
#include "Screens/Inventory/DivineBeastsInventoryScreen.h"
#include "Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h"
#include "Screens/Login/DivineBeastsLoginScreen.h"
#include "Screens/Social/DivineBeastsSocialScreenBase.h"
#include "Screens/System/DivineBeastsSystemScreenBase.h"
#include "Screens/World/DivineBeastsWorldScreenBase.h"
#include "Screens/World/DivineBeastsQuestScreen.h"
#include "ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h"
#include "ViewModels/Social/DivineBeastsSocialViewModel.h"

/**
 * 十大业务域的实际可继承基础类审计。
 * 不创建空Blueprint或网络对象；测试只检查真实UClass继承和默认领域身份。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIDomainHierarchyTest,
    "DivineBeasts.UI.Domains.Hierarchy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIDomainHierarchyTest::RunTest(const FString&)
{
    // Core/Account/Character/World/Inventory五域保持已有反射类身份，只在内部增量完善继承。
    TestTrue(TEXT("Login应继承项目账号基类"),
        UDivineBeastsLoginScreen::StaticClass()->IsChildOf(
            UDivineBeastsAccountScreenBase::StaticClass()));
    TestTrue(TEXT("角色创建应继承项目角色基类"),
        UDivineBeastsCharacterCreateScreen::StaticClass()->IsChildOf(
            UDivineBeastsCharacterScreenBase::StaticClass()));
    TestTrue(TEXT("角色选择应继承项目角色基类"),
        UDivineBeastsCharacterSelectScreen::StaticClass()->IsChildOf(
            UDivineBeastsCharacterScreenBase::StaticClass()));
    TestTrue(TEXT("任务应继承项目世界页面基类"),
        UDivineBeastsQuestScreen::StaticClass()->IsChildOf(
            UDivineBeastsWorldScreenBase::StaticClass()));
    TestTrue(TEXT("背包应继承唯一项目页面基类"),
        UDivineBeastsInventoryScreen::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()));

    // World HUD长期界面保持单独的普通Widget生命期，不误进入CommonUI激活页面栈。
    for (UClass* HUDClass : {
        UDivineBeastsOpenWorldHUD::StaticClass(),
        UDivineBeastsVillageHUD::StaticClass(),
        UDivineBeastsTutorialHUD::StaticClass(),
        UDivineBeastsTrainingHUD::StaticClass()})
    {
        TestTrue(TEXT("所有公共世界HUD须继承世界HUD基类"),
            HUDClass->IsChildOf(UDivineBeastsWorldHUDBase::StaticClass()));
    }
    TestTrue(TEXT("技能条继承战斗表现基类"),
        UDivineBeastsAbilityBarPanel::StaticClass()->IsChildOf(
            UDivineBeastsCombatPanelBase::StaticClass()));
    TestTrue(TEXT("玩家状态继承战斗表现基类"),
        UDivineBeastsPlayerStatusPanel::StaticClass()->IsChildOf(
            UDivineBeastsCombatPanelBase::StaticClass()));

    // Social/System/LiveOps不引入任何虚假网络服务或生产资产。
    TestTrue(TEXT("社交领域拥有可扩展项目页面基类"),
        UDivineBeastsSocialScreenBase::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()));
    TestTrue(TEXT("系统设置保留平台菜单行为"),
        UDivineBeastsSystemScreenBase::StaticClass()->IsChildOf(
            UDivineBeastsMenuScreen::StaticClass()));
    TestTrue(TEXT("运营领域拥有可扩展项目页面基类"),
        UDivineBeastsLiveOpsScreenBase::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()));

    const UDivineBeastsLoginScreen* LoginCDO =
        GetDefault<UDivineBeastsLoginScreen>();
    const UDivineBeastsInventoryScreen* InventoryCDO =
        GetDefault<UDivineBeastsInventoryScreen>();
    TestEqual(TEXT("登录页面领域身份"), LoginCDO->GetBusinessDomain(),
        EDivineBeastsUIDomain::Account);
    TestEqual(TEXT("背包页面领域身份"), InventoryCDO->GetBusinessDomain(),
        EDivineBeastsUIDomain::Inventory);
    return true;
}

/**
 * 验证社交/运营快照只接受当前已授权数据源的严格递增修订。
 * 包含：未绑定拒绝、重复拒绝、旧账号拒绝、容量边界及注销后清空。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIDomainScopeTest,
    "DivineBeasts.UI.Domains.AccountScopedSnapshots",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIDomainScopeTest::RunTest(const FString&)
{
    UDivineBeastsSocialViewModel* Social = NewObject<UDivineBeastsSocialViewModel>();
    UDivineBeastsLiveOpsViewModel* LiveOps = NewObject<UDivineBeastsLiveOpsViewModel>();
    TestNotNull(TEXT("社交视图模型必须可创建"), Social);
    TestNotNull(TEXT("运营视图模型必须可创建"), LiveOps);
    if (!Social || !LiveOps)
    {
        return false;
    }

    const FGuid FirstAccount = FGuid::NewGuid();
    const FGuid SecondAccount = FGuid::NewGuid();
    FDivineBeastsSocialUIProjection SocialSnapshot;
    SocialSnapshot.SourceScopeId = FirstAccount;
    SocialSnapshot.Revision = 1;
    SocialSnapshot.bServiceAvailable = true;
    TestFalse(TEXT("无认证来源时拒绝社交数据"), Social->ApplySocialSnapshot(SocialSnapshot));
    TestTrue(TEXT("绑定第一个账号数据源"), Social->BeginSourceScope(FirstAccount));
    TestTrue(TEXT("接收首份合法社交数据"), Social->ApplySocialSnapshot(SocialSnapshot));
    TestFalse(TEXT("同版本重复投送必须拒绝"), Social->ApplySocialSnapshot(SocialSnapshot));
    SocialSnapshot.SourceScopeId = SecondAccount;
    SocialSnapshot.Revision = 2;
    TestFalse(TEXT("旧账号仍活跃时拒绝异账号社交数据"), Social->ApplySocialSnapshot(SocialSnapshot));
    SocialSnapshot.SourceScopeId = FirstAccount;
    SocialSnapshot.PartyMembers.SetNum(11);
    TestFalse(TEXT("异常超长队伍列表应拒绝"), Social->ApplySocialSnapshot(SocialSnapshot));
    SocialSnapshot.PartyMembers.Reset();
    TestTrue(TEXT("修改快照后合法新版本成功"), Social->ApplySocialSnapshot(SocialSnapshot));
    Social->ClearSourceScope();
    TestFalse(TEXT("注销后拒绝迟到快照"), Social->ApplySocialSnapshot(SocialSnapshot));
    TestFalse(TEXT("注销后好友服务不应显示为可用"),
        Social->GetSnapshotRef().bServiceAvailable);
    TestTrue(TEXT("新账号可开始新的来源代次"), Social->BeginSourceScope(SecondAccount));

    FDivineBeastsLiveOpsUIProjection LiveOpsSnapshot;
    LiveOpsSnapshot.SourceScopeId = FirstAccount;
    LiveOpsSnapshot.Revision = 1;
    LiveOpsSnapshot.bServiceAvailable = true;
    TestTrue(TEXT("绑定运营服务账号来源"), LiveOps->BeginSourceScope(FirstAccount));
    LiveOpsSnapshot.UnreadCount = -1;
    TestFalse(TEXT("不允许负数未读提示"), LiveOps->ApplyLiveOpsSnapshot(LiveOpsSnapshot));
    LiveOpsSnapshot.UnreadCount = 0;
    LiveOpsSnapshot.Notices.SetNum(129);
    TestFalse(TEXT("过量公告不应进入UI缓存"), LiveOps->ApplyLiveOpsSnapshot(LiveOpsSnapshot));
    LiveOpsSnapshot.Notices.Reset();
    TestTrue(TEXT("合法运营快照被接收"), LiveOps->ApplyLiveOpsSnapshot(LiveOpsSnapshot));
    TestFalse(TEXT("重复运营修订必须被拒绝"), LiveOps->ApplyLiveOpsSnapshot(LiveOpsSnapshot));
    LiveOps->ClearSourceScope();
    TestFalse(TEXT("注销后运营列表已清空"),
        LiveOps->GetSnapshotRef().bServiceAvailable);
    return true;
}

#endif
