# GamePlatformSettings 测试与验证证据

## 1. Runtime Automation

`Private/Tests/GamePlatformSettingsRuntimeTests.cpp` 当前覆盖：

- Descriptor 合法性和范围错误。
- 客户端分层优先级。
- 重复 SettingId Fail Closed。
- 类型安全解析和 NaN 拒绝。
- V1→V2→V3 连续 Migration。
- Migration 中途失败整体回滚。
- 敏感 Descriptor 进入 User 持久化作用域时拒绝。
- 超过4096字符的 String 文本解析拒绝。
- Server持久化作用域错误声明为Client时拒绝。
- ServerDefault未同时绑定Server运行端侧与Server持久化作用域时拒绝。

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
- 异步 Save 期间 Provider 拓扑变化存在延迟重载保护。
- 敏感持久化／Server敏感覆盖／String容量安全边界存在。
- User/Server持久化端侧唯一性和ServerDefault端侧隔离存在。
- Server环境变量规范化键冲突检测存在。
- Client User Profile 必须使用不透明用户上下文的哈希槽隔离；无用户上下文时拒绝保存。
- Runtime 通过中立 `SwitchUserContext` 接口切换账号上下文，源码不得依赖 Online/Session/Server/Telemetry/Presentation。
- Server敏感Descriptor仅在真实外部覆盖尝试时拒绝，不因定义本身误阻断。
- 没有生产 `IGamePlatformSettingsProvider` 时输出成熟度 WARNING，但不把纯框架判为结构失败。

## 4. 构建验证

当前本轮定向 UBT 编译尝试返回 `ConflictingInstance`：另一个构建进程仍持有 UE5.8 全局 UBT Mutex，因此 Settings 编译没有启动。这不是模块编译错误，也不能记为编译通过。

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
- 登录／角色切换／跨地图／跨体验切换时 GameInstance Snapshot 保持。
- Logout／切账号的 User 层刷新。
- 两个不同 UserContextKey 在同一设备生成不同本地档案槽；Logout 空键不加载旧账号档案；脏数据或 SaveInFlight 时切换被拒绝。
- 启用 Settings 的 Dedicated Server 测试配置中验证 INI／Environment／CommandLine 覆盖、优先级、非法值与敏感值拒绝。
- 多显示器／高 DPI／Android/iOS 平台差异。
- Client／Server Cook/Stage。
- 长稳和反复 Apply/Reload/Save。

所有未执行项必须明确写“未验证”，不得用源码存在或静态检查冒充。

