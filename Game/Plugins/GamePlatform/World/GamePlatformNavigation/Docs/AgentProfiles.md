# AgentProfiles（导航代理配置）

`UGamePlatformNavigationAgentProfile`字段包括 ProfileId、AgentRadius、AgentHeight、StepHeight、MaxSlope、SupportedNavDataId、DefaultFilterId、InvokerPolicy、TileGeneration/RemovalRadius、DefaultPartialPathPolicy 和 Version。

`ValidateProfile`拒绝空 ID、非有限/非正半径高度、非法坡度/台阶、非法版本，以及 Invoker RemovalRadius 小于 GenerationRadius 等配置。Profile Registry 拒绝重复 ID。

服务器还提供 `ValidateAgentProfileForActor（校验角色代理配置）`：对 ACharacter（角色）读取 CapsuleComponent（胶囊组件）和 CharacterMovement NavAgentProperties（角色移动导航代理属性），拒绝比真实角色更窄/更矮的 Profile，避免“路径通过但角色碰撞过不去”的假安全。

未指定 Profile 时使用 UE 当前默认 Supported Agent；自定义 Profile 必须先在当前 World Navigation Service 注册。真实多 Agent NavData 仍未验证。
