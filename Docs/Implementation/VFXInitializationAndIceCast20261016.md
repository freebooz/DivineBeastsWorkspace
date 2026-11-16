# VFX初始化审查与冰箭地面起手（2026-10-16）

2026-10-25变更：用户要求火球和冰箭默认0.5秒施法，两条服务器GAS链已将共享默认改为0.5秒；见[默认窗口记录](DevelopmentMagicAndHUD_20261012.md)。下文2秒时序、法阵和测试结果保留为当时证据，不表示新默认已完成原生资产、Cook或客户端视觉验收。

本次只处理迁入库19项初始化提示涉及的8个Legacy系统，以及用户指定的冰箭2秒起手与仅地面法阵。资源继续归DBAPresentationPack_Core；原D盘工程不修改，不建立新播放器。

## 初始化与真实显示

Monolith 0.23的RendererAttributeInit通过模块路径名称推断初始化，无法识别普通InitializeParticle写入、V2静态枚举与自定义HLSL；原始19条工具提示保留，没有通过改名或空模块消除。逐项检查确认19项均有真实生成写入；18项活跃层在PIE SimCache中有正尺寸和非透明粒子，所有Ribbon均有非零跨度。NE_Aura_Mist原始SpawnRate明确为0且无Burst，保持作者停用状态。

真实运行发现并修复：火花两个GPU层的拖曳包装缺少vec3 VelocityOut，恢复原输出合同；火花尺寸模块原始尺寸为零、曲线文本误写进float，恢复原30/18等尺寸及原曲线键到原生曲线接口；传送门E06_Tendrils宽度错误绑定MaterialRandom，改回HLSL实际写入的Particles.RibbonWidth。没有改变原RGB、拖曳公式或增加粒子数量。

8系统及冰箭共9系统的原生FinalColorLDR画面已读取检查。部分Legacy拖尾本身很细小，保留原作者外观；这不是将全42系统重做或全部人工验收。

证据统一在`Saved/Validation/VFXInitialization20261016`：InitializationInputs.json保留原提示和启用模块，SparkScaleRepair.json记录原曲线，CaptureInitial.json和Capture.json记录真实SimCache，CaptureFinal.json合并未修改与修复后采样，Verification.json记录19项结果，ContactSheet.png便于视觉复核。项目未解决初始化项为0，原始工具提示仍为19；二者不能混称。

## 冰箭2秒与仅地面法阵

两条现有GAS释放链共用FrostboltCastDurationSeconds=2.0；复制施法事实、服务器发射定时和客户端蒙太奇速率一致。UI继续订阅已有施法事实事件，不增加Widget Tick或业务轮询。取消、角色代次失效仍撤销定时与表现。

真实Cast系统只保留GroundOuter和GroundInner，分别位于地面6/8厘米且水平放置；删除Core、Orbit、Snow、GatherGlow与OverheadRune起手层，Flight移除RuneRing。冰晶箭身、飞行流带及命中保留；已有地面纹理RGB不变。粒子寿命2.1秒，Attached定义上限2.2秒，由服务器真实结束事件停止。PIE在2.0002秒仍可见，只有两层水平地面法阵；保存并原生重载后3系统编译错误/警告均为0。

## 验证边界

本次原生回归限定两条GAS冰箭起手测试和FrostTwoSecondMontage，验证1.95秒未发射、2.05秒一次发射、取消/换代无迟到发射，以及2秒动作速率与取消释放。构建采用固定UE5.8、Win64 Development、NoSharedPCH、NoUBA、MaxParallelActions=8；Editor附加UsePrecompiled与DBAServer。最终实际构建、Cook、测试及运行结果归本轮Saved证据，不以本文计划代替成功结果。

客户端Cook仅装配既有实际使用的治疗/冰箭闭包，不强制发布整个1170资产库。服务器不包含纯表现包；没有修改后端账号或数据库。双客户端启动不等于人工联机施法验收。

中文说明审查覆盖本轮技能时长、蒙太奇适配、两处回归、初始化检查/捕获/修复脚本及资产作者；未进行全项目历史注释审计。

本机UE冷启动曾超过施法回归入口的原900秒限时，未进入三项测试便超时。入口新增显式TimeoutSeconds，默认仍为900秒，并将真实进程/超时结果独立落盘；本轮后续使用1800秒，不增加测试数量或改变构建参数。首次构建后Water插件README被工作区另一修改改变，旧证据门禁真实拒绝；补充Editor增量只耗0.89秒，原失败记录保留在本轮Pipeline关联证据。

## 本轮执行结果

- EditorBuild：`6ba1bd1d-1bb2-443e-be60-9d9896737406`，退出码0。
- ClientServerBuild：`3e282f46-19f9-4bcb-8f67-4a06aac047a6`，退出码0。
- ClientCook：`3919179f-5a09-4820-8dd8-bfbb7d013132`，退出码0。
- ServerCook：`680ce82d-aad0-4b03-aab6-4fddc68fd5ce`，退出码0。
- 原生回归：`aeb60923-6850-4c4f-b20e-ac8587a4174a`，实际3项全部Success。
- 画面与初始化：`Saved/Validation/VFXInitialization20261016/Verification.json`，8个提示系统和地面冰箭共9系统，19项全部有对应初始化证据，18项有可见粒子，1项为原作者零生成率。原始工具提示仍为19。
- 客户端启动与人工联机验收分开记录：运行身份归同目录Runtime.json，启动结果不代替玩家登录、实战施法或美术验收。

最终运行：两个SSLM客户端PID为25068, 37884，均已到登录页，窗口标题与新产物身份匹配，启动日志无Fatal；Village PID=26884，MainArena PID=56580。五个既有后端ready检查成功，账号未重建。证据为RuntimeVerification.json；人工登录与联机施法尚未验收。
