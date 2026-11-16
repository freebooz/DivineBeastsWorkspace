# 标准火球术纹理与材质

当前唯一三阶段为 NS_DBA_FireballCast / Flight / Impact，Emitter数为4 / 7 / 4。稳定Definition、Catalog与平台执行链不变。所有权威弹道、碰撞及伤害仍由GamePlatformCombat裁决。

## 飞行视觉合同

40cm完整Opaque球形内核，约57.14cm Mesh外焰配68.57cm Sprite火舌，约74.29cm柔光；圆润亮头接连续渐细的波浪长尾，尾端有少量分叉。Sphere使用唯一SM_DBA_Fireball_CoreSphere，100cm源直径、3968三角面，由合法OBJ源经UE原生导入，无权威碰撞变更。自有发光实例统一按material_emission_scale=0.5写绝对参数，重复作者不叠乘；共享柔光母材质无标量，由本火球ParticleColor应用同一系数，Alpha及共享母材质保持各自合同。

七Emitter：Core（GPU本地）、SurfaceFlames（GPU本地）、FireTrail（CPU世界）、Filaments（CPU世界、3个NiagaraID分组）、Glow（CPU本地单粒子、Sprite及Light）、Embers（GPU世界）、AirRipple（GPU本地）。EmitterState先于SpawnBurst，避免首次两次出生。光源每火球最多1个，半径200cm；High/Epic启用，Medium/Low关闭真实粒子灯，材质发光保留。简单粒子光不承诺软阴影。

当前标准火球使用共享DA_DBA_Fireball_Flight：1200cm/s起步，4000cm/s²推进，0.15秒达到1800cm/s；继续追踪原目标，<30°只取消额外弧线。其他未配置Profile的技能保留既有常速入口。

世界尾迹按原生SpawnPerUnit采样：主尾20cm间距，金丝每条25cm间距。每点累计真实世界行进距离，目标历史最长700cm并按User.TrailLengthScale缩放；年龄上限0.5秒，年龄透明度仅在最后35%寿命收尾，空间透明度仍贯穿整段。静止不堆积新点，不以出生帧瞬时Owner速度计算寿命。Ribbon原始根宽137.14cm补偿源图透明留白，前15%保持饱满截面后渐细；UV全幅1×1。新端只在球内最后5%尾长淡入，尾端14%材质羽化透明，并叠加空间/时间Alpha衰减。可见根宽及自然分叉须以原生帧审核，不把原始137.14cm当作火球实际直径。

## 五类飞行材质及纹理

| 材质家族 | 职责 |
| --- | --- |
| M_DBA_Fireball_CoreSurface | Opaque Unlit球面，CoreFlow三通道，缓慢流动、黄白亮核与金黄脉络 |
| M_DBA_Fireball_Shell | sRGB ShellRing，旋转贴面火舌，独立Alpha |
| M_DBA_Fireball_Trail | sRGB TrailBroad / TrailFilament，宽尾、三金丝及少量飞行火星共用 |
| M_DBA_DevMagicGlow（既有） | 复用柔光，实例MI_DBA_Fireball_Glow |
| M_DBA_PalmAirRipple（既有） | 有界热扰动；共享母材质P_UseParticleAlpha默认0，只有火球实例显式1 |

原M_DBA_Fireball_Flame保留Cast/Impact灰度翻页职责，不再负责飞行彩色主尾。旧MI_DBA_Fireball_Volume已原生迁移为MI_DBA_Fireball_CoreSurface，退休身份禁止新Cook发布。没有第二套半球、System或项目播放器。

六张新源：CoreFlow1024²、HeatRG512²、NoisePack512²为原生校准的线性RGBA数据；ShellRing1254²、TrailBroad/TrailFilament2172×724为已获用户人工认可的彩色RGBA原稿。原字节及SHA保存在SourceManifest.json，未后期改RGB。实际GPU导入尺寸分别1024²/512²/512²/1024²/2048×512/1024×256，Color采样解码sRGB一次，Alpha保持线性。HeatRG/NoisePack为配套基础数据，当前共享热扰动沿用既有材质机制；不把未绑定的数据图写成已生效功能。

飞行RGBA主尾采用半周期错开的双采样、sin²交叉淡化，样本回绕点权重为零；内部按Alpha加权后恢复直通RGB，最终Alpha只应用一次。旧4×4灰度Cast/Impact素材保持MASKS、关闭sRGB、1024²、无跨格Mip。正式Renderer SubImage1×1，翻页在材质内完成。

DynamicParameter R为粒子秒数；飞行主尾/金丝G为归一化历史距离（颜色冷却），B为累计历史距离cm，A为强度1；其它层G/B为0。ParticleColor默认白色乘色，A负责空间及年龄消散。旧通用TrailFade在当前火球禁用，其它技能保留原用法。UseNiagaraAge=1；静态审核MID使用0，绝不保存到正式实例。

## 生命周期、质量与验收

确认命中后仅表现可Drain，主体/外焰/柔光/热浪Inactive Kill，世界尾迹/火星Complete。平台既有账本Stopping保留完整句柄和资源直到OnSystemFinished，0.5秒兜底。取消与世界退出Immediate；世界拥有投射物Niagara组件，来源Actor正常销毁，不延长服务器命中与结算。

Low关闭金丝/火星/热扰动，Medium以上保留装饰；热浪四档Alpha0/0.5/1/1.5。Native编译、冷重载、SimCache、GPU实测、Cook、实际网络准入及用户艺术认可分别留证。源图认可不等于材质认可；不标Production Ready。

作者：Tools/Unreal/VFX/SpellProjectiles/AuthorFireballMaterialsUE58.py、AuthorFireballHeadMeshUE58.py、IntegrateFireballNiagaraMonolith.py；数据烘焙BakeFireballDataTexturesUE58.py。唯一视觉映射及尺寸源PresentationManifest.json；运动模板源Tools/Unreal/Combat/ProjectileFlightProfiles.json。24项纹理/母材质/实例所有权在工程Saved；所有原生操作由直接Monolith执行，只逐包保存本任务资源。

本轮证据Saved/Validation/FireballTrail20261101及FireballFlight各独立RunId；旧17.5米/80cm/4Emitter记录仅为历史，不代表当前方案。

