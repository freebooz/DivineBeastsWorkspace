#include "Attributes/GamePlatformControlAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UGamePlatformControlAttributeSet::UGamePlatformControlAttributeSet()
{
    InitTenacity(0.0f);
    InitMaxPoise(100.0f);
    InitPoise(100.0f);
    InitPoiseRegen(0.0f);
}

void UGamePlatformControlAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformControlAttributeSet, Tenacity, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformControlAttributeSet, Poise, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformControlAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformControlAttributeSet, PoiseRegen, COND_None, REPNOTIFY_Always);
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
    else if (Attribute == GetMaxPoiseAttribute() || Attribute == GetPoiseRegenAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetPoiseAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxPoise()));
    }
}

void UGamePlatformControlAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    SetTenacity(FMath::Clamp(GetTenacity(), 0.0f, 1.0f));
    SetMaxPoise(FMath::Max(0.0f, GetMaxPoise()));
    SetPoise(FMath::Clamp(GetPoise(), 0.0f, GetMaxPoise()));
    SetPoiseRegen(FMath::Max(0.0f, GetPoiseRegen()));
}

void UGamePlatformControlAttributeSet::OnRep_Tenacity(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformControlAttributeSet, Tenacity, OldValue);
}

void UGamePlatformControlAttributeSet::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformControlAttributeSet, Poise, OldValue);
}

void UGamePlatformControlAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformControlAttributeSet, MaxPoise, OldValue);
}

void UGamePlatformControlAttributeSet::OnRep_PoiseRegen(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformControlAttributeSet, PoiseRegen, OldValue);
}
