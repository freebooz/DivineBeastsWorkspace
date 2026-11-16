# 《神兽联盟》火冰双掌聚能、飞行与1号键视觉接线｜本轮结果

日期：2026-10-25。工作空间：DivineBeastsWorkspace（神兽联盟工作空间）。引擎：UE5.8（虚幻引擎5.8）。

## 结论

已续接现有四套CastFlight V3（施法/飞行第三版）Niagara（粒子系统），完成两处真实渲染缺陷修复，并把数字1定向飞弹的前摇改为真实左右手独立表现请求。1号飞行定义保持已绑定的V3大火球。完整Editor（编辑器）构建通过，16项原生施法生命周期测试全部通过。**不等于视觉已达到母图，亦未完成新手村真实按键/联网验收。**

## 一、实际输入与替换范围

对十二生肖全部PlaceholderAbilitySet（当前占位技能授权集）进行了原生读取：数字One（1键）→DBA.PrimaryAbility（主技能输入）→Platform.Ability.Input.DivineBeasts.Primary（主输入标签）→各英雄的DirectedMissile（定向飞弹）。数字2为Fireball（火焰弹），本轮没有误改数字2。

前摇的新逻辑身份为 `presentation.dba.development.magic.directedmissile.cast@1`，原生定义：
`/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DirectedMissileCast`。

新前摇引用 `NS_DBA_FirePalmCast_V3`（单掌火焰聚能），通过现有平台表现服务分别生成左右掌实例，依附真实骨骼hand_l/hand_r（左右掌）；只在实际骨架存在时接受已列举的手掌别名。每次施法一个操作身份，两个不同RequestId（请求身份）。

飞行定义 `DA_DBA_VFX_DirectedMissileFlight` 保持指向 `NS_DBA_FireOrbFlight_V3`（大火球飞行）；本轮原生回读再次确认。前摇和飞行只改视觉，技能ID、授权、冷却、伤害、服务器施法时序、弹体速度/碰撞不变。

## 二、四套资源与实机状态

全部位于 `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight`。

- `NS_DBA_FirePalmCast_V3`：火系双掌卷吸/燃烧，三个CPU（中央处理器）发射器，左右手使用同一系统与不同旋向。
- `NS_DBA_IcePalmCast_V3`：寒雾、冷核、晶种三个CPU发射器，不是火焰改成蓝色；已做双掌隔离运行，尚未替换原寒冰箭地面法阵。
- `NS_DBA_FireOrbFlight_V3`：体积火核、外围火焰及世界空间CPU Ribbon（带状尾迹）。
- `NS_DBA_IceCrystalFlight_V3`：实体晶核、外围晶面及世界空间寒气Ribbon。

四套系统均原生编译0错误、0警告。飞行演示使用审核Actor（场景对象）脚本轨迹，不应描述为正式联网弹体。

## 三、真正修复的问题

### 1. 动画帧索引为负

旧原生SimCache（粒子缓存）中出现4个超出4×4图集范围的索引，例如-4.229和-1.895。先运行旧缓存回归并确认失败，再通过Monolith原生动作修复共享掌心运动模块：稳定随机相位采用非负值减floor（向下取整），动画帧索引限定0—15.999。

重新运行四阶段原生PIE（编辑器内运行）后，抽样30个帧索引全部在有效范围。证据：
`Saved/Validation/CastFlightV3/Resume_20261025T141412494Z/FrameIndexRegression.json`。

### 2. 寒雾出现蓝色方块、火焰硬边

源寒雾RGB（色彩通道）每个像素都是255，透明轮廓位于Alpha（透明通道，0—251）。原材质却以R（红色通道）乘粒子Alpha作为透明度，因此形成整片蓝色方块。

通过Monolith `material_query.connect_expressions`（材质节点连接）将两份项目专用母材质的透明度采样由R改成A，保留粒子透明度和密度乘法；原图不改。原生连接图回读、材质编译和保存已完成。新截图中蓝色方块已消除，火焰硬边改善；造型和密度仍需美术精修。

