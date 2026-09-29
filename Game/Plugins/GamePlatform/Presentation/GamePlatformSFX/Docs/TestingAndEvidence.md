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

覆盖：规范Definition逻辑ID、禁止资产路径、请求参数范围。

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

以下结果将在本轮真实执行后更新：

```text
静态架构门禁：待执行
GamePlatformSFX范围 git diff --check：待执行
GamePlatformSFXClient Editor模块编译：待执行
Client Target编译：待执行
Server隔离构建：待执行
UE Automation：待执行
真实音频资产播放：未执行
5v5 Audio Insights性能实测：未执行
```

未执行项不得写成通过。
