# GamePlatformCamera 实现规格

> 状态：P0-7 设计规格，尚未实施。
> 目标：建立 LocalPlayer 作用域、完全客户端、Definition 驱动的 3A 相机框架。

## 1. 现状

当前仅有 `GamePlatformCameraClient（ClientOnly）` 模块注册壳，无正式 CameraMode、Stack、Definition、Target、Blend、测试和 Review。

## 2. 职责

平台相机负责：
- Camera Mode（相机模式）。
- Mode Stack（模式栈）和优先级。
- Blend Policy（混合策略）。
- FOV／Arm／Offset。
- Collision／Camera Lag。
- LookAt／Target。
- Lock-on 视觉跟随。
- Camera Impulse／Shake 的统一入口。
- Exploration／Combat／Arena／Spectator／Death 等中立模式类型。
- LocalPlayer 隔离。

不负责：
- 服务器权威瞄准。
- 命中判定。
- 竞技规则。
- 神兽联盟英雄名字或项目地图。
- 重新实现 UE CameraManager、CameraModifier、CameraShake 底层。

## 3. 推荐类型

```text
UGamePlatformCameraModeDefinition
    : UGamePlatformDefinitionBase
```

建议字段：
- ModeId。
- Priority。
- BlendIn／BlendOut。
- FOV。
- Relative Offset／Pivot Policy。
- Collision Policy。
- Lag Policy。
- Target Policy。
- Input Policy。
- Quality／Platform Variant。

运行时：
- `UGamePlatformCameraLocalPlayerSubsystem`：LocalPlayer 作用域，拥有 Mode Stack；若实际测试证明 PlayerCameraManager 更合适，可由 Provider 适配，不建立进程单例。
- `FGamePlatformCameraModeHandle`：作用域 + 代次，防旧句柄操作新世界。
- `IGamePlatformCameraTargetProvider`：提供受控目标，避免 Camera 依赖具体 Character。
- `IGamePlatformCameraProvider`：连接 UE PlayerCameraManager／CameraComponent。

## 4. Presentation 集成

Gameplay／Moba／项目层只提交中立语义：
- Camera.Impact
- Camera.LockOn
- Camera.Arena.Intro
- Camera.Death
- Camera.Spectator

Camera Provider 根据 Context 和 Camera Profile 决定实际模式或 Shake。

Presentation 失败不改变玩法。

## 5. 项目扩展

一般项目差异只创建：
- Camera Mode DataAsset。
- Hero Camera Profile。
- Arena Camera Profile。
- World Camera Profile。

只有确有额外结构时才建立 `UDivineBeastsCameraModeDefinition`；禁止每个英雄一个 Camera C++ 类。

## 6. 生命周期

- GameInstance/LocalPlayer/World 变化时明确撤销 World-scope 模式。
- Handle 含 Generation，旧世界句柄不能 Pop 新世界 Mode。
- Modal UI、Spectator、Death 等高优先级模式使用可恢复 Stack，而不是直接修改全局变量。
- Split-screen 每个 LocalPlayer 独立。

## 7. 性能

- 无必要不 Tick 整个模式栈；只由活动 Camera 更新。
- Collision Query 有预算和频率限制。
- Shake/Impulse 有并发与强度上限。
- Camera Telemetry 记录 Mode 切换、碰撞修正和异常目标丢失，不记录敏感身份。

## 8. 测试与 Review

自动测试：
- Push／Pop／Replace。
- 优先级和 Blend。
- 旧 Handle 拒绝。
- World 销毁清理。
- LocalPlayer 隔离。
- UI 输入焦点不污染 Camera 输入。
- Server Target 不包含本模块。

人工 Review：
- Exploration。
- Combat。
- Lock-on。
- Arena。
- Spectator。
- Death。
- Collision。
- 高速移动。
- 多分辨率／手柄。
- Motion sickness／Accessibility 选项。

## 9. 实施顺序

1. Definition／Handle／Stack 纯逻辑测试。
2. LocalPlayer 服务和 UE Camera Adapter。
3. Presentation Provider。
4. Input／UI 协调。
5. 项目 Profile。
6. Review Map。
7. Client Cook／性能／人工审核。
