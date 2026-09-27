# TestingAndEvidence（测试与证据）

源码 Automation 测试文件当前 3 个：CombatMath（护盾/溢出/绕盾/过量治疗）、AttributeSet 默认值与非法值约束、Stun/Silence GameplayEffect 标签与 Ability 阻断定义。

`Build/Validation/VerifyCombat.ps1`已执行并通过静态门禁：单 Runtime 模块、允许依赖、禁止反向依赖/客户端危险 RPC/项目耦合、Root 未伪实现、0 个 `.uasset/.umap`。

全工作区 `Test-PluginLayers.ps1`已执行通过：44 插件、77 模块、16 条项目内依赖边、736 项检查。

UE Automation 测试二进制实际执行、三 Target 构建、Cook、专服双客户端、网络异常和真实 Development 资产测试均为未执行。
