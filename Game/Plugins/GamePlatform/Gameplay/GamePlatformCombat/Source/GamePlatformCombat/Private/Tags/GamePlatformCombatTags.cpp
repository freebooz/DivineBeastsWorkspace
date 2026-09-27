#include "Tags/GamePlatformCombatTags.h"

namespace GamePlatformCombatTags
{
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Combat.Data.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Data_Healing, "Combat.Data.Healing");

    UE_DEFINE_GAMEPLAY_TAG(Damage_BypassShield, "Combat.Damage.BypassShield");

    UE_DEFINE_GAMEPLAY_TAG(State_Dead, "Combat.State.Dead");

    UE_DEFINE_GAMEPLAY_TAG(Control_Stun, "Combat.Control.Stun");
    UE_DEFINE_GAMEPLAY_TAG(Control_Silence, "Combat.Control.Silence");

    UE_DEFINE_GAMEPLAY_TAG(Event_Damage, "Combat.Event.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Event_Healing, "Combat.Event.Healing");
    UE_DEFINE_GAMEPLAY_TAG(Event_Control, "Combat.Event.Control");
    UE_DEFINE_GAMEPLAY_TAG(Event_Death, "Combat.Event.Death");

    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Damage, "GameplayCue.Combat.Damage");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Healing, "GameplayCue.Combat.Healing");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Control, "GameplayCue.Combat.Control");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Death, "GameplayCue.Combat.Death");
}
