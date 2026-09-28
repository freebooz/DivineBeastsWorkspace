# 神兽联盟公共用户界面内容包

`DBAUIPack_Core`是 DivineBeasts（神兽联盟项目层）内部的纯内容插件，负责公共用户界面二进制资产的唯一所有权。它不是第四架构层，也不重新实现 `GamePlatformUI` 或 `DBAClient` 的运行机制。

## 当前资产范围

- `/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout`：每个本地玩家的根布局，承载平台定义的HUD、Screen、Modal、System、Notification、Loading和Debug层。
- `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login`：账号密码登录页面，仅消费 `UDivineBeastsLoginViewModel` 的只读状态和命令。

## 生成和修改规则

所有 Widget Blueprint、布局、样式和动画必须由 Monolith MCP 在锁定的 UE5.8 编辑器中创建或修改。C++父类、ViewModel、路由、测试和配置由项目源码维护；禁止在内容包内增加C++模块、HTTP调用或业务权威逻辑。密码不得写入资产默认值、日志或生成清单。

每次变更后必须使用 Monolith 检查Widget树和父类，执行Widget编译、保存并读取已保存状态。`Docs/MonolithGenerationManifest.json`记录工具与资产级证据；UE编译、Cook、运行和人工视觉核验仍需单独执行。

## 2026-09-28 生成记录

- Monolith 版本：`0.20.3`，工程：`DivineBeastsArena`。
- 根布局父类为 `DivineBeastsRootLayout`，共10个控件节点；9个命名层按全屏锚点和固定层级顺序配置。
- 登录页父类为 `DivineBeastsLoginScreen`，共17个控件节点；账号、密码、提交、忙碌、维护和错误反馈均使用C++约定的命名控件。
- `PasswordInput.IsPassword=True`；账号、密码与登录按钮已配置显式键盘／手柄导航，密码默认值为空。
- 两个控件蓝图的 Monolith 编译均为0错误、0警告；登录页可访问性审计为0问题。
- CommonUI审计保留1条通用焦点属性警告：项目没有工具所寻找的`DesiredFocusTargetName`属性，而是由平台页面基类的原生焦点契约和页面目录中的`AccountInput`完成初始焦点。该警告不等同于运行验证通过，仍须在PIE中复核真实焦点。

本记录只证明资产创建、编辑器编译、保存和结构回读。当前尚未完成PIE登录交互、真实后端认证、客户端Cook、移动设备适配或人工视觉签审。
