#pragma once

#include "NativeGameplayTags.h"

namespace DivineBeastsPresentationTags
{
    /** OpenWorld/Village通用：服务器确认交互提交后的项目表现反馈。 */
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
        World_Interaction_Committed);

    /** Village Tutorial/Training：进入可展示引导反馈状态。 */
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
        Village_Guidance_Ready);
    /** 角色选择/创建三维场景共用的环境飘落物语义。 */
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(
        FrontEnd_FallingFoliage);
    /** 纯客户端自然环境鸟种语义，互不冲突，不进入权威战斗或服务器移动。 */
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BirdFlock_Seagull);
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BirdFlock_Swallow);
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BirdFlock_Eagle);
    /** 纯本地蝶/鱼环境语义，不承担捕捉奖励或权威AI。 */
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_ButterflyAzure);
    DIVINEBEASTSPRESENTATIONRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_FishSilver);
}
