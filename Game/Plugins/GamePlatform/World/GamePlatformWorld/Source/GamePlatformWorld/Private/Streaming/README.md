# World流送实现与UE5.8支持边界

本目录只实现当前UWorld的流送协调，不分配服务器、不切换地图、不创建子关卡实例、不改变Data Layer、不承诺Region全部Actor或导航/物理/复制/业务已就绪。公开值类型在`Public/Types/GamePlatformWorldStreaming.h`，私有入口是`FGamePlatformWorldStreaming`。

## 调用契约

- 全部方法、析构、Owner检查和引擎源回调仅游戏线程。构造传入真实World及其非空ContextGeneration；仅Game/PIE，拒绝Commandlet。DedicatedServer不是独立世界类型；服务器另受WP源流送开关约束。
- Request返回有效句柄和FGamePlatformResult::Success只表示接受，初始状态总为Pending。输入拒绝返回空句柄，不新增屏障；调用者必须处理OutResult，不能忽略必需请求的提交失败。
- Owner是弱引用，必须是该World、GetWorld指向该World的对象或该World的Outer子对象，不接受跨世界、CDO/Archetype对象。不会通过请求为Owner、World或关卡额外保活。
- 调用方在发布就绪快照前调用Tick。GetState/IsRequiredReady/HasRequiredFailure读取最近采样状态，不替代引擎流送Tick。没有引擎流送更新就不能凭等待时间获得Ready。
- Ready代表当前采样时真实底层满足需求；不释放源。后续底层失去驻留可退回Pending，并为这一段连续Pending重新开始TimeoutSeconds预算。Owner失效、明确加载失败、外部撤销/替换、超时转Failed；Failed不自动重试。
- 超时使用FPlatformTime::Seconds单调墙钟，不依赖世界TimeDilation或游戏是否暂停。暂停而仍调用本协调器Tick时也计时。
- 显式Cancel撤销本项需求及本项就绪义务，精确已知句柄可重复取消。自动失败仍留在必需屏障，直到调用者明确Cancel处理；不要依赖丢失Owner来悄悄删除失败。
- 无必需需求时IsRequiredReady为真，这只代表流送屏障，不是WorldReady。关闭后始终为假。可选请求失败不使HasRequiredFailure为真。
- 绑定OnWorldBeginTearDown立即关闭，避免未BeginPlay的World跳过OnWorldEndPlay。Shutdown幂等，先停止接受、解绑委托，再注销自己的Provider；析构再次确保清理。
- 每次接受创建独立RequestId；未知、跨代次、其他协调器的句柄失败。终止记录保留到协调器析构供GetState与幂等取消；当前锁定接口没有Forget，记录数量随该上下文累计请求数增长，长期高频预取场景需要后续明确历史保留契约，不能无限频率逐帧提交新请求。

## World Partition

使用且只包含Engine公开头：`WorldPartition/WorldPartitionStreamingSource.h`、`WorldPartition/WorldPartitionSubsystem.h`、`WorldPartition/WorldPartition.h`。

核对本机5.8.0源树的真实签名：

```cpp
virtual bool IWorldPartitionStreamingSourceProvider::GetStreamingSource(FWorldPartitionStreamingSource&) const;
virtual const UObject* IWorldPartitionStreamingSourceProvider::GetStreamingSourceOwner() const;
void UWorldPartitionSubsystem::RegisterStreamingSourceProvider(IWorldPartitionStreamingSourceProvider*);
bool UWorldPartitionSubsystem::IsStreamingSourceProviderRegistered(IWorldPartitionStreamingSourceProvider*) const;
bool UWorldPartitionSubsystem::UnregisterStreamingSourceProvider(IWorldPartitionStreamingSourceProvider*);
bool UWorldPartitionSubsystem::IsStreamingCompleted(const IWorldPartitionStreamingSourceProvider* = nullptr) const;
void UWorldPartitionSubsystem::ForEachWorldPartition(TFunctionRef<bool(UWorldPartition*)>);
const TArray<FWorldPartitionStreamingSource>& UWorldPartition::GetStreamingSources() const;
bool UWorldPartition::IsInitialized() const;
bool UWorldPartition::CanStream() const;
bool UWorldPartition::IsStreamingEnabled() const;
bool UWorldPartition::IsServer() const;
bool UWorldPartition::IsServerStreamingEnabled() const;
```

Provider接口不是UObject且没有虚析构。每个请求持有具体Provider类型的TUniquePtr，地址稳定；引擎仅登记借用指针。必须先Unregister再销毁，不通过接口指针delete。释放仅撤销自己的源，由引擎聚合其余源和Data Layer决定最终驻留，不直接卸载cell。

