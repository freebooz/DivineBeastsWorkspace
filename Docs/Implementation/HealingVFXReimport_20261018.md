# SSLM治疗VFX重导与性能分档（2026-10-18）

已从用户此前指定的 `D:/GameVFX/ModularGameVFX.uproject` 完整重导治疗花瓣的26份原生依赖，增加3份保留原公式的兼容模块与2份画质系统，共31份真实资产，归 `DBAPresentationPack_Core/Content/VFX/HealingBloom`。全部由Monolith与UE5.8原生工具创建、编译和保存。源文件26项SHA256保持不变；原Library及源工程保留。

稳定Attached定义 `DA_DBA_VFX_HealingPetalBloom` 已指向新系统，逻辑ID `presentation.dba.development.magic.healing.impact@1`、确认治疗事件、目标附着、6秒参数、6.2秒回收上限、冷却和UI均保持既有合同。没有增加VFX播放器、预算服务、Gameplay/GAS至Niagara依赖或客户端权威。

## 原设计对照

源演示Actor的空间设置为1.15倍与离地3厘米；通过最后一层原调色模块恢复粒子位置和Mesh缩放。Sprite大小保留原值，因为原Niagara Sprite渲染未按Actor缩放其尺寸，重复乘1.15会把光环放大。RGB、纹理、材质、花瓣与花粉运动均保留，不用调亮或锐化弥补链路。

独立重新打开UE5.8后，对原系统与新High使用同世界位置、相同镜头/曝光/治疗参数、1/60秒固定步长、冻结模拟和相同视图预热，通过原生PIE FinalColorLDR取证。0.5、2、5秒三个采样的最大通道误差分别为1、1、0（8位255级），各图全部像素误差不超过1。此为受控原生渲染对照，不宣称不同游戏关卡照明也会逐像素相同，人工实战画面仍待签收。

## 性能合同

| 画质 | 有粒子的视觉层 | 每实例粒子 | 调整 |
| --- | --- | --- | --- |
| High/Epic | 9 | 204 | 完整原版 |
| Medium | 6 | 54 | 雾与两类花粉Burst为0 |
| Low | 4 | 41 | 进一步关闭内环与珍珠花瓣Burst，保留治疗光环、光裙与三维花瓣 |

三档均保留9个原生Emitter槽，未把零粒子层冒充已减少Emitter执行槽。通过既有Definition的QualityVariants选择中低档；High/Epic回退到完整主系统。三档共享纹理、材质、Mesh和兼容模块；同一请求只选择一个系统。没有额外动态光、额外粒子、预热池或常驻业务轮询，沿用原生EffectType与平台唯一执行链。

原库采用无限循环和无限循环时长，粒子Lifetime为60。保留Lifetime=60与原NormalizedAge以维持材质外观，但将Emitter设为Self/Once/Fixed 6.2秒并沿用Kill响应，避免视觉结束后继续空转。十实例原生PIE实际共2040粒子，七秒时活跃数为0；测试临时关闭后台编辑器节流，结束恢复。后台低帧率有Niagara最大步长，早期直接按World时间断言的失败记录保留。

`Performance/Result.json`证明并发实例与回收；原生ParticlePerfStats仅取得3个有效活跃帧，CPU/RT数字只作诊断，不能当作性能预算或FPS认证。打包客户端四档、1v1—5v5、GPU/Overdraw、P95/P99、10分钟压力与断线重连仍未执行；未标记可投产。

## 保存、冷加载与交付

旧多链接模块调用在复制后会出现保存/重载编译失败；已刷新模块调用与原参数链接，独立UE5.8命令行进程重新加载保存包，三个系统均0错误、0警告、进程退出0，并在新GUI编辑器再次进行三档渲染。早期失败与最终结果分开保留，不用当前缓存代替保存包证明。

证据根为 `Saved/Validation/HealingReimport20261018`：`ImportPlan.json`、`ColdVerify.json`、`ColdVerifyFinal.log`、`ColdEditorComparison/Result.json`与`PixelComparison.json`、`Performance/Result.json`、`AssetAudit.json`、`Verification.json`。已核对31项真实注册资产和稳定定义引用，不残留源库挂载、旧Runtime或Game根视觉依赖。

容器门禁 `Tests/Assets/ValidateDevelopmentCastingContainer.py` 已要求新High/Medium/Low，旧Library系统存在不能再证明本次发布；相关9项回归通过。本次没有重新Cook/Stage、客户端/服务器构建或打包GPU测试，旧Cook不代表新内容已经进入可执行包。

回退通过Monolith将稳定治疗定义改回旧Library系统与EffectType、清空本次两个QualityVariants并保存/重开；`HealingDefinitionBefore.uasset`只作证据备份，不在编辑器运行时直接覆盖文件。

原编辑器10088在过程中记录ConsoleCtrl退出；本任务未保存或覆盖其最初四个无关脏包，退出后无法复核其最终内存/保存状态。随后独立启动54076重新打开保存包并取证。工作区期间有外部提交，未执行Git重置、覆盖用户修改或提交操作。
