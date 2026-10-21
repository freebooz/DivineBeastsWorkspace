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
#include "Components/Image.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

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

/**
 * 使用真实Set的InputTag→AbilityId→唯一Profile条目→已保存Widget画刷，捕获“V6图存在，
 * 但运行授权ID仍取公共占位图”的断链。八槽图来自Profile显式顺序；V1英雄前四槽按
 * 旧作者清单明确的HeroDefinitionId/InputTag/IconAssetPath比较，其余四槽只验证保留图的画刷。
 * 这是资产映射与画刷门禁，未运行VM租约链，不模拟服务器Ready或声称联机画面通过。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAuthorizedHeroIconProjectionTest,
    "DivineBeasts.UI.Combat.AuthorizedHeroIconProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAuthorizedHeroIconProjectionTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsAbilityBarPanel>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar.WBP_DBA_UI_AbilityBar_C"));
    if (!TestNotNull(TEXT("真实已保存技能栏"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsAbilityBarPanel> Bar(
        NewObject<UDivineBeastsAbilityBarPanel>(GetTransientPackage(), Class));
    Bar->Initialize();
    FString LegacyText;
    TSharedPtr<FJsonObject> LegacyCatalog;
    const FString LegacyPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
        TEXT("../Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json")));
    if (!TestTrue(TEXT("旧作者图标来源清单可读"), FFileHelper::LoadFileToString(LegacyText, *LegacyPath)) ||
        !TestTrue(TEXT("旧作者清单JSON有效"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LegacyText), LegacyCatalog))) return false;
    const TArray<TSharedPtr<FJsonValue>>* LegacyEntries = nullptr;
    if (!TestTrue(TEXT("旧作者清单包含显式条目"), LegacyCatalog->TryGetArrayField(TEXT("Entries"), LegacyEntries))) return false;
    const TCHAR* Inputs[] = {TEXT("Primary"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot4"),
        TEXT("Slot3"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7")};
    const TCHAR* WidgetNames[] = {TEXT("PrimaryAbilitySlot"), TEXT("ActiveAbilitySlot1"),
        TEXT("ActiveAbilitySlot2"), TEXT("UltimateAbilitySlot"), TEXT("MovementAbilitySlot"),
        TEXT("DefenseAbilitySlot"), TEXT("ControlAbilitySlot"), TEXT("SignatureAbilitySlot")};
    TArray<UImage*> Images;
    for (const TCHAR* WidgetName : WidgetNames)
    {
        auto* Widget = Cast<UGamePlatformSlotWidget>(Bar->GetWidgetFromName(WidgetName));
        if (!TestNotNull(FString(TEXT("真实槽位 ")) + WidgetName, Widget)) return false;
        Widget->Initialize();
        UImage* Image = Cast<UImage>(Widget->GetWidgetFromName(TEXT("AbilityIcon")));
        if (!TestNotNull(TEXT("真实图标控件"), Image)) return false;
        Images.Add(Image);
    }
    for (const TCHAR* Hero : {TEXT("Rat"), TEXT("Ox"), TEXT("Tiger"), TEXT("Rabbit"), TEXT("Dragon"),
        TEXT("Snake"), TEXT("Horse"), TEXT("Goat"), TEXT("Monkey"), TEXT("Rooster"), TEXT("Dog"), TEXT("Boar")})
    {
        const FString Root = FString(TEXT("/DBAGameplay/Abilities/Placeholders/DA_DBA_")) + Hero + TEXT("_PlaceholderAbilitySet");
        const auto* Set = LoadObject<UGamePlatformAbilitySetDefinition>(nullptr,
            *(Root + TEXT(".") + FPackageName::GetShortName(Root)));
        const FString Mount = FString(TEXT("/DBAHeroPack_")) + Hero;
        const FString ProfilePath = Mount + TEXT("/UI/Abilities/DA_DBA_") + Hero + TEXT("_PlaceholderUIProfile");
        const auto* Profile = LoadObject<UDivineBeastsAbilityUIProfile>(nullptr,
            *(ProfilePath + TEXT(".") + FPackageName::GetShortName(ProfilePath)));
        if (!TestNotNull(TEXT("真实授权集"), Set) || !TestNotNull(TEXT("当前英雄唯一Profile"), Profile)) return false;
        const bool bHasEightSlotArt = Profile->ArtPreviewAbilityIds.Num() == 8;
        if (!TestTrue(FString(Hero) + TEXT("真实异构Profile仅允许无新图或完整八槽"),
            bHasEightSlotArt || Profile->ArtPreviewAbilityIds.IsEmpty())) return false;
        TMap<FName, FSoftObjectPath> LegacyIcons;
        if (!bHasEightSlotArt)
        {
            for (const auto& Value : *LegacyEntries)
            {
                const auto Row = Value->AsObject();
                FString HeroId, Input, IconPath;
                if (Row && Row->TryGetStringField(TEXT("HeroDefinitionId"), HeroId) &&
                    HeroId == FString(TEXT("Hero.Zodiac.")) + Hero &&
                    Row->TryGetStringField(TEXT("InputTag"), Input) && !Input.IsEmpty() &&
                    Row->TryGetStringField(TEXT("IconAssetPath"), IconPath))
                    LegacyIcons.Add(FName(*Input), FSoftObjectPath(IconPath));
            }
            TestEqual(FString(Hero) + TEXT("旧作者只制作四个主动槽图，未制作槽保持临时图"), LegacyIcons.Num(), 4);
        }
        TArray<FGamePlatformUISlotState> States;
        TArray<UTexture2D*> ExpectedTextures;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const FName InputTag(*(FString(TEXT("Platform.Ability.Input.DivineBeasts.")) + Inputs[Index]));
            const auto* Grant = Set->Abilities.FindByPredicate([InputTag](const FGamePlatformAbilityGrant& Item)
                { return Item.InputTag.GetTagName() == InputTag; });
            if (!TestNotNull(FString(Hero) + TEXT("真实输入授权 ") + Inputs[Index], Grant)) return false;
            const FName AbilityId(*Grant->AbilityId.ToString());
            const auto* Entry = Profile->FindEntry(AbilityId);
            if (!TestNotNull(TEXT("授权ID对应的实际表现条目"), Entry)) return false;
            UTexture2D* ActualTexture = Entry->Icon.LoadSynchronous();
            if (!TestNotNull(TEXT("实际授权图真实Texture2D"), ActualTexture)) return false;
            UTexture2D* ExpectedTexture = nullptr;
            if (bHasEightSlotArt)
            {
                const auto* Art = Profile->FindEntry(Profile->ArtPreviewAbilityIds[Index]);
                if (!TestNotNull(TEXT("作者显式角色图条目"), Art)) return false;
                ExpectedTexture = Art->Icon.LoadSynchronous();
            }
            else if (const auto* Path = LegacyIcons.Find(InputTag))
                ExpectedTexture = Cast<UTexture2D>(Path->TryLoad());
            if (bHasEightSlotArt || LegacyIcons.Contains(InputTag))
            {
                if (!TestNotNull(TEXT("明确已制作角色图真实Texture2D"), ExpectedTexture)) return false;
                TestTrue(TEXT("角色图由本英雄包拥有"), ExpectedTexture->GetPathName().StartsWith(Mount + TEXT("/UI/Abilities/")));
                TestEqual(FString(Hero) + TEXT("授权ID接当前角色明确已制作图：") + AbilityId.ToString(), ActualTexture, ExpectedTexture);
            }
            // 没有制作的新槽不凭文件名补八张；这里只检查既有临时图实际投影，不称角色专属图。
            else ExpectedTexture = ActualTexture;
            FGamePlatformUISlotState State;
            State.SlotId = InputTag;
            State.ContentId = AbilityId;
            State.Icon = ActualTexture;
            State.bEnabled = false;
            State.bPending = true;
            States.Add(State);
            ExpectedTextures.Add(ExpectedTexture);
        }
        Bar->ApplyAbilitySlots(States);
        for (int32 Index = 0; Index < 8; ++Index)
        {
            TestEqual(FString(Hero) + TEXT("真实授权图投影到槽位 ") + Inputs[Index],
                Images[Index]->GetBrush().GetResourceObject(), static_cast<UObject*>(ExpectedTextures[Index]));
        }
        Bar->ApplyAbilitySlots({});
        for (UImage* Image : Images) TestNull(TEXT("换角色先清空上一套实际图"), Image->GetBrush().GetResourceObject());
    }
    return true;
}
#endif
