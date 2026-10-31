# 标准火球术 Niagara 接入与同步运行计划

> 执行方式：使用 executing-plans 在本任务连续实施，最后进行独立审查。用户已明确要求接入、同步重新打包并运行，不再次等待方案确认。

**Goal:** 在现有唯一火球前摇、飞行、命中系统中接入已制作纹理/材质，完成同步发布和实际运行验证。

**Architecture:** 保留三项当前Niagara系统、Definition和逻辑ID，继续使用Presentation→唯一VFX执行链。复用现有Emitter、体积主体和空气扰动，只补充材质所需的公共粒子参数适配及必要表现调整，不改权威弹道、伤害和施法资格。

**Tech Stack:** 锁定UE5.8.0、Monolith MCP 0.23.0、现有BuildFoundation/Cook入口。

**Spec:** 用户本轮请求、后续两张飞行参考图和“火球0.5米大小”；已交付的 `DBAPresentationPack_Core/Docs/FireballMaterialKit.md`。

## Global Constraints

- 当前唯一系统为 `/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_Fireball{Cast,Flight,Impact}`，不创建Review/V2等平行版本。
- 球头按**直径0.5米（50cm）、半径25cm**解释，保持圆形明黄色高亮；沿真实路径留下长而连续、由粗亮到细淡的火焰轨迹。新要求替代材质包初始半径30cm。
- 前摇继续消费 `User.CastDurationSec`、`User.ElapsedAtStartSec`、`User.HandSideSign`，以服务器施法窗口为准；取消和世界退出沿现有句柄清理。
- 真实资产只通过Monolith修改；先备份、检查引用和脏包，逐包保存并独立重载。不得操作其它任务的编辑器35812；当前9316对应本任务51920。
- Win64/Development，增量 `-NoSharedPCH -NoUBA -MaxParallelActions 8`；Editor另用 `-UsePrecompiled -AdditionalPlugins DBAServer`，Client/Server不使用UsePrecompiled。
- 全部Cook、Stage、TEMP/TMP和证据位于当前工程；不Clean、不清DDC，不关闭网络校验、VFX检查或发布门禁。
- 启动前核对实际Client、Village和MainArena的构建输入、Cook、可执行路径/PID/Boot身份；版本不一致先同步发布和受控替换。
- 调试 `t.MaxFPS=0/r.VSync=0`，不设置正数上限，不改玩家持久化偏好。账号/角色数据保持现有环境。

## Review Focus

1. 旧前摇动态通道B是Layer，新材质B是Dissolve；适配后不得将整个火焰错误消隐。
2. GPU/CPU模块InputMap曝光和必需参数合同在冷重载后仍正确，不能只看热编译。
3. 长Ribbon的世界空间历史点、Bounds、U轴方向和年龄淡出必须正确，不能变成跟随球体的锥形尾巴。
4. 50cm要同时核对球体网格缩放、材质半径及实际可见主体；外焰不掩盖圆形球头。
5. 取消、重复命中、快速转World和旧服务器复制合同分别验证；进程启动不等于真实准入。

## Task 1：原位接入与原生验收

- [ ] 将实际系统Spec、材料/模块引用及源码状态保存在 `Saved/Validation/FireballNiagara20261031`，备份本轮将修改的包。
- [ ] 在 `Tests/Assets/ValidateFireballNiagaraUE58.py` 增加基于真实保存资产依赖、尺寸及当前绑定的检查，先证明现状缺少新材质接线。
- [ ] 在现有三系统复用Emitter，接入已制作实例；公共材质参数模块归当前SpellProjectiles/Niagara，补齐InputMap合同。
- [ ] 修正圆形明黄50cm球头及长尾；只调整表现，保留真实运动轨迹。原生编译、保存/重载、实际GPU/CPU采样、固定镜头与动态取证。
- [ ] 当前 `PresentationManifest.json` 补充实际发布依赖并更新原生/容器门禁，不创建第二份当前绑定真源。

## Task 2：增量构建、双端Cook与版本门禁

- [ ] 与正在打包的“为插件设置演示关卡并统一字号”任务取得稳定输入交接；期间不修改其Cook资产。
- [ ] 补齐所有正式文档/原生清单后冻结输入，完整Editor、Client、Server增量构建。
- [ ] Client使用 `CookFrontEndClient -CustomConfig VillageDevelopmentClient -ForceNiagaraCompileOnLoad`；Server使用 `CookFoundation -Target Server -CustomConfig VillageDevelopmentServer`，保持既有地图和剥离门禁。
- [ ] 核对实际容器必需资源、同次Niagara编译、着色器诊断和服务器无纯表现资源；失败保留原始证据，不缩减检查集。

## Task 3：受控发布与实际运行

- [ ] 记录并核对旧进程与空竞技槽，使用现有部署入口替换Village/MainArena，不重置后端或账号。
- [ ] 核对Client/Server输入及实际Stage/进程后，启动两个验证客户端；检查真实准入和网络日志。
- [ ] 在运行客户端施放火球，检查前摇、发射、圆形明黄50cm球头、长轨迹、确认命中和取消回收，保存画面证据。
- [ ] 独立审查本轮资产/工具/合同和实际结果，通知已协调的任务恢复；分别报告构建、Cook、运行与人工视听边界。

进度账本：`Saved/Validation/FireballNiagara20261031/Progress.md`。
