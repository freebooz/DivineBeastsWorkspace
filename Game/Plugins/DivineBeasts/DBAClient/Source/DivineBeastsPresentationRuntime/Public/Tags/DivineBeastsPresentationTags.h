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
}
