# 法术目标追踪与命中表现修复（2026-10-18）

本次仅修改法术中心定位、命中表现事件接线及名称板选中装饰；不调整伤害、治疗、施法时间、冷却或角色动画。

- 所有现有投射法术沿用 `GamePlatformCombatProjectile` 的服务器追踪运动。正式占位技能和开发法术起射统一调用 `UGamePlatformCombatComponent::GetCombatTargetPoint`；飞行子步读取目标当前碰撞包围盒中心，未选目标仍按原轨迹飞行。目标弱身份及化身代次保持冻结，切换选择不会改写在途弹体。
- 真实扫掠继续决定遮挡与命中。受击实体的中心用于复制给客户端的 `ImpactLocation`；权威伤害的 `HitContext` 保留真实碰撞表面证据。世界障碍仍在真实表面播放命中反馈。
- VFX 与音效分别消费服务器确认阶段，各自先登记受理状态以拒绝同步重入。Niagara 内容包加载或请求失败不再连带抑制命中音效。寒冰、火球沿用已有专用命中声，奥术飞弹复用已入库的奥术脉冲声。
- 已通过 Monolith 0.23.0 删除 `/DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar` 的 `SelectionOutline` 和 `SelectionMarker`，停止名称箭头与“已选中”文案；保留名称、真实生命条和生命值。父类不变，蓝图编译零错误、零警告，保存及重载成功。血条填充保持本轮既有 80×6 尺寸。
- 用户要求的追踪策略已写入工作空间 `AGENTS.md` 和用户全局 `C:/Users/Administrator/.codex/AGENTS.md`。

验证证据位于 `Saved/Validation/SpellTargeting20261018/`。主工作区 Client/Server 构建 `a7b05608-bec2-4364-b888-e2f0f422f168` 通过；后续仅修改一处 AI 诊断日志以消除独立 DLL 链接缺失，Server 重新构建 `70c1e9d0-b332-44e4-8754-7f3e260b064c` 通过。主工作区 Editor `143f29b4-55ab-47c2-a152-07c9a08e19ce` 编译和证据生成通过，随后其他任务修改源码导致测试入口拒绝复用。

用户明确其他任务继续修改，故使用托管工作树 `C:/Users/Administrator/.codex/worktrees/spell-homing-validation/DivineBeastsWorkspace` 固定验证输入，并以 `V:/DivineBeastsWorkspace` 的 Windows 短盘符映射避免 UE 260 字符路径限制；未放宽构建证据门禁。隔离副本 Editor 完整构建 `90f59a83-dc4c-44d9-9c2d-99049ecb5143` 通过且证据有效；原生回归 `de42e482-d520-4f15-908d-4031a74b1330` 的 14 项全部成功。复制到主工作区的原始结果在 `Saved/Validation/SpellTargeting20261018/IsolatedEvidence/`，保留原始路径身份，不冒充主工作区最新源码的完整验收。最终 Cook、双客户端联网及人工视听验收仍待完成。

Editor 重启前已将另一任务的未保存审核地图备份至 `Saved/Validation/SpellTargeting20261018/EditorBeforeRestart/`，保存成功并确认无剩余脏包后才关闭编辑器。