备份及前后节点证据：`Saved/Validation/CastFlightV3/Resume_20261025T141412494Z/AlphaFix`。

## 四、生命周期与源码修改

仅修改项目客户端四个现有文件：
- `DivineBeastsDevelopmentCastingWorldSubsystem.h/.cpp`（施法世界子系统）：增加两掌独立请求/锚点账本；使用已确认的施法时长、已过时间及左右旋向；组件注册/Submit（表现提交）后重新核验操作；取消、重入、死亡清理沿用原生命周期。
- `DivineBeastsDevelopmentMagicPresentation.cpp`（表现映射）：内容修订升到5，仅让DirectedMissile.Cast（1号前摇）解析到新定义，命中仍复用旧火球。
- `DivineBeastsDevelopmentCastingLifecycleTests.cpp`（施法生命周期原生测试）：增加双掌与提交中取消测试，保留原火球/冰法/音效回归。

没有新增平台播放框架、逐帧轮询服务、服务器玩法代码，也没有覆盖其他会话改动或提交Git。

## 五、编译与测试证据

完整Win64 Development Editor（Windows64位开发版编辑器）构建：
`Saved/Validation/FoundationM0/8c251669-3751-481c-9696-4e182949b319/Build/result.json`，Passed（通过）、ExitCode=0（退出码0）、Eligible（证据合格）。复用当前引擎缓存与预编译依赖，不宣称专服目标或打包客户端已构建。

红色回归：`Native-8f9ca297-3598-4ee5-98fd-3a14142571da`，双掌用例实际观察到只有1个请求和缺少窗口参数；另一取消用例通过。

绿色回归：`Saved/Validation/CastingMonster20261013/Native-9e6960e5-9d06-448a-80da-45cef83d89ea/Result.json`，16项全部通过，包括新增双掌、同步取消、原有重入/后继技能/动画归属/音效清理。前一次全组运行也是16项断言成功，但脚本预期数量填15而失败；没有篡改旧结果，已按实际16项重新执行通过。

这些测试使用真实施法组件、世界子系统和请求边界测试提供者；**不是实际Niagara GPU渲染、联网或人工按键的替代证据**。

独立关闭自有编辑器后，以新UE5.8命令行进程执行只读冷加载，四套Niagara均0编译错误、四份定义及参数白名单回读通过。证据：`Saved/Validation/CastFlightV3/Resume_20261025T141412494Z/ColdVerification.json`。引擎启动另有既存DeveloperTools Allowlist（开发工具允许列表）缺失提示，该提示没有当成VFX错误，也没有修改安全配置消除它。

## 六、原生截图及未完成项

本轮Alpha修复后的12张1280×720原生截图与缓存：
`Saved/Validation/CastFlightV3/NativePIE/20261025T144328516962Z`。

每种效果各3帧：FirePalms（火系双掌）、IcePalms（冰系双掌）、FireFlight（火球飞行）、IceFlight（冰晶飞行）。此夹具附着到真实左右手骨骼，但角色姿态固定，是隔离视觉验证，不是按下1键录屏。

仍未完成：新手村真实按键端到端、成形阶段与角色动作完整同步、连续视频、多人联网与性能、画质分级及人工视觉通过。火焰仍偏分离火舌，寒气较淡，飞行尾迹较细，冰晶还有石块感，不能声称已达到四张母图。

## 七、自动重连执行范围

本次恢复了自有UE编辑器和Monolith连接；一次编辑器尚未完成启动导致读取被拒绝，随后核对同一自有PID与监听端口恢复，只重试读取，没有重复资源创建。只读最多3次重连，写入不确定先回读状态。没有绕过既有附件地址或安全拒绝，也没有部署后台常驻重连服务。

## 八、回退

1号飞行原定义备份仍在 `Saved/Validation/CastFlightV3/Slot1Rollback`；本轮新Cast（前摇）定义与源码差异可单独撤销，不可使用全库reset/clean（重置/清理）覆盖他人工作。共享掌心模块与两份材质均保留修改前审核副本。源资源和美术目标保持独立，技术通过不解除人工验收要求。
