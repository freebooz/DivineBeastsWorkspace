# 神兽联盟自然远古界面定稿

2026-10-11用户明确将最终风格确定为自然、藤蔓、树叶、碧绿、原始与远古；此决定替代此前商代青铜主题。菜单按钮参考用户提供的石雕、根系、叶片与玉石横向牌匾。角色创建和选择仍沿用已确认的现代MMO布局，中心三维角色预览、鼠标旋转和动画职责不变。

## 资源与三层职责

全部视觉源图和界面纹理由第三层真实内容包DBAUIPack_Core拥有。GamePlatformUI仅提供中立界面能力，项目C++适配、ViewModel和路由仍由DBAClient/DivineBeastsUIClient维护；非竞技登录与新手村HUD不增加MOBA依赖。原有反射身份、控件绑定名称和旧素材保留兼容，不因视觉换肤移动英雄资源或地图。

源图位于`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Nature/`：

| 源图 | 引擎资产 | 用途 |
| --- | --- | --- |
| `NatureMenuButton.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureMenuButton` | 无文字横向菜单底图，远古石兽、藤蔓、叶片与玉石；文案由按钮子控件呈现 |
| `NaturePanelFrame.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NaturePanelFrame` | 中部透明的藤蔓叶片边框，围绕表单、信息与HUD显示区 |
| `NatureLoginBackdrop.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureLoginBackdrop` | 原始森林、石雕、碧绿水光和远古遗迹的登录背景 |
| `NatureHUDFrame.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureHUDFrame` | 轻量横向根系与玉石细边框，避免纵向装饰拉宽后遮挡状态栏 |
| `NatureAbilityDock.png` | `/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureAbilityDock` | 后续技能栏参考图定稿的一体式石兽藤蔓底座；头像、数字和技能均实时叠加 |

五份PNG由本次图像生成工具制作；引擎纹理导入、Widget调整、编译、保存与回读只能通过Monolith MCP执行。按钮底图不烘焙“开始”等文字，避免不同命令文案失真。用户已确认的神兽联盟LOGO继续使用。所有字体和用户名、密码控件的尺寸保持固定；仅画面背景铺满视口，装饰和主要按钮按固定尺寸布局。

## 技能栏参考图增量

后续用户明确提供一体式技能栏参考。底部以固定880×293装饰容纳左侧真实肖像/等级、上方绿色生命与蓝色气势、五个技能槽；中央为终极技能，顺序为普攻、主动一、终极、主动二、被动。快捷键和冷却继续来自实际已授予技能投影，不将示例Q/W/R/E/D写为未经绑定的输入。未接入的药水、卷轴和数量不制作虚假交互。右上小地图保留，状态效果按真实事件出现。

`WBP_DBA_UI_MomentumBar`是平台资源条基类的纯视觉实例，蓝色区分气势，保持既有字体11像素；不增加资源类型或玩法。平台资源条事件现在可显示`ResourceValueText`，只显示可信有限快照；未绑定、隐藏或非法数值时清空，不以资产文字伪造生命值。

## HUD旅行修复

平台`PrepareForTravel`通过`ClearHUD`清理上一世界控件，会将预置CombatHUD从HUDLayer中移除。原实现只改Visibility，因此旅行后真实头像、地图与技能组合无法重新显示。项目根布局现在通过本地PlayerController/Pawn事件恢复自己的已有CombatHUD实例，并明确传播当前或空的PlayerContext；不恢复上一世界临时HUD，不创建第二个HUD，不以Tick轮询状态。

技能条和玩家状态面板记录实际订阅的控制器，换代或退出时解绑旧来源。断开时清空上下文和旧角色显示数据。头像、小地图与技能只消费真实本地角色和复制状态，视觉资源失败不影响服务器权威。

## 验证边界

真实保存的RootLayout参与自动化回归：清空HUD后重新挂载、临时控件不复活、重复事件只保留一个组合、控制器换代解除旧委托、断开清空上下文。用户此前的笼统“HUD可见”反馈随后被“没有看到HUD”纠正，不能把旧反馈当成这次修复的旅行验收。

编译、Monolith保存重载、包内资源检查与客户端人工验收分别记录在内容包`Docs/MonolithGenerationManifest.json`及本轮Saved证据中。编辑器界面预览只证明静态布局，不证明客户端已经登录、完成旅行或联机技能机制通过。游戏窗口按用户“仅检查代码与日志”的限制保留人工操作。
