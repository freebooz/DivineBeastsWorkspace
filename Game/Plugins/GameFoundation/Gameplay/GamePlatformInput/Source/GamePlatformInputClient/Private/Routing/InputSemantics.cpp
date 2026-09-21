#include "Services/GamePlatformInputServices.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMove,"Platform.Input.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLookDelta,"Platform.Input.LookDelta");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLookRate,"Platform.Input.LookRate");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputAttack,"Platform.Input.Attack.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputAbility1,"Platform.Input.Ability.Slot1");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputAbility2,"Platform.Input.Ability.Slot2");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputAbility3,"Platform.Input.Ability.Slot3");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputAbility4,"Platform.Input.Ability.Slot4");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputInteract,"Platform.Input.Interact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTarget,"Platform.Input.Target.Lock");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMenu,"Platform.Input.Menu");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputConfirm,"Platform.Input.Confirm");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputCancel,"Platform.Input.Cancel");
FGameplayTag GamePlatformInputServices::GetSemanticTag(EGamePlatformInputSemantic Semantic)
{
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move: return InputMove;
    case EGamePlatformInputSemantic::LookDelta: return InputLookDelta;
    case EGamePlatformInputSemantic::LookRate: return InputLookRate;
    case EGamePlatformInputSemantic::AttackPrimary: return InputAttack;
    case EGamePlatformInputSemantic::AbilitySlot1: return InputAbility1;
    case EGamePlatformInputSemantic::AbilitySlot2: return InputAbility2;
    case EGamePlatformInputSemantic::AbilitySlot3: return InputAbility3;
    case EGamePlatformInputSemantic::AbilitySlot4: return InputAbility4;
    case EGamePlatformInputSemantic::Interact: return InputInteract;
    case EGamePlatformInputSemantic::TargetLock: return InputTarget;
    case EGamePlatformInputSemantic::Menu: return InputMenu;
    case EGamePlatformInputSemantic::Confirm: return InputConfirm;
    case EGamePlatformInputSemantic::Cancel: return InputCancel;
    default: return {};
    }
}
EGamePlatformInputUnit GamePlatformInputServices::GetUnit(EGamePlatformInputSemantic Semantic)
{
    if (Semantic == EGamePlatformInputSemantic::Move) { return EGamePlatformInputUnit::NormalizedAxis; }
    if (Semantic == EGamePlatformInputSemantic::LookDelta) { return EGamePlatformInputUnit::DegreesDelta; }
    if (Semantic == EGamePlatformInputSemantic::LookRate) { return EGamePlatformInputUnit::DegreesPerSecond; }
    return EGamePlatformInputUnit::Boolean;
}
uint8 GamePlatformInputServices::GetChannel(EGamePlatformInputSemantic Semantic)
{
    if (Semantic == EGamePlatformInputSemantic::Move) { return 1; }
    if (Semantic == EGamePlatformInputSemantic::LookDelta || Semantic == EGamePlatformInputSemantic::LookRate) { return 2; }
    return Semantic >= EGamePlatformInputSemantic::Menu ? 8 : 4;
}
