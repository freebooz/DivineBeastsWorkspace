# 神兽联盟公共用户界面内容包

`DBAUIPack_Core`是 DivineBeasts（神兽联盟项目层）内部的纯内容插件，负责公共用户界面二进制资产的唯一所有权。它不是第四架构层，也不重新实现 `GamePlatformUI` 或 `DBAClient` 的运行机制。

## 当前资产范围

- `/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout`：每个本地玩家的根布局，承载平台定义的HUD、WorldProjection、Feedback、Screen、Modal、Notification、Loading、System和Debug九层。
- `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login`：账号密码登录页面，仅消费 `UDivineBeastsLoginViewModel` 的只读状态和命令。
- `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_CharacterCreate`：持久角色创建页，英雄资格来自只读快照，未提交名称只存在输入控件，提交既有创建命令。
- `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_CharacterSelect`：持久角色选择页，列表来自真实档案，提交既有选择命令；两页均提供本地预览旋转与返回登录。

## 生成和修改规则

所有 Widget Blueprint、布局、样式和动画必须由 Monolith MCP 在锁定的 UE5.8 编辑器中创建或修改。C++父类、ViewModel、路由、测试和配置由项目源码维护；禁止在内容包内增加C++模块、HTTP调用或业务权威逻辑。密码不得写入资产默认值、日志或生成清单。

每次变更后必须使用 Monolith 检查Widget树和父类，执行Widget编译、保存并读取已保存状态。`Docs/MonolithGenerationManifest.json`记录工具与资产级证据；UE编译、Cook、运行和人工视觉核验仍需单独执行。

## 2026-09-28 生成记录

- Monolith 版本：`0.20.3`，工程：`DivineBeastsArena`。
- 根布局父类为 `DivineBeastsRootLayout`，共10个控件节点；9个命名层按全屏锚点和固定层级顺序配置。
- 登录页父类为 `DivineBeastsLoginScreen`，共13个控件节点；可见界面只保留标题、用户名、密码和登录按钮，不使用卡片或面板。页面、两个输入框均为黑色背景，输入框保留灰色边框和蓝色聚焦描边，登录按钮使用蓝色强调；忙碌、维护和错误反馈控件按事件需要显示。
- `PasswordInput.IsPassword=True`；账号、密码与登录按钮已配置显式键盘／手柄导航，密码默认值为空。
- 两个控件蓝图的 Monolith 编译均为0错误、0警告；登录页可访问性审计为0问题。
- CommonUI审计保留1条通用焦点属性警告：项目没有工具所寻找的`DesiredFocusTargetName`属性，而是由平台页面基类的原生焦点契约和页面目录中的`AccountInput`完成初始焦点。该警告不等同于运行验证通过，仍须在PIE中复核真实焦点。
- `DivineBeastsUIClient` Editor定向构建成功；重启编辑器后7项`DivineBeasts.UI`原生自动化测试全部通过。测试曾真实发现并促使修复初始`NAME_None`路由错误、命令完成事件被修订号变化吞掉以及未交付移动端资产路径被错误生成的问题。

以上2026-09-28记录只证明当时资产创建、编辑器编译、保存和结构回读；后续证据见生成清单的独立更新记录，不能覆盖历史验证边界。

## 2026-10-01 角色页面更新

两页分别为26/23个节点，父类为`DivineBeastsCharacterCreateScreen`和`DivineBeastsCharacterSelectScreen`。Monolith编译、保存、重启回读和独立Cook已执行。固定表单宽320、控件高36逻辑像素；视口DPI为1，窗口变化只改变锚点位置。原生事件处理不轮询业务、不直接HTTP、不保存角色权威状态。页面资源失败保留当前页面和中文错误，避免认证后只剩裸三维视口。实际运行、人工审核和移动适配边界见清单及Saved验证记录。

## 2026-10-08 现代角色布局

创建页以十二生肖磁贴选择英雄，选择页以真实档案卡片选择角色，中央继续显示项目三维预览。公共卡片资产为`UI/Components/WBP_DBA_HeroChoice`和`WBP_DBA_CharacterChoice`，父类均为`DivineBeastsCharacterChoiceEntry`；条目只广播本地显示意图，不持有后端权威档案或直接发送HTTP。页面提交前重新检查当前身份和资格，失活时注销事件并清空条目身份，忙碌时关闭输入。

页面命名控件、卡片类默认值、样式与选中边框全部通过Monolith MCP配置。创建页24节点、选择页21节点、英雄磁贴8节点、档案卡片10节点，最终编译均为0错误、0警告。尺寸固定：名称输入220×36、英雄磁贴76×60、角色卡片252×72逻辑像素；窗口变化只改变锚点位置，极小窗口仍需要独立适配验收。

已执行Editor/Client编译与真实PIE选择页预览；条目身份生命周期回归通过。UI全集为8通过、1失败，失败项是既有Combat.PlayerStatusSnapshot缺少属性集。运行仍使用Manny/Quinn开发原型；资产级保存和界面验证不代表登录到新手村准入已通过。最新证据见生成清单，不用本说明替代Cook、双客户端联机或人工签审。

## 前端客户端打包必需资源

2026-10-08最新地图限定包虽然Cook/Stage退出0，但漏掉通过原生软路径加载的RootLayout、登录页、角色页和卡片；两个运行客户端因根布局缺失显示黑屏。后续该流程使用`Build/Game/CookFrontEndClient.ps1 -EngineRoot <锁定UE5.8目录> -Cook`，它加载`FrontEndClient`专用配置并检查最终IoStore，而不以源码资产存在或编辑器可运行代替包内容交付。正式UI资产仍由Monolith拥有，修复不改Widget树或角色业务权威。

## 东方神话前端与原创品牌更新

登录页改为左上原创LOGO、中央偏下固定表单；角色页保留现代MMO布局，增加鼠标预览区域。当前创建/选择页为27/24节点，名称输入220×38；此前24/21节点和220×36为历史版本。LOGO、背景、真实UI纹理、原生输入与本地预览待机的三层归属、生成提示词及验证边界见[前端视觉与预览实施说明](Docs/前端视觉与预览实施说明.md)，最新实测结果以Monolith清单为准。
