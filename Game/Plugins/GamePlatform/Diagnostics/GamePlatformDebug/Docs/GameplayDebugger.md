# GameplayDebugger（玩法调试器）

插件依赖 UE5.8 GameplayDebugger（玩法调试器）Runtime（运行时）模块，并通过 IGameplayDebugger::RegisterCategory（注册分类）注册：
GP.World、GP.Character、GP.Ability、GP.Combat、GP.AI、GP.Navigation、GP.Network、GP.Session。

每个分类在权威端 CollectData（采集数据），只调用对应 State Provider（状态提供者），然后使用 AddTextLine（添加文本行）进入 Gameplay Debugger 原生复制通道。没有自研第二套 Debug Actor（调试Actor）复制系统。

UE5.8 Gameplay Debugger 只复制用户实际启用分类的数据，因此关闭分类不会执行该分类的高频采集。插件注册分类为 EnabledInGameAndSimulate（游戏与模拟可用），实际显示仍由 Gameplay Debugger 分类开关控制。

客户端默认使用 ' 键进入 Gameplay Debugger，并选择目标与对应 GP.* 分类。实际按键可由项目 Gameplay Debugger Settings（玩法调试器设置）调整。