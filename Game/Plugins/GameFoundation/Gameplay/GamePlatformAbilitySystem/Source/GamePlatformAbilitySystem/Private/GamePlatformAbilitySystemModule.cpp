#include "GamePlatformAbilityTags.h"
#include "Modules/ModuleManager.h"

namespace GamePlatformAbilityTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputRoot, "Platform.Ability.Input", "平台批准输入标签根");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTestPredicted, "Platform.Ability.Input.TestPredicted", "开发预测技能输入");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTestServer, "Platform.Ability.Input.TestServer", "开发服务器技能输入");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Blocked, "Platform.Ability.State.Blocked", "服务器阻止平台技能激活");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestEnabled, "Platform.Ability.State.TestEnabled", "开发技能前置标签");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestDuration, "Platform.Ability.State.TestDuration", "开发持续效果标签");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestInfinite, "Platform.Ability.State.TestInfinite", "开发无限效果标签");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCooldown, "Platform.Ability.State.TestCooldown", "原生冷却效果标签");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCue, "GameplayCue.Platform.Ability.Test.Activated", "中立GAS开发通知");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(FailureGate, "Platform.Ability.Failure.ActivationGate", "缺少当前玩法激活资格");
}
// 模块加载只登记机制，不登录、不生成角色、不授予能力。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformAbilitySystem)
