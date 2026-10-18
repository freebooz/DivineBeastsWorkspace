# SSLM 全插件运行期增量审查记录（2026-10-18）

工程：DivineBeastsWorkspace（SSLM工作空间），UE5.8（虚幻引擎）。本轮仅记录实际检查与源码整改；不把静态测试、C++语法检查、地图包头或待审任务当作正式功能/人工验收。

## 已修复的真实问题

**GamePlatformSession（跨服会话插件，高优先级）**：修复BeginTransfer（开始连接）同步失败/取消后错误返回成功，状态事件内部同步取消后仍继续创建传输，以及旅行/准入完成回调可能同步清空正在执行的TFunction（可调用对象）所造成的生命周期风险。真实传输对象以共享引用保护调用栈，操作身份不匹配时不再提交准入或ClientTravel（客户端地图旅行）。原生测试源码补充同步错误、取消与旧回调。受影响3个C++单元均经锁定MSVC/UE5.8响应文件完成 /Zs（语法）检查。

**GamePlatformVFX（平台视觉特效，性能优化）**：鸟群实例化网格Actor按物种5～60Hz UpdateRateHz（更新频率）设置原生TickInterval（更新周期），不再每游戏帧执行空刷新；保留积累时间尾数。修复旧鸟群源码检查器，转而检查现在的统一EnvironmentFlock（鸟/蝶/鱼环境群落）资源租约、Owner（所有者）令牌、取消和世界退出逻辑。相关2个C++单元语法检查通过。

**GamePlatformTelemetry（平台遥测，长期运行内存风险）**：网络重试一次性Ticker（定时器）的句柄改为触发时即移出受管数组，避免队列长期保持Pending（待完成）时积累历史句柄；调度失败明确完成批次。Shutdown（关停）增加重入幂等控制和预算回调注册失败的安全收口。原生测试源码增加RetryHandleLifecycle（重试句柄生命周期）、ShutdownIdempotent（关停幂等）两项，C++实现与测试2个编译单元语法检查通过。

**GamePlatformSave（平台客户端本地存档，写入容量及失败清理）**：底层`FGamePlatformSaveStorage::WriteAtomically`（本地原子写入）此前只依赖上游`EncodeRecord`（编码策略）做大小校验，若未来独立调用底层写入可在超限时触发磁盘IO。本批增加`GetMaxEncodedBytes`（单记录编码后大小预算）的写入边界，在创建目录/临时文件/主档备份前拒绝超限，并统一返回`SaveFileTooLarge`。另外在`CopyFile`（备份复制）失败分支清理可能遗留的本次`.bak.tmp`临时文件，保留原主档和旧备份。`GamePlatform.Save.Storage.RejectsOversizedWrite`新增随机隔离目录的UE原生测试源码，验证字节保留、没有新备份和无超限替换；`GamePlatformSaveClient`（存档客户端模块）现有Unity编译单元以锁定MSVC/UE5.8响应文件通过`/Zs`（仅语法）；**最新原生UE自动化、独立客户端完整编译/Cook尚未执行**。另有待调查风险：`CancelPendingRequests`（取消待完成请求）释放本实例忙碌槽位后，已投递的后台IO仍可能执行；需结合不同GameInstance同一物理Key的代次和竞态测试明确处理，不能视为已修复。

## 已执行的验证

- Session（会话）独立原生内核：Debug/Release（调试/发布）各编译成功，CTest各1/1项通过；原始日志位于 `Saved/Validation/GamePlatformSession/Verify-97fdde22a1d64ffc8ba7d7fbd5ecb60e`。脚本修复系统未配置CMake（构建工具）命令导致的假阻断；综合联调门禁仍按设计返回2。
- 鸟群发布策略Python回归43/43通过；合成Cook（资源烘焙）规则3/3通过；修复后的统一鸟群源码集成检查通过。
- 上轮跨插件运行安全静态回归10/10通过；加入Save写入及进程内跨GameInstance租约安全合同后，运行安全静态回归12/12通过；GamePlatformSave架构静态门禁退出0；当前67插件台账/工作单重新生成与一致性核对退出0；Telemetry（遥测）架构门禁通过；平台模块图2项、目标装配17项、服务器生命周期5项通过。
- Ticker静态盘点：生产代码21个文件、23处注册，33处注销调用；该计数不代表路径已在所有异常/断线场景实际执行。

## 仍然阻断正式生产验收

新增Session/Telemetry UE原生Automation（自动化）尚未执行；最新代码未完成正式UBT编译链接、真实网络/多人测试、Niagara GPU性能、客户端/专用服务器独立Cook和人工视觉点击验收。现有DemoHub（插件演示中心）可映射19插件至13张诊断地图，但53个需视觉交互域中仍有51个未登记正式可操作案例，人工签收为0。活动UE Editor（编辑器）及他人的未提交资源均未被本批抢占或覆盖，未提交主分支。
## 2026-10-18 续接：GamePlatformSave跨GameInstance共享槽位防并发

- 上文存档段落所列“旧实例取消后释放忙碌表、后台IO仍在进行”的风险已做源码级补强：新增私有`FGamePlatformSaveProcessSlotLease`（进程物理槽位租约），使用互斥的稳定StorageId登记表和线程安全共享所有权，`BeginRequest`（申请）发现另一个GameInstance仍持有同一实际存档路径时返回`SaveSlotBusy`。Save、Load、Delete的线程池任务均分别捕获本次租约，旧GameInstance撤销Pending与Callback不能提前解锁仍运行的IO。
- 原生测试源码增加`GamePlatform.Save.Storage.ProcessSlotLeaseLifecycle`（租约生命周期），检查同槽拒绝、异槽并行、Owner销毁/Worker仍持有、真实任务最后引用释放后再获取。此测试当前仅已编写，尚未在UE引擎实际执行。
- 本批真实对`GamePlatformSaveClient`（存档客户端模块）Unity编译单元使用锁定UE5.8/MSVC与缓存UBT响应文件执行`/Zs`（C++语法检查），**经保持UHT生成宏对应行号后退出码0**。这不是正式UHT重新生成、UE模块编译链接、独立Client或多实例写入验收。
- 此方案仅支持单进程不同GameInstance。**跨独立Client进程共用ProjectSavedDir仍无系统级文件互斥保证**；应在发布配置隔离数据根，并单独确定是否需要OS级文件锁。真实文件读写/取消竞态、失败备份恢复与完整Cook继续阻断发布。
- 2026-10-18受控尝试使用`Build/Game/BuildFoundation.ps1 -Editor -OnlyModules GamePlatformSaveClient -AdditionalPlugins GamePlatformSave`（存档模块定向正式构建），工程互斥锁发现另一Foundation构建或Cook占用，**返回2 / NotExecuted（未执行）**，并没有执行最新UE模块编译；证据：`Saved/Validation/FoundationM0/73cc9707-b535-410f-b72c-fffa64093888/Build`。未强制解除锁或停止其他任务。
