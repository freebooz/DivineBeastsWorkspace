# GamePlatformSFX 性能与安全设计

版本：0.2.0｜2026-09-29

## 1. 性能原则

- 无Tick/Ticker轮询；
- 无同步资产加载；
- Definition、Sound、Attenuation、Concurrency通过GamePlatformData租约和Asset Bundle按需加载；
- 播放完成由 `OnAudioFinishedNative` 事件回收；
- RequestId用于重复请求去重和预测取消；
- 待加载记录上限128、平台实例追踪上限256，防止调用错误导致无界容器增长；
- 真正Voice数量、虚拟化、抢占和每Owner并发优先由 `USoundConcurrency` 与UE音频设备负责。

这些本地追踪上限不是目标平台Voice预算。PC/移动端最终Voice、CPU、内存预算必须用真实内容压测后批准。

## 2. 为什么不实现AudioComponent池

本轮没有自行建立AudioComponent对象池。UE音频系统已有声音并发、虚拟化、组件生命周期等能力；在没有真实5v5 Profile数据前增加自研池会增加悬空Owner、重置参数和声音状态泄漏风险。

只有 Unreal Insights / Audio Insights 证明组件创建本身成为瓶颈时，才应增加可验证的池化策略。

## 3. 资源加载

公共Request只传规范逻辑DefinitionId，不传资产路径。Definition中的音频资产全部为 `TSoftObjectPtr`，`SFXRuntime` Bundle由数据租约持有，活动实例结束后释放。

禁止 `LoadSynchronous`、构造期硬加载、Gameplay类硬引用具体SoundWave/MetaSound。

## 4. 生命周期安全

- WorldSubsystem只在客户端真实游戏世界创建；
- 异步Data回调使用弱Subsystem调用者；
- Attached Owner使用 `TWeakObjectPtr<USceneComponent>`；
- Handle带Generation和Weak World；
- World Deinitialize先使本代次失效，再停止组件和释放租约；
- AudioFinished事件释放活动租约，不依赖下一次调用触发Prune。

## 5. 参数安全

MetaSound/SoundCue请求参数当前只开放有限浮点类型，并必须由Definition白名单声明。这样避免项目层通过任意FName向底层音频实例注入未审计参数。

## 6. 权威与防作弊

SFX是纯表现机制。播放成功、失败、静音、资源缺失或用户篡改本地音频均不得改变：伤害、技能时序、命中、冷却、资产、任务、比赛结果或服务器状态。

## 7. Settings关系

SFX不持有“主音量/音效音量”的第二份用户配置。最终混音通过UE SoundClass/SoundMix/AudioModulation消费 `GamePlatformSettings` 已解析偏好；依赖应由上层组合层连接，而不是形成SFX↔Settings循环。

## 8. 复审发现的性能与生命周期风险

### 8.1 总追踪预算

当前代码分别限制 Pending=128、Active=256，但没有限制两者总和。整改后必须增加统一总预算门禁，避免“Pending未超128且Active未超256”时总量仍持续增长。

### 8.2 AudioFinished绑定时机

当前 `SpawnSound*` 会立即启动播放，而事件绑定发生在Spawn之后。生产版应改成先创建未播放的 `UAudioComponent`、绑定完成事件，再开始播放。该修改优先级高于自研对象池。

### 8.3 参数集合预算

Request和Definition的参数集合必须增加统一条目上限，避免错误内容导致大量TMap/TSet复制、遍历和组件参数写入。

### 8.4 不提前做对象池

即使整改为受控AudioComponent创建，也不等于立即实现自研池。只有Audio Insights证明组件创建/销毁是明确瓶颈时，再引入具备完整Reset合同的池化实现。

## 9. 目标性能验证指标

最终数值不在没有实测时写死，但测试至少记录：

- 同屏 Active/Pending 峰值；
- 实际 Voices 与 Virtualized Voices；
- Audio Render Thread CPU；
- Game Thread SFX创建峰值耗时；
- SFX Definition/声音资源内存；
- 预测取消与Corrected重放数量；
- Concurrency拒绝/抢占数量；
- 2D/3D/Attached分类数量；
- World切换后残留组件与租约数量必须归零。

PC与移动端分别形成基线，不直接复用同一预算。
