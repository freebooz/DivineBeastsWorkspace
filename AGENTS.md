# SSLM 工作空间入口

唯一现行项目总规范为 [神兽联盟统一工程规范](Docs/SSLMEngineeringStandard.md)。在实施前读取其中的「当前工作区与发布规则」和受影响领域。此入口须保持 AGENTS.md 文件名。沿途核对真实生效的 AGENTS.md 与 AGENTS.override.md。

- 先核对实际目录、分支和未提交改动，再读取受影响源码和资产合同。保存他人的改动，按最新明确用户指令处理冲突。
- 主工程为 Game/DivineBeastsArena.uproject；插件唯一位于 Game/Plugins。三层边界、内容包登记与发布验收看统一规范。引擎资产通过正式 UE5.8 原生链路，UI 使用 Monolith MCP。Codex 对话不使用 HWebCodex。
- 本地验证先核对真实 Village/MainArena 战斗与复制版本，使用标准十账号启动入口，调试不限帧。现有构建参数优先增量，所有 Cook、Stage、中间产物和验证制品保留当前工程内。
- 事实来源以 Shared、实际配置、内容登记、真实构建与运行证据为准。规范与历史记录不能替代实际验证。
