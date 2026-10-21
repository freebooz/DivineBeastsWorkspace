// 原生只读资产合同：实际重载12角色的GAS集合/GA/Definition/Profile和技能条；不伪造线上Ready。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Abilities/DivineBeastsPlaceholderGameplayAbility.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Definitions/GamePlatformAbilitySetDefinition.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlayableHotbarAssetsTest,
    "DivineBeasts.UI.Combat.PlayableHeroHotbarAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlayableHotbarAssetsTest::RunTest(const FString&)
{
    auto* Bar=LoadClass<UDivineBeastsAbilityBarPanel>(nullptr,TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar.WBP_DBA_UI_AbilityBar_C"));
    if (!TestNotNull(TEXT("已保存正式技能栏"),Bar)) return false;
    TestFalse(TEXT("正式技能栏必须读取真实授权，不能停在美术预览"),Bar->GetDefaultObject<UDivineBeastsAbilityBarPanel>()->bPreviewSelectedHeroIcons);
    for (const TCHAR* Hero:{TEXT("Rat"),TEXT("Ox"),TEXT("Tiger"),TEXT("Rabbit"),TEXT("Dragon"),TEXT("Snake"),TEXT("Horse"),TEXT("Goat"),TEXT("Monkey"),TEXT("Rooster"),TEXT("Dog"),TEXT("Boar")})
    {
        const FString Root=FString(TEXT("/DBAGameplay/Abilities/Placeholders/DA_DBA_"))+Hero+TEXT("_PlaceholderAbilitySet");
        const auto* Set=LoadObject<UGamePlatformAbilitySetDefinition>(nullptr,*(Root+TEXT(".")+FPackageName::GetShortName(Root)));
        const FString ProfilePath=FString(TEXT("/DBAHeroPack_"))+Hero+TEXT("/UI/Abilities/DA_DBA_")+Hero+TEXT("_PlaceholderUIProfile");
        const auto* Profile=LoadObject<UDivineBeastsAbilityUIProfile>(nullptr,*(ProfilePath+TEXT(".")+FPackageName::GetShortName(ProfilePath)));
        if (!TestNotNull(FString(Hero)+TEXT("真实集合"),Set)||!TestNotNull(FString(Hero)+TEXT("真实Profile"),Profile)) return false;
        FString Error;
        TestTrue(FString(Hero)+TEXT("Profile通过身份、条目及图标门禁：")+Error,Profile->ValidateProfile(Error));
        TestTrue(FString(Hero)+TEXT("授权集合通过基础定义门禁"),Set->ValidateDefinition().IsSuccess());
        TSet<FGameplayTag> Inputs;
        for (const auto& Grant:Set->Abilities)
        {
            if (!Grant.InputTag.IsValid()) continue;
            TestFalse(TEXT("输入槽不能重复"),Inputs.Contains(Grant.InputTag)); Inputs.Add(Grant.InputTag);
            UClass* Class=Grant.AbilityClass.LoadSynchronous(); // 测试显式重载，生产施法不增加同步加载。
            const auto* Ability=Class?Cast<UDivineBeastsPlaceholderGameplayAbility>(Class->GetDefaultObject()):nullptr;
            if (!TestNotNull(FString(Hero)+TEXT("复用真实占位执行器"),Ability)) return false;
            TestEqual(TEXT("授权编号与GA定义编号一致"),Ability->AbilityDefinitionId.PrimaryAssetName,FName(*Grant.AbilityId.ToString()));
            const auto* Definition=Cast<UDivineBeastsAbilityDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Ability->AbilityDefinitionId).TryLoad());
            if (!TestNotNull(TEXT("主资产登记及真实定义"),Definition)) return false;
            Definition->BalanceTable.LoadSynchronous();
            FDivineBeastsAbilityBalanceRow Balance;
            TestTrue(TEXT("真实数值行身份及等级一致：")+Error,Definition->TryGetLoadedBalance(1,Balance,Error));
            if (Ability->PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Healing)
                TestEqual(TEXT("治疗保留20米范围"),Balance.CastRangeCm,2000.f);
            else
            {
                TestEqual(TEXT("角色所有攻击映射最远50米"),Balance.CastRangeCm,5000.f);
                TestEqual(TEXT("攻击运动参数与50米施放范围一致"),Ability->ProjectileMotion.RangeCm,5000.f);
            }
            TestEqual(TEXT("定义属于当前角色"),Definition->HeroDefinitionId,FName(*(FString(TEXT("Hero.Zodiac."))+Hero)));
            TestFalse(TEXT("临时映射不得冒充V6最终机制批准"),Definition->bV6NativeApproved);
            const auto* Entry=Profile->FindEntry(FName(*Grant.AbilityId.ToString()));
            if (!TestNotNull(TEXT("服务器技能必须具有同ID的Profile条目"),Entry)) return false;
            TestNotNull(TEXT("实际技能图标可以重载"),Entry->Icon.LoadSynchronous());
            const FString Input=Grant.InputTag.ToString();
            if (Input.EndsWith(TEXT(".Primary"))) TestEqual(TEXT("数字1保留定向飞弹"),Ability->PresentationStyleOverride,FName(TEXT("DBA.Development.DirectedMissile")));
            if (Input.EndsWith(TEXT(".Slot3"))) TestEqual(TEXT("数字5保留真实治疗"),Ability->PlaceholderMode,EDivineBeastsPlaceholderAbilityMode::Healing);
            TestNotNull(TEXT("映射技能沿用真实GAS冷却"),Ability->GetCooldownGameplayEffect());
        }
        TestEqual(FString(Hero)+TEXT("具有8个不同可施放输入槽"),Inputs.Num(),8);
    }
    return true;
}
#endif
