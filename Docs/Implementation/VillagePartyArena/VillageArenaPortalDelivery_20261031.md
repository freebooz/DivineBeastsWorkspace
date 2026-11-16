# SSLM 新手村竞技入口与门心旋涡交付记录

本记录归已批准的VillageArenaPortalDesign_20261031.md。五个入口共用同一模型/材质、面板和后端候选/准入链；追加的中心旋涡与吸入粒子不承担传送、碰撞或胜负判定。

## 当前实现

- 新手村东侧原有可达场地放置1v1—5v5五门。五项服务器入口定义分属唯一模式，共用Arena.Entry.Open交互；原PlayerStart、地形、湖泊保留。原生导航回读五条非partial路径均可达。
- 统一竞技面板含快速匹配和公开玩家ID约战，邀请接受/拒绝/取消、候选确认及关闭面板后的队列HUD均走既有事件通知和唯一命令适配。真实UMG保存/重载后19+5个文字及输入字段字号均10。
- 指定双方的约战复用现有原子资格预留、名单/队长/认证版本校验、候选确认和MainArena分配、签票、结算、安全返回。双方满编、超时或资格变化不另造第二匹配器。无需先建队即可单排；预组队不拆分。
- 门中心为青碧色三臂旋涡，少量暖金细光；局部粒子旋转并向中心收束、消隐循环。一个共享GPU Niagara、一个Emitter、一个附着Definition；局部固定Bounds、EffectType实例限制和四质量装饰缩放。门膜继续提供受控局部空气折动，Low关闭。
- 每World只由主LocalPlayer激活一个项目内容预载/注册事务，再经Presentation→唯一VFX播放五个独立身份的请求。门/世界退出和主玩家更换按原身份取消、撤包/释放自身租约，无Tick/定时器扫描。编辑器预览Actor标记EditorOnly，正式运行由上述协调器负责。

## 资源归属

| 资源 | 唯一位置 |
| --- | --- |
| 权威入口定义 | /DBAWorldPack_Village/Definitions/Activities/Arena |
| 共用门框、膜面及材质 | /DBAWorldPack_Village/Environment/Activities/Arena |
| 旋涡NS、GPU模块、粒子材质、EffectType | /DBAWorldPack_Village/VFX/Activities/Arena |
| 纯表现Definition | /DBAWorldPack_Village/Definitions/Presentation/Activities/DA_VillageArenaGate_Vortex |
| 匹配WBP与队列HUD | /DBAArena/UI/Screens/WBP_DBA_UI_Arena_Matchmaking；/DBAArena/UI/HUD/WBP_DBA_UI_ArenaQueueStatus |

服务器排除仅覆盖旋涡和纯表现Definition叶子，保留五项权威入口定义。Native Tag唯一注册在DivineBeastsArenaRuntime，客户端只引用共享常量。所有原生创建、修改、编译、保存和重载通过Monolith；未生成文本占位资产。

## 证据与边界

证据根为工作空间Saved/Validation/VillageArenaPortal20261031。

- 后端：Backend子目录及BackendPortalChallengeResult_20261031.md记录29包、productiondeps/grpcdeps、竞态和真实隔离Redis回归。
- 原生：Green/Automation为15项关键回归；FinalNative为完整Arena UI及入口/VFX的14项；TagFixNative为三项标签修复回归。最新专服显示剥离修复另见StrippedNative，必须以index.json逐项结果为准。
- 真实UMG：UIFontReload.json；真实地图/导航：Entrances.Readback.json、WorldAuthor.json。
- 旋涡：ColdVortexReadback.json记录独立进程、五门激活、无限帧t.MaxFPS=0/r.VSync=0；VortexMotionNative.json绑定源资产SHA，真实GPU两次采样均96粒子，0.25秒94粒子半径减小、2粒子循环重生，后续数量仍稳定且有界。
- 原生画面：EntrancesFinal.png；动态RHI视频：ViewportClip/VillageArenaVortex.mp4。视频采样频率只控制取证，不限制游戏渲染帧率；不是GPU性能测量。
- 独立整体源码审查及标签/专服空引用修复限定复审均无剩余Critical/Important。

最终构建、Cook、部署与实际进程身份持续登记于VillageArenaPortalExecution_20261031.md。历史失败保留：ClientOnly Native Tag被引擎拒绝、模块ParameterMap曝光合同导致冷载失败、同次Shader编译缺证及专服TextRender被剥离后的空引用，均不能用旧绿色结果掩盖。

## 人工验证

在新手村东侧上坡查看五门，接近门面点击或按F。门只打开并预选对应模式，不直接切图。两客户端分别登录既有测试账号后可检验单人匹配或通过公开玩家ID约战，再逐人确认、进入MainArena及结算返回。2v2—5v5分别需要4/6/8/10个真实会话。

人工视觉、真实1v1完整准入/结算/返回、较大模式实际会话及四画质GPU预算尚须分别签收。源码/构建/Cook或编辑器预览不代替这些验收，不标记Production Ready。

## 最终构建与运行交接（2026-10-31 21:55）

- 完整Editor构建48b1ccfa-9ead-4549-932e-517813f55891通过；完整Client/Server目标31d238ca-040d-476a-af6d-62dc0dbe1b42通过。两次构建之间无C++/契约变化，后一次Editor刷新了原生资产清单文档输入。
- Server Cook/Stage b7a2906f-d6ec-4ca7-9f9e-79f005f8d64b通过；Client Cook/Stage 32549ea0-b462-487c-93e0-4b9782628299通过，二者均绑定同一48b1ccfa Editor门禁。资源归属、客户端容器/法术闭包与服务器纯表现剥离按实际包装器结果通过。
- 实际Village PID28064（17782）和MainArena PID48884（17881）均运行b7a2906f Stage服务器，与最后完整Server二进制SHA一致。核对启动身份后已运行超过八分钟，已初始化且未发现Fatal/战斗复制校验错误；原专服Title空引用未复现。
- 两个客户端PID21928、13148已由32549ea0 Stage恢复，停在登录界面，无自动认证；启动身份和完整目标二进制SHA已核对。两份日志均回读t.MaxFPS="0"、r.VSync="0"，未发现Fatal/战斗复制校验错误。这不是实际Village/MainArena准入证明。
- 前端版本门禁和源文件哈希为Saved/Validation/VillageArenaPortal20261031/FrontEndVersionGate.json；客户端启动回读为ClientStartupVerification.json，运行目录为Saved/Validation/FrontEndClients/d1197916-f3a3-4d87-80ba-46e6c05baddc。
- 本轮Village编辑器PID35812保留打开；默认Monolith端口9316属于另一个火球材质编辑器PID51920，未接管或关闭。用户停止Computer Use后已停止键鼠自动化。
- 本轮所有源码、原生资产和插件清单已冻结，不再发起新构建或Cook。后续火球任务可读取此处稳定交接结果，更新其资源后须重新执行对应双端门禁；本记录仅证明本次旋涡版本。
