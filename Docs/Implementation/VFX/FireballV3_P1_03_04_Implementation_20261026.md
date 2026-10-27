# 火球 V3 P1—03/04 原生审核实施记录

> 执行方式：按 writing-plans／executing-plans 技能在当前会话逐项实施，不提交或合并主分支。

## 目标与边界

本次用户消息为实施规格及01/02已经人工批准的确认来源。只新增03火流汇聚、04大火球成形的独立审核资源；01/02、FireMage原始资源、正式技能定义、服务器时序、GAS、伤害及弹道均不修改。三层边界保持：通用播放器属于GamePlatformVFX，项目美术属于已注册DBAPresentationPack_Core；审核驱动器仅在Editor/PIE临时世界运行。

## 实施计划

- [x] P0：读取工程规则、分支及任务状态；由真实UE资产确认源Niagara、材质、纹理和01/02；记录原资源指纹。
- [x] P1：03/04均采用4个逻辑发射器，统一7个User参数；固定镜头、曝光、预设和有限包围盒。
- [x] P2：保留Energy/Density接口制作阶段实例；因Additive/ParticleSubUV无法提供厚球壳与稳定Ribbon UV，在审核目录新建两份专用母材质及七份实例。
- [ ] P3：创建03；左右掌世界位置到同一动态中心的贝塞尔＋收束螺旋；核心直径10–16cm，保留掌火残留。
- [ ] P4：创建04；继承03核心并膨胀至55–80cm；Mesh承担球体，少量辅助粒子承担外围翻卷。
- [ ] P5：检查移动/旋转双掌、移动中心、尺寸、阶段连续性及取消清理。
- [ ] P6：原生编译、保存、重新加载；固定镜头输出03/04截图及03→04连续视频。
- [ ] P7：记录实际发射器、粒子数、执行耗时、透明过绘与质量降级；未能测量的指标明确保留未完成。
- [ ] P8：补全准确资源引用和结果；停止在等待人工视觉审核。

## 参数和阶段预设

统一参数：User.LeftPalmWS、User.RightPalmWS、User.ConvergeCenterWS、User.PhaseProgress、User.FlameScale、User.FlameIntensity、User.QualityLevel。世界位置采用Niagara原生Position合同；进度0..1、尺寸/强度默认1、质量0..3。

03预设：核心直径由10cm增长至14cm，火流逐渐收束；两侧各一股连续三维曲线，不生成大型球体。04从14cm连续增长到68cm，白金高温区控制在中心小区域；主体橙金，外围朱红。具体视觉参数属于本轮审核初值，不能代替人工批准。

03发射器：E03_LeftStream、E03_RightStream、E03_GatherCore、E03_AirRipple。04发射器：E04_CoreSphere、E04_FlameShell、E04_OuterLicks、E04_AirRipple。

低画质优先关闭空气扰动、削减外围火舌与火星；保留两股核心火流及完整球体。空气扰动局限法术附近，不使用全屏SceneColor叠加或Niagara Fluids。

## 审核路径

沿用已存在的 `/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/`，不修改01/02同目录资产。新增系统名称严格采用用户指定的NS_DBA_Fireball_P1_03_StreamConverge_Review和NS_DBA_Fireball_P1_04_CoreFormed_Review。

脚本和中文记录进入Tools/Unreal/VFX/DevelopmentMagic及本文件；原始截图、视频、指纹、审计及性能报告进入Saved/Validation/FireballV3P10304_20261026。审核资源不注册正式表现目录，不绑定正式技能。

## 执行记录

- 初始Git为main、工作区干净；仅本对话的内部执行代理运行。另一个同工作区对话为磁盘只读审查，未检测到UE构建进程。
- 已恢复自有UE5.8编辑器和直接Monolith连接，实际工程与目录匹配。启动恢复文件已完整复制、校验并保留，未覆盖人工旧稿。
- 两份指定名称Niagara系统、一个运动模块、两份专用母材质及七份实例已由UE创建并保存；两系统编译均0错误、0警告。保存重载证据见SavedReloaded.json，12份原稿指纹不变。
- Position默认值工具接口触发UE HasPositionData断言，避开该接口默认值写入，改为运行时原生SetVariablePosition输入；没有修改引擎。模块InputMap暴露合同及CPU VM不支持的smoothstep/动态向量分支已仅在本次模块修复。
- 首轮原生PIE录得106张1920×1080帧；03实际仅有两股火流，中央核心/热扰动缺失，04外围火舌/热扰动缺失。04核心/球壳位置一致，主体实测Scale=0.68、核心Scale=0.14。造型仍粗糙，不能当作完成图。
- 已定位Monolith DuplicateEmitter仅复制句柄，缺少系统图重建；本次两个副本的新增发射器已改用完整AddEmitterToSystem路径重建并重新编译。尚未完成重建后的运行复验、动态掌心/大世界测试、质量档、过绘或独占GPU性能测量。
- 用户随后要求检查已有火冰V3；已先完成四套已有系统的只读原生截图，见FireIceV3_VisualInspection_20261026.md。这些旧V3截图不作为本次03/04交付证明。
- 人工视觉审核：等待；正式技能替换：未执行；飞行/命中/爆炸：不在范围内。
