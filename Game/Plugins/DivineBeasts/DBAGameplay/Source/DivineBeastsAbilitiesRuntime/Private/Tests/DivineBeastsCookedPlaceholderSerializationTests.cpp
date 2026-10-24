// 打包加载回归：真实读取十二生肖占位GA及CDO，不登录、不授予技能、不改共享CDO。
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Abilities/DivineBeastsPlaceholderGameplayAbility.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCookedPlaceholderSerializationTest,
    "DivineBeasts.Abilities.CookedPlaceholderSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
    EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCookedPlaceholderSerializationTest::RunTest(const FString&)
{
    // 新属性必须来自当前UHT反射；旧Editor的烘焙不能靠客户端源码/收据冒充一致。
    TestNotNull(TEXT("当前能量费用缓存反射存在"),
        FindFProperty<FObjectPropertyBase>(UDivineBeastsConfiguredGameplayAbility::StaticClass(), TEXT("CachedEnergyCost")));
    TestNotNull(TEXT("当前原费用缓存反射存在"),
        FindFProperty<FObjectPropertyBase>(UDivineBeastsConfiguredGameplayAbility::StaticClass(), TEXT("CachedOriginalCost")));
    const TCHAR* Heroes[]={TEXT("Rat"),TEXT("Ox"),TEXT("Tiger"),TEXT("Rabbit"),TEXT("Dragon"),TEXT("Snake"),
        TEXT("Horse"),TEXT("Goat"),TEXT("Monkey"),TEXT("Rooster"),TEXT("Dog"),TEXT("Boar")};
    const TCHAR* Modes[]={TEXT("Primary"),TEXT("Passive"),TEXT("Fireball"),TEXT("Frostbolt"),
        TEXT("ArcaneMissiles"),TEXT("DirectedMissile")};
    int32 Loaded=0;
    for(const TCHAR* Hero:Heroes)
    {
        for(const TCHAR* Mode:Modes)
        {
            const FString Name=FString::Printf(TEXT("GA_DBA_%s_Placeholder%s"),Hero,Mode);
            const FSoftClassPath Path(FString::Printf(TEXT("/DBAGameplay/Abilities/Placeholders/%s.%s_C"),*Name,*Name));
            UClass* Class=Path.TryLoadClass<UDivineBeastsPlaceholderGameplayAbility>();
            if(TestNotNull(Name+TEXT("真实蓝图类加载"),Class))
            {
                if(TestNotNull(Name+TEXT("真实CDO反序列化"),Cast<UDivineBeastsPlaceholderGameplayAbility>(Class->GetDefaultObject())))
                {++Loaded;}
            }
        }
    }
    TestEqual(TEXT("十二生肖六类占位GA全部加载"),Loaded,72);
    AddInfo(FString::Printf(TEXT("PLACEHOLDER_SERIALIZATION_LOADED=%d"),Loaded));
    return !HasAnyErrors();
}
#endif