Request.LevelPackage必须为空；TargetLocation是世界坐标。Priority采用引擎0最高、255最低范围。未暴露Shapes/TargetGrids/半径：使用引擎默认球形网格加载范围，适用于所有目标网格；不会捏造精确Region包围体。

完成条件不止Register成功：主WP必须登记、初始化、CanStream；本请求Name必须已进入该WP运行策略的GetStreamingSources；Provider仍登记且Owner有效；最后才调用官方IsStreamingCompleted(provider)。这样避免无Provider源、无已登记分区或无运行策略时官方空查询返回true被误用。

官方完成查询按当前运行Data Layer及源覆盖内容判断，Activated可满足Loaded；不负责激活禁用Data Layer，空空间没有匹配cell也可以完成。它不是“这个业务Region存在且其中一切已就绪”的证明；Region Provider/权威碰撞/导航/业务需独立贡献者验证。源被过滤、运行策略未消费、引擎尚未更新时保持Pending，最终可超时。

WP禁用流送，或Listen/Dedicated服务器未启用源驱动流送时，返回Unsupported（WorldStreamingUnsupportedPartition）。不修改CVar/地图设置，不将服务器全常驻策略伪装成按源流送成功。传统地图不受此WP专有限制。

## 传统Level Streaming

只查找World.GetStreamingLevels内已有对象，使用完整长包名和UWorld::RemovePIEPrefix比较；同包匹配多个实例必须报歧义，不挑第一个。主PersistentLevel不是已登记流送关卡，不由此接口加载。

调用SetShouldBeLoaded(true)，Activated额外调用SetShouldBeVisible(true)；Loaded不会隐藏已有可见关卡。Priority转换为255-Priority，取与现有值的最大值，只提升优先级。UE5.8按更高传统优先级先考虑，WP则数值越小越高。

Loaded要求IsLevelLoaded且当前状态为LoadedNotVisible或LoadedVisible；Activated要求LoadedVisible、IsLevelLoaded、IsLevelVisible全部成立。不使用HasLoadedLevel（包含PendingUnloadLevel），不以ShouldBeLoaded/ShouldBeVisible代替完成。

引擎没有可供本包装独占的传统加载/可见性需求租约。Cancel、失败及Shutdown都不会写false、不会卸载/删除对象、不会恢复旧priority/visibility。即使最后一个自己的请求释放，提升的标志也保留；是否最终卸载由原关卡所有者决定。无法恢复external修改，也不尝试连续回写争夺控制权；观察到包重定向、移除、撤回加载/所需可见性时明确失败。外部优先级调整不反复覆盖。

## 世界子系统集成提醒

UE5.8 UWorldSubsystem默认支持Game、Editor、PIE；项目须自行缩窄到Game/PIE并另判IsRunningCommandlet（没有Commandlet世界类型）。预览世界不可隐式启动业务。PostInitialize和OnWorldBeginPlay的override仍应调用Super。

真实World.cpp中：BeginTearingDown先置bIsTearingDown再广播OnWorldBeginTearDown；未BeginPlay时EndPlay可提前返回；已BeginPlay时Actor EndPlay先于Subsystem.OnWorldEndPlay。后者顺序与WorldSubsystem.h注释不一致，不能仅按注释假设子系统先于Actor关闭。资源清理阶段PreDeinitialize先于SubsystemCollection.Deinitialize。

## 验证证据与未执行项

2026-09-21：先新增`Private/Tests/WorldStreamingTests.cpp`，再新增生产类型及实现；后续补充最小兼容和可选失败用例。共8个UE自动化测试源码，测试命名空间`GamePlatform.World.Streaming`。测试只建立瞬态World/已有空加载器，验证协调副作用；没有生成uasset/umap，不把空关卡伪装成真实加载。

本轮只核对公开头及必要引擎实现、执行文本差异检查。没有运行UE/UHT/UBT、没有执行上述8个测试、没有完成运行意义的red/green；未新增纯算法测试或修改主CMake。历史空插件描述仍原位保留，主工程和引擎构建阻断未解除。

待真实UE环境验证：WP源实际注册/策略消费/Loaded与Activated完成/过滤/数据层/撤销后的多Owner驻留；传统真实关卡Loaded与Visible及加载失败；多PIE/旅行/暂停/Owner GC/专服与监听服务器；Ready退化与超时；所有委托在世界关闭后的清理。测试源码不等于这些集成项目已通过。
