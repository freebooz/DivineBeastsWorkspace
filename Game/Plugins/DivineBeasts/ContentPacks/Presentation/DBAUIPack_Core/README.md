# 神兽联盟公共用户界面内容包

`DBAUIPack_Core`是 DivineBeasts（神兽联盟项目层）内部的纯内容插件，负责公共用户界面二进制资产的唯一所有权。它不是第四架构层，也不重新实现 `GamePlatformUI` 或 `DBAClient` 的运行机制。

## 当前资产范围

- `/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout`：每个本地玩家的根布局，承载平台定义的HUD、Screen、Modal、System、Notification、Loading和Debug层。
- `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login`：账号密码登录页面，仅消费 `UDivineBeastsLoginViewModel` 的只读状态和命令。

## 生成和修改规则

所有 Widget Blueprint、布局、样式和动画必须由 Monolith MCP 在锁定的 UE5.8 编辑器中创建或修改。C++父类、ViewModel、路由、测试和配置由项目源码维护；禁止在内容包内增加C++模块、HTTP调用或业务权威逻辑。密码不得写入资产默认值、日志或生成清单。

每次变更后必须使用 Monolith 检查Widget树和父类，执行Widget编译、保存并读取已保存状态。`Docs/MonolithGenerationManifest.json`记录工具与资产级证据；UE编译、Cook、运行和人工视觉核验仍需单独执行。
