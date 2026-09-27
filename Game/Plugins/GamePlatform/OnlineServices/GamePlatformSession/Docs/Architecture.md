# D02｜架构与实际目录

插件属于GameFoundation平台层，不导入项目协议、生肖、MOBA、Data或应用流程类型。当前模块→UE Core仅用于模块身份注册，箭头表示代码依赖。没有Online→Session反向依赖，因为本轮Online公开接口根本不存在，双方业务适配尚未建立。

```text
GamePlatformSession/                          # 平台会话插件根
├── GamePlatformSession.uplugin               # 唯一模块，Client/Editor允许、Server排除
├── README.md                                 # 交付边界和12文档入口
├── Source/GamePlatformSession/               # 唯一模块源码
│   ├── GamePlatformSession.Build.cs          # 最小依赖及目标失败保护
│   └── Private/                              # 没有稳定公开服务，内核不外泄
│       ├── GamePlatformSessionModule.cpp     # 仅注册模块，无自动连接
│       ├── State/                            # 无引擎依赖的真实状态算法
│       │   ├── SessionConnectionState.h      # 状态/身份/值快照及内部接口
│       │   └── SessionConnectionState.cpp    # 代次、就绪、取消与结束规则
│       └── Tests/SessionStateTests.cpp       # CMake实际编译的原生测试，宏隔离main
├── Tests/CMakeLists.txt                      # Debug/Release真实构建入口
└── Docs/                                     # 11份专题说明，名称见README
```

FSessionConnectionState每个调用者独立构造，内部只保存非敏感字符串、代次、事实位和副本快照。它不是进程单例、不是网络复制对象、也不是UGameInstanceSubsystem。尚未实现的子系统和Online/Travel适配器不以空类占位。完成接入后应由游戏实例作用域持有状态内核，所有变更在游戏线程串行执行；当前类型本身不提供锁或跨线程安全承诺。

FOperationIdentity区分作用域、操作、尝试、账号和连接代次。FBinding保存分配、游戏会话、实例、启动、世界、协议和后端Epoch。FSnapshot按值返回，不包含凭据或端点。观察事件只在调用者已完成可信连接关联后才能进入内核；直接调用Observe的测试只是算法测试。

后端持久化属于Backend基础设施：postgresadmission.Store调用同一个000003迁移中的事务函数。没有内存仓储冒充生产共享状态。当前数据库内核串行化全部准入写操作，可靠性优先，但吞吐、分片和权威租约续租未验证。

主工程SessionIntegration服务器夹具尚未创建：Online缺失，真实握手如何唯一关联UNetConnection也未验证。不得把数据库中的测试实例记录解释成UE注册就绪。未来夹具只负责开发场景的真实网络和服务器身份接入，不成为第二个插件；正式服务器能力将迁移到未来GamePlatformServer。
