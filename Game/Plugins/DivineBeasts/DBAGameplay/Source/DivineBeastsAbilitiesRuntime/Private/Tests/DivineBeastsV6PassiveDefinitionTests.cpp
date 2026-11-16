#if WITH_DEV_AUTOMATION_TESTS
// SSLM V6固有被动原生合同：只核临时定义/GE类默认对象，不假装已运行真实多人战斗。
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsV6PassiveDefinition.h"
#include "Effects/DivineBeastsV6PassiveGameplayEffects.h"
#include "Components/DivineBeastsV6PassiveComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "GameplayEffectTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsV6PassiveDefinitionsTest,
    "DivineBeasts.Abilities.V6.Passive.DefinitionsFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsV6PassiveDefinitionsTest::RunTest(const FString&)
{
    UDivineBeastsV6PassiveDefinition* Definition = NewObject<UDivineBeastsV6PassiveDefinition>();
    if (!TestNotNull(TEXT("临时V6被动定义对象"), Definition)) return false;
    TestFalse(TEXT("默认被动资产禁止用于服务器"), Definition->bApprovedForRuntime);
    TestEqual(TEXT("默认被动类型必须Unsupported"),
        Definition->Mode, EDivineBeastsV6PassiveMode::Unsupported);

    FString Error;
    TestFalse(TEXT("未批准的被动无法激活"),
        Definition->ValidateForHero(FName(TEXT("Hero.Zodiac.Tiger")), Error));

    Definition->bApprovedForRuntime = true; // 只存在于本自动化临时对象，不写DataAsset。
    Definition->HeroDefinitionId = TEXT("Hero.Zodiac.Tiger");
    Definition->Mode = EDivineBeastsV6PassiveMode::TigerFracture;
    Definition->Rules.DurationSeconds = 3.0f;
    Definition->Rules.MaxStacks = 3;
    Definition->Rules.PerStackMagnitude = 2.0f;
    Definition->Rules.MinimumConfirmedDamage = 1.0f;
    TestTrue(TEXT("已授权身份一致且参数合法的裂甲配置可通过纯字段合同"),
        Definition->ValidateForHero(FName(TEXT("Hero.Zodiac.Tiger")), Error));
    TestFalse(TEXT("猪角色不能套用虎裂甲数据"),
        Definition->ValidateForHero(FName(TEXT("Hero.Zodiac.Boar")), Error));
    Definition->Rules.MaxStacks = 6;
    TestFalse(TEXT("多于5层必须拒绝"),
        Definition->ValidateForHero(FName(TEXT("Hero.Zodiac.Tiger")), Error));
    Definition->Rules.MaxStacks = 3;
    Definition->bApprovedForRuntime = false;
    TestFalse(TEXT("取消批准后立即失败关闭"),
        Definition->ValidateForHero(FName(TEXT("Hero.Zodiac.Tiger")), Error));

    const UDivineBeastsV6TigerFractureGameplayEffect* Tiger =
        GetDefault<UDivineBeastsV6TigerFractureGameplayEffect>();
    const UDivineBeastsV6BoarBattleWillGameplayEffect* Boar =
        GetDefault<UDivineBeastsV6BoarBattleWillGameplayEffect>();
    if (!TestNotNull(TEXT("真实裂甲GE类默认对象"), Tiger) ||
        !TestNotNull(TEXT("真实战意GE类默认对象"), Boar)) return false;

    TestEqual(TEXT("虎GE只修改一项GAS属性"), Tiger->Modifiers.Num(), 1);
    TestEqual(TEXT("猪GE只修改一项GAS属性"), Boar->Modifiers.Num(), 1);
    if (Tiger->Modifiers.Num() == 1)
    {
        TestEqual(TEXT("裂甲直接复用平台DamageReduction属性"),
            Tiger->Modifiers[0].Attribute,
            UGamePlatformCombatAttributeSet::GetDamageReductionAttribute());
        TestEqual(TEXT("裂甲的SetByCaller字段必须唯一"),
            Tiger->Modifiers[0].ModifierMagnitude.GetSetByCallerFloat().DataName,
            UDivineBeastsV6TigerFractureGameplayEffect::MagnitudeDataName());
    }
    if (Boar->Modifiers.Num() == 1)
    {
        TestEqual(TEXT("战意直接复用平台DamageBonus属性"),
            Boar->Modifiers[0].Attribute,
            UGamePlatformCombatAttributeSet::GetDamageBonusAttribute());
        TestEqual(TEXT("战意的SetByCaller字段必须唯一"),
            Boar->Modifiers[0].ModifierMagnitude.GetSetByCallerFloat().DataName,
            UDivineBeastsV6BoarBattleWillGameplayEffect::MagnitudeDataName());
    }
    TestEqual(TEXT("裂甲只由同一来源按目标聚合"), Tiger->GetStackingType(),
        EGameplayEffectStackingType::AggregateBySource);
    TestEqual(TEXT("战意本体按单个目标聚合"), Boar->GetStackingType(),
        EGameplayEffectStackingType::AggregateByTarget);
    TestEqual(TEXT("裂甲GE硬叠层上限5"), Tiger->StackLimitCount, 5);
    TestEqual(TEXT("战意GE硬叠层上限5"), Boar->StackLimitCount, 5);
    TestEqual(TEXT("裂甲GE不是永久Buff"), Tiger->DurationPolicy,
        EGameplayEffectDurationType::HasDuration);
    TestEqual(TEXT("战意GE不是永久Buff"), Boar->DurationPolicy,
        EGameplayEffectDurationType::HasDuration);

    const UDivineBeastsV6PassiveComponent* PassiveCDO =
        GetDefault<UDivineBeastsV6PassiveComponent>();
    TestFalse(TEXT("正式Pawn上的被动组件默认不激活"), PassiveCDO->IsPassiveActive());
    return true;
}
#endif
