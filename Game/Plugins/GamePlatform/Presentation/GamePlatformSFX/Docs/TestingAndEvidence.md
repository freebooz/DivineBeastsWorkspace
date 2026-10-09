# GamePlatformSFX 测试与验证证据

版本：0.2.0｜2026-09-29

本文件严格区分源码存在、静态门禁、编译、UE Automation、真实音频资产运行和性能实测。

## 1. UE Automation源码

当前策略测试：

```text
Source/GamePlatformSFXClient/Private/Tests/GamePlatformSFXPolicyTests.cpp
```

测试前缀：

```text
GamePlatform.SFX.*
```

覆盖：规范Definition逻辑ID、禁止资产路径、请求参数范围；新增GamePlatformSFXLifecycleTests.cpp覆盖Stopped回收、终态容量/期限与先取消。新增Native总预算测试已观察RED→GREEN（128 Pending及256总槽边界）。新增UE用例尚未执行。

## 2. 静态架构门禁

入口：

```text
Tests/Scripts/TestGamePlatformSFXArchitecture.ps1
```

检查ClientOnly/目标允许列表、依赖方向、无DivineBeasts/Moba反向依赖、无Tick/Ticker、无LoadSynchronous、软SoundBase、GamePlatformData租约、AudioFinished事件、Presentation SFX Provider与正式文档。

## 3. 需要真实验证的运行场景

- Definition和SFXRuntime Bundle异步加载；
- SoundWave / SoundCue / MetaSound Source；
- 2D、World、Attached；
- Owner销毁；
- World切换；
- Stop重复调用；
- Prediction Cancelled；
- Concurrency抢占与虚拟化；
- Attenuation与Occlusion；
- 资源缺失/Definition非法；
- 5v5高密度技能声音；
- Client Cook；
- Dedicated Server不链接GamePlatformSFXClient；
- 不同设备与音频输出环境。

## 4. 当前证据状态

本轮复审实际证据：

```text
静态架构门禁：已执行，通过
  TestGamePlatformSFXArchitecture.ps1，退出码0。

GamePlatformSFX范围 git diff --check：已执行，通过
  仅出现工作区LF/CRLF转换提示，没有diff格式错误。

GamePlatformSFXClient Editor模块编译：本轮未执行
Client Target编译：本轮未执行
Server隔离构建：本轮未执行
UE Automation：本轮未执行
真实音频资产播放：未执行
5v5 Audio Insights性能实测：未执行
```

静态门禁通过只证明当前结构基线，不证明P1生命周期问题不存在，也不等于编译、运行或性能验收通过。

## 5. 下一轮必须新增的测试

- 极短SoundWave/MetaSound：验证不会错过AudioFinished导致组件和Lease滞留；
- StartTime接近声音尾部：验证立即结束仍可清理；
- Pending + Active总预算：验证总量硬门禁；
- 128个Pending附近并发完成：验证不会突破总追踪上限；
- Definition参数数量超限；
- Request参数数量超限；
- Attached Owner在租约完成前销毁；
- World teardown与Data Completion同一调度窗口竞争；
- Stop重复调用、FadeOut后二次Stop；
- Predicted → Confirmed同RequestId去重；
- Predicted → Corrected停止旧实例并重放；
- Predicted → Cancelled只停止不重播；
- 两个LocalPlayer同一World下2D SFX行为；
- Dedicated Server Target确认不链接GamePlatformSFXClient；
- Client Cook确认纯SFX资源按需要进入包。

未执行项不得写成通过。
