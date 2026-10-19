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

## 2026-10-18 追加：按真实施法朝向构建追踪弧线

用户要求施法朝向与目标方向不一致时自动绕弧并持续追踪。正式占位、开发法术及 V6 单投射物三处调用方统一传入角色当前前向作为起飞切线，不再在生成弹体前将方向直接改成目标方向。服务器 `GamePlatformCombatProjectile` 按每个运动子步的原目标碰撞中心有限速率转向，正后方或竖直反向也有稳定转弯平面；靠近目标时收紧转弯半径。

- 连发继续冻结原目标身份和化身代次，改选不会改写在途弹体；目标死亡、销毁、换代及来源失效仍终止。未选目标保留既有直线或螺旋，不扫描附近单位。
- 螺旋偏移在接近目标时平滑收束。进入原半径六倍距离内后，有效半径只减不增，收缩速度不超过轴向速度的一半；每段先用上一步有效半径补偿旧相位，避免换轴或缩径时重放历史偏移。关闭螺旋的法术不受此逻辑影响。
- 每段继续使用真实世界与 Pawn Sweep，弯弧每段最多 11.25 度，并保留原有时间、轴向距离及螺旋相位子步上限；不会穿过障碍伪造目标命中。来源中心到目标中心校验锁定射程，避免前向起射偏移错误拒绝合法的后方射程边界目标。
- 只在初始化时补偿初始转弯和起射偏移，最多增加 0.5 秒有限飞行预算，后续目标移动不刷新期限；不改变技能施法时间、冷却、伤害和服务器战斗资格。
- 客户端继续观察平台弹体的复制运动与确认事实，沿用唯一 Presentation/VFX/SFX 链；没有新增播放器、Niagara System、池或复制字段。

原生验证位于 `Saved/Validation/CurvedHoming20261018/`：`RedDiagnostic` 证实旧行为在侧向起飞、后方射程边界、起飞障碍和真实 1 号输入四项失败；`HelixRed` 进一步复现大半径螺旋绕过目标。修正后的 `Green/Result.json` 为 **21/21 成功**，包含平台全部 14 项投射物测试、真实 1 号攻击／5 号治疗及两类三连发目标冻结／失效回归；原生进程退出码 0，三个受影响 Editor 模块构建 `22e12e5d-f61d-4f78-81b1-7a83d675c33c` 成功。此结果不冒充联网、GPU 或人工视觉验收。

最终完整 Editor 增量构建 `b03a1916-156c-4937-b980-113546cb276b` 成功，构建前后输入一致，证据状态 `Eligible`；完整 Client／Server 增量构建 `e6b7f31f-a658-4729-8467-9e14851d177c` 两目标均退出 0。参数沿用锁定 UE5.8、Win64／Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`，仅 Editor 使用 `-UsePrecompiled -AdditionalPlugins DBAServer`。现有 V6 战斗安全源码回归另有 12/12 成功，与上述原生测试分开计数。`FinalTargets/Result.json` 记录三目标结果；本次未 Cook、替换运行包或启动联网前端，正在运行的 Village／MainArena 不据此宣称已更新。共享任务进入源码稳定窗口，构建结束后已释放引擎锁，不继续占用构建／Cook。
