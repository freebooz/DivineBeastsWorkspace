# 神兽联盟｜人型怪物追击、即时攻击与本地受击屏幕反馈实施记录（2026-10-24）

## 任务范围
本轮仅覆盖新手村已确认的人型怪物 `ADivineBeastsVillageVampire`（人型吸血鬼）、既有平台AI原生轻量决策，以及普通世界客户端 `UDivineBeastsRootLayout`（本地玩家界面根布局）。独立专用服务器仍是攻击及伤害的唯一裁决者。不改生肖技能、普通狼、友好NPC或竞赛模式的客户端UI装配。

## 本次代码变更
1. `DivineBeastsVillageVampire.cpp`（人型吸血鬼实现）：构造基础步行速度从320调整到450 cm/s，等于当前玩家构造默认值。**最终玩家实际正常速度来自已就绪的英雄定义，可能并非450。**
2. `GamePlatformAIController.h/.cpp`（平台AI控制器）：增加可选项目速度解析虚函数；在已经核验目标的决策流程里采纳速度；原生路径成功到达攻击距离内时立即补充一次决策，而不是只等周期计时器。所有合法性、AI状态、技能门禁、冷却和权威战斗入口不变。
3. `DivineBeastsVillageWolfAIController.h/.cpp`（新手村怪物服务器AI）：仅在人型吸血鬼追击已认证、就绪的玩家时读取 `UDivineBeastsHeroDefinition.Movement.MaxWalkSpeed`（英雄基础移动速度），不跟随临时减速/冲刺倍率。
4. `DivineBeastsRootLayout.h/.cpp`（客户端根布局）：复用已有 `UGamePlatformCombatFeedbackWorldSubsystem`（服务器确认战斗表现事件总线）。仅当当前拥有玩家被确认扣除生命值时触发 `BP_OnLocalPlayerDamaged(float DamageFraction)`（本地玩家受击反馈蓝图事件），强度按最大生命归一化。远端角色、治疗、仅护盾吸收、无效事件不会触发。事件监听与浮字是否配置解耦。
5. 增加 `DivineBeastsLocalDamageScreenFeedbackTests.cpp`（本地玩家受击条件测试）和 `DivineBeastsVillageHumanoidSpeedTests.cpp`（人型/普通狼/友好NPC基础移动配置测试）。

## 工程证据与状态
- `git diff --check`（Git差异格式检查）：执行成功，未发现差异格式错误；执行期间有Git行尾格式提示，不等于源码错误。
- UE5.8增量编译：引擎 `D:\\UnrealEngine-5.8.0-release`（本机虚幻5.8引擎），`DivineBeastsArenaEditor`（神兽联盟编辑器目标），`-NoLink`（仅编译不链接），包含 `GamePlatformAIServer`（平台AI服务器模块）、`DivineBeastsCharactersRuntime`（角色运行时模块）、`DBAServer`（项目服务器模块）、`DivineBeastsUIClient`（项目UI客户端模块）：最新返回 `Result: Succeeded`。
- 第一次常规链接构建发现新增局部变量名遮蔽成员字段的 C4458；随后已修正，后续仅编译检查返回成功。
- 常规链接构建还存在 `LNK1104`（目标DLL无法写入），现场有正在运行并加载此项目的 `UnrealEditor.exe`（虚幻编辑器）进程。保护在途任务，没有停止它，没有删除/清理二进制。
- 自动化测试文件**已编译**，尚未在新模块DLL中真正执行；未完成完整Editor链接、Cook（资源烘焙）、联机客户端启动或游戏视频验收。

## 当前未完成环节
### 1. 红色屏幕边缘的真实美术与蓝图接线
现有作者界面资产：`DBAUIPack_Core/Content/UI/Root/WBP_DBA_UI_RootLayout.uasset`（根布局用户界面蓝图）。
UI资源改动需按项目全局规定通过 Monolith MCP（虚幻编辑器资产操作服务）进行。当前该服务未注册，**不得以文本写入uasset或伪造已经渲染的屏幕截图**。C++已提供受击回调，但真实红色渐变在蓝图内尚未接线，因此本轮不标记为视觉完成。

恢复合法编辑环境后的建议步骤：
- 在现有根布局顶部、位于常规HUD之上的非交互层加入 `Image`（图片控件），命名 `Img_LocalDamageVignette`（本地受击边缘遮罩）。
- 创建 `M_DBA_UI_LocalDamageVignette`（本地受击UI材质），使用用户界面材质域、透明混合。依照UV距边缘的最小距离生成渐变遮罩，中心透明、四边柔和暗红，无血液或喷溅纹理。
- 在根布局蓝图实现继承事件 `BP_OnLocalPlayerDamaged`：播放受击渐显/渐隐，建议渐显0.08～0.15秒、渐隐0.4～0.8秒；根据 `DamageFraction`（本次伤害/最大生命比例）提高边缘透明度，最大强度限制为不遮挡玩家视野的低透明度。多次受击平滑刷新，不叠加失控。
- 设置 `Not Hit-Testable`（不可参与指针命中测试），支持16:9、21:9及移动端宽高比；保证鼠标和技能栏操作不受阻挡。
- 只有在专服加双客户端真实运行画面中确认红边、强度、衰减、多人视口隔离后才能验收。

### 2. 攻击动画命中帧
新手村当前近战服务器实现仍是 `TryExecuteNativeAttack`→`Combat.ApplyDamage`→`PublishConfirmedCast`（确认攻击表现），所以伤害在服务器确认攻击请求时就结算，**不是按正式近战攻击动画命中帧延迟结算**。本轮优化的是及时发起合法攻击，未声称命中帧时序已重构。正式动画帧与伤害同步要另行通过权威定时/动画通知证明，不得用客户端动画通知直接授予伤害。

### 3. 完整构建与联机视觉验收
在当前编辑器使用者允许释放目标DLL后进行完整 Editor、Client、Server 构建，并执行两客户端进入 `L_Village_Start`（新手村地图）的攻防与红屏验证，提供真实日志、时间戳、游戏截图或视频。失败须原样记录，不凭代码存在推断游戏效果。

## 不允许破坏
- 非人型狼怪速度375 cm/s、友好村民速度180 cm/s；
- 服务器权威伤害与安全区规则；
- 普通浮字、已有生命条、英雄技能和多玩家界面隔离；
- 当前工作区其他未提交改动，以及在途编辑器、Cook及资产处理任务。
