#include "Attributes/GamePlatformControlAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UGamePlatformControlAttributeSet::UGamePlatformControlAttributeSet()
{
    InitTenacity(0.0f);
}

void UGamePlatformControlAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 只同步控制时长计算所需的玩家私有抗性；无视野敌方不能读取该内部倍率。
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformControlAttributeSet, Tenacity, COND_OwnerOnly, REPNOTIFY_Always);
}

void UGamePlatformControlAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (!FMath::IsFinite(NewValue))
    {
        NewValue = 0.0f;
    }

    if (Attribute == GetTenacityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
}

void UGamePlatformControlAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    SetTenacity(FMath::Clamp(GetTenacity(), 0.0f, 1.0f));
}

void UGamePlatformControlAttributeSet::OnRep_Tenacity(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformControlAttributeSet, Tenacity, OldValue);
}
