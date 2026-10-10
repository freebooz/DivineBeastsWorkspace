# 通用天气架构与契约

## 依赖方向

`DBAWorldsRuntime`（第三层） → `GamePlatformWeatherRuntime`（第一层） → `GamePlatformData/Core`（平台公共基础）。
`GamePlatformWeatherClient`（平台客户端模块） → `GamePlatformWeatherRuntime`＋`GamePlatformSurfaceClient`＋`GamePlatformPresentationClient/Core`。

Runtime不得依赖Client，Server Target不得装配WeatherClient；没有Weather→DBAWorlds反向依赖。纯项目天气实例和美术资源不得放入平台插件。

## 生命周期与关键路径

1. 可信项目世界GameMode在BeginPlay确定激活天气；不在模块StartupModule自动Spawn或在前端强制开天气。
2. 服务器WorldSubsystem生成至多一个`AGamePlatformWeatherReplicator`，推送Revision=1初始清晰快照。
3. `SetWeather`校验状态、时长及调用方服务器资格；计算当前过渡中间态作为From，To为目标量化状态，Revision单调增加并ForceNetUpdate。
4. 客户端Actor BeginPlay/OnRep将快照投影给同一World的Runtime天气服务。客户端WeatherClient订阅事件，仅在过渡期短时启动定时器。
5. 客户端Sample按服务器时钟重建雨雪/湿润/积雪等值；Surface负责写MPC，Presentation提交VFX/SFX语义。缺资源只影响表现，不反写服务器天气。
6. 天气停止或世界退出清理Timer、绑定和已发出的持久表现请求；旧实例/旧请求不得重新生效。

## 服务接口

- `ActivateWeather(InitialState)`：仅服务器；同世界重复激活为幂等成功，不重置已运行的天气。
- `SetWeather(TargetState, TransitionSeconds)`：仅服务器；非法状态、时间或世界返回false，成功则取消自动调度；过渡重入从服务器当下采样状态切换。
- `ApplyPreset(Preset)`：调用方保持数据租约存活；无效定义明确失败。
- `ConfigureSchedule(Entries, RandomSeed)`：条目1..32，权重1..1000，最小持续>=1s，最大不超过86400s，过渡0..3600s；先整体校验再替换，失败不改变原表。
- `StartSchedule/StopSchedule`：仅已激活服务器可启动；只保留一个下一轮定时器；停止不会删除当前权威天气。
- `GetCurrentSnapshot/AddSnapshotHandler/RemoveSnapshotHandler`：本世界当前天气只读快照与明确订阅/解除。
- `GetVisualWeather/AddVisualChangedHandler/RemoveVisualChangedHandler`：客户端本世界天气视觉值与适配订阅；不能做权威玩法判定。

## 网络与异常

不对玩家提供修改天气的RPC。单世界单复制Actor（bAlwaysRelevant），低频按状态变化复制；不逐帧同步。晚加入者从Actor初始快照重建。客户端不运行调度种子。Revision阻止网络旧状态倒退；跨世界新建Subsystem隔离。无有效GameState时世界秒数仅临时回退；联网人工验收应确认服务器时钟就绪。失败的表面资源与Provider应记录真实诊断，不能把`Submitted`或`ApplyEnvironmentState`当GPU效果验证。

## V1不包含

跨分片全局气象服务、分区域FastArray天气、季节/昼夜控制、动态改变物理摩擦/寻路/战斗结算及基于地理天气数据的实时外部抓取。扩展这些职责前单独更新接口、协议与完整三目标构建/网络回归。
