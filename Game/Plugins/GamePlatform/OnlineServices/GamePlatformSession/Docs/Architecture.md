# D02｜架构与实际目录

插件属于GamePlatform第一层平台能力，不导入项目协议、生肖、MOBA、Data、ApplicationFlow或Telemetry类型。模块只依赖Core/CoreUObject/Engine/GamePlatformCore；DBAClient从第三层单向依赖Session，GamePlatformOnline与Session之间没有构建级双向依赖。Online认证事实由项目组合根同步给Session，而Session不会读取Token或反向调用Online。

```text
GamePlatformSession/                          # 平台会话插件根
├── GamePlatformSession.uplugin               # 唯一模块，Client/Editor允许、Server排除
├── README.md                                 # 交付边界和12文档入口
├── Source/GamePlatformSession/               # 唯一模块源码
│   ├── GamePlatformSession.Build.cs          # 最小依赖及目标失败保护
│   ├── Public/                               # 客户端Subsystem、Transport契约和稳定错误码
│   └── Private/                              # 状态内核、适配实现和自动化测试
│       ├── GamePlatformSessionModule.cpp     # 仅注册模块，无自动连接
│       ├── State/                            # 无引擎依赖的真实状态算法
│       │   ├── SessionConnectionState.h      # 状态/身份/值快照及内部接口
│       │   └── SessionConnectionState.cpp    # 代次、就绪、取消与结束规则
│       └── Tests/SessionStateTests.cpp       # CMake实际编译的原生测试，宏隔离main
├── Tests/CMakeLists.txt                      # Debug/Release真实构建入口
└── Docs/                                     # 11份专题说明，名称见README
```

`UGamePlatformSessionClientSubsystem` 以GameInstance为作用域持有一个 `FSessionConnectionState`。状态内核仍是纯C++值对象，只保存非敏感身份、代次、事实位和副本快照；网络回调由Subsystem统一切回游戏线程并使用Operation/Auth/Connection代次拒绝迟到事件。Transport通过接口注入，当前没有在缺少可信完整Binding时创建伪实现。

FOperationIdentity区分作用域、操作、尝试、账号和连接代次。FBinding保存分配、游戏会话、实例、启动、世界、协议和后端Epoch。FSnapshot按值返回，不包含凭据或端点。观察事件只在调用者已完成可信连接关联后才能进入内核；直接调用Observe的测试只是算法测试。

后端持久化属于Backend基础设施：postgresadmission.Store调用同一个000003迁移中的事务函数。没有内存仓储冒充生产共享状态。当前数据库内核串行化全部准入写操作，可靠性优先，但吞吐、分片和权威租约续租未验证。

服务器职责已经归入现有 `GamePlatformServer`，并新增Admission Subsystem/Provider边界；它要求真实PlayerController、ConnectionId、ServerBootId和一次性证明，拒绝客户端自报身份。但生产Admission Provider与真实NetConnection关联尚未实现。Backend的Gateway世界进入与GameServerControl分配/签票已接通，仍需扩展可信Boot/Epoch/协议绑定后才能完成Session Transport。
