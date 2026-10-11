#include "Tags/DivineBeastsPresentationTags.h"

namespace DivineBeastsPresentationTags
{
    UE_DEFINE_GAMEPLAY_TAG(
        World_Interaction_Committed,
        "DBA.Presentation.World.Interaction.Committed");

    UE_DEFINE_GAMEPLAY_TAG(
        Village_Guidance_Ready,
        "DBA.Presentation.Village.Guidance.Ready");
    UE_DEFINE_GAMEPLAY_TAG(
        FrontEnd_FallingFoliage,
        "DBA.Presentation.FrontEnd.FallingFoliage");
    // 原生注册，不能依赖任意Config子目录会被隐式扫描。
    UE_DEFINE_GAMEPLAY_TAG(BirdFlock_Seagull, "DBA.Presentation.Environment.BirdFlock.Seagull");
    UE_DEFINE_GAMEPLAY_TAG(BirdFlock_Swallow, "DBA.Presentation.Environment.BirdFlock.Swallow");
    UE_DEFINE_GAMEPLAY_TAG(BirdFlock_Eagle, "DBA.Presentation.Environment.BirdFlock.Eagle");
}
