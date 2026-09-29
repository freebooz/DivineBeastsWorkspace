# D02｜架构与实际目录

插件属于GamePlatform第一层平台能力，不导入项目协议、生肖、MOBA、Data、ApplicationFlow或Telemetry类型。公开依赖保持Core/CoreUObject/Engine/GamePlatformCore；私有UE Transport仅依赖同层GamePlatformGameplay的中立Admission RPC载体，不依赖GamePlatformServer或项目层。DBAClient从第三层单向依赖Session，GamePlatformOnline与Session之间没有构建级双向依赖。Online认证事实由项目组合根同步给Session，而Session不会读取Token或反向调用Online。

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
│       ├── Transport/GamePlatformUESessionTransport.cpp # 默认UE ClientTravel/失败监听/准入握手适配
│       └── Tests/                            # 状态、公开契约和Subsystem集成测试
├── Tests/CMakeLists.txt                      # Debug/Release真实构建入口
└── Docs/                                     # 11份专题说明，名称见README
```

`UGamePlatformSessionClientSubsystem` 以GameInstance为作用域持有一个 `FSessionConnectionState`，Initialize时安装默认 `FGamePlatformUESessionTransport`。状态内核仍是纯C++值对象，只保存非敏感身份、代次、事实位和副本快照；网络回调由Subsystem统一切回游戏线程并使用Operation/Auth/Connection代次拒绝迟到事件。Transport只接受Gateway返回的完整ExpectedBinding，不从Ticket正文、URL或本地环境重新推导权威字段。

FOperationIdentity区分作用域、操作、尝试、账号和连接代次。FBinding保存分配、游戏会话、实例、启动、世界、协议和后端Epoch。FSnapshot按值返回，不包含凭据或端点。观察事件只在调用者已完成可信连接关联后才能进入内核；直接调用Observe的测试只是算法测试。

当前实际运行链的TransferTicket防重放与SessionEpoch生产共享状态由Redis实现：ReplayStore使用SET NX + TTL，SessionEpoch使用INCR分配并通过原子已接受Epoch栅栏拒绝旧票；本地测试使用对应内存实现。`postgresadmission.Store` 与000003迁移仍保留为历史/独立事务能力，但不是当前UE Admission Provider并行运行的第二套真源。

服务器职责归入现有 `GamePlatformServer`：模块为PostLogin后的真实PlayerController动态挂载复制握手组件，GamePlatformGameplay只承载拥有者Reliable RPC，唯一 `FGamePlatformHttpAdmissionProvider` 使用内部Bearer + GameServerId + ServerBootId调用GameServerControl验票，再把后端验证结果绑定ConnectionId/ConnectionGeneration。客户端默认Transport在新世界真实组件出现后才报告NetworkConnected，并仅在服务器返回完整且逐字段匹配ExpectedBinding的确认后报告AdmissionConfirmed。
