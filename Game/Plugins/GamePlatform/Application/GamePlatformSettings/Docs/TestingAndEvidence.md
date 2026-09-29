# GamePlatformSettings 测试与验证证据

## 1. Runtime Automation

`Private/Tests/GamePlatformSettingsRuntimeTests.cpp` 当前覆盖：

- Descriptor 合法性和范围错误。
- 客户端分层优先级。
- 重复 SettingId Fail Closed。
- 类型安全解析和 NaN 拒绝。
- V1→V2→V3 连续 Migration。
- Migration 中途失败整体回滚。

测试前缀：`GamePlatform.Settings.Runtime.*`

## 2. Client Automation

`GamePlatformDeviceSettingsPolicyTests.cpp` 覆盖：

- 合法设备设置。
- 非法分辨率。
- NaN 帧率。
- 越界画质。
- 浮点抖动去重。

测试前缀：`GamePlatform.Settings.Policy.*`

## 3. 静态专项门禁

`Tests/Scripts/TestGamePlatformSettingsArchitecture.ps1` 检查：

- Runtime／Client／Server／Editor 四模块和 TargetAllowList。
- CanContainContent=false。
- 插件级只依赖 GamePlatformCore。
- Runtime 不形成横向领域或上层依赖。
- Server 不引入 UMG／Slate／输入／音频／UGameUserSettings。
- Client 不引入服务器／部署读取。
- 0 Tick/Ticker。
- Provider／Registry／Snapshot／Migration 关键实现存在。
- Client Async SaveGame 与 UGameUserSettings 边界。
- Server INI／Environment／CommandLine 边界。
- 必需文档和测试文件存在。

## 4. 构建验证

必须分别记录真实结果：

- Editor Development：Runtime + Client + Server + Editor。
- Client Development：Runtime + Client，Server/Editor 不进入。
- Server Development：Runtime + Server，Client/Editor 不进入。
- Shipping Client / Server：条件允许时执行，不用 Development 结果代替。
- Cook/Stage：单独验证产物，不以编译替代。

## 5. 运行验证

仍需真实执行：

- Provider 动态注册／注销后 Reload。
- User 层 Save→重启→Load。
- Profile 损坏后默认回退且原文件不覆盖。
- Migration 真实旧 Profile。
- Standalone/Packaged 显示模式 Preview/Cancel/Confirm。
- 登录→角色选择→Village→OpenWorld→MainArena→OpenWorld 跨图保持。
- Logout／切账号的 User 层刷新。
- Dedicated Server 三角色的 INI／Environment／CommandLine 覆盖与非法值拒绝。
- 多显示器／高 DPI／Android/iOS 平台差异。
- Client／Server Cook/Stage。
- 长稳和反复 Apply/Reload/Save。

所有未执行项必须明确写“未验证”，不得用源码存在或静态检查冒充。

