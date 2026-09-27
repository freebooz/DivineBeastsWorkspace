# GamePlatformSFX 实现规格

> 状态：P0-7 设计规格，尚未实施。  
> 目标：建立与 VFX 同级但不复制 VFX 分类的通用音频表现框架，优先使用 UE 原生 Audio／MetaSounds 能力。

## 1. 现状

当前仅 `GamePlatformSFXClient（ClientOnly）` 模块注册壳，没有 Definition、Catalog、Provider、音频生命周期、测试或 Review。

## 2. 核心原则

标准链：

```text
Presentation Semantic Request
→ SFX Provider
→ Catalog / Resolver
→ SFX Definition
→ GamePlatformData Lease
→ UE Audio / MetaSounds
→ Instance Handle
```

禁止：
- Gameplay 直接 PlaySound。
- 复制 VFX 的 10 Behavior／17 Category 作为音频类型。
- 自己重写 Audio Mixer。
- SFX 失败改变权威结果。
- Server Cook 带入纯客户端音频资源。

## 3. 推荐类型

```text
UGamePlatformSFXDefinition
    : UGamePlatformDefinitionBase
```

建议数据：
- DefinitionId／Semantic。
- Sound／MetaSound 软引用。
- Attenuation Profile。
- Concurrency Profile。
- Priority。
- Spatialization Policy。
- Occlusion Policy。
- Bus／Mix Category。
- Loop／Lifetime。
- FadeIn／FadeOut。
- Platform／Quality Variant。
- Fallback。
- PreloadAssets。
- Parameter Schema。

`UGamePlatformSFXCatalog`：
- 中立语义 → Definition 主资产 ID。
- 确定性冲突检查。
- 项目／内容包 Scope 由平台中立 Context 决定。

运行时：
- `IGamePlatformSFXService`：低层播放／停止／预加载接口，仅 Client。
- `FGamePlatformSFXHandle`：作用域、Generation、实例身份。
- `UGamePlatformSFXWorldSubsystem` 或等价 World-scope 服务：只有真实 World 生命周期职责时建立。
- UI／非空间音频按 LocalPlayer／GameInstance 作用域处理，不强迫所有声音走 World。

## 4. UE 原生能力优先

优先复用：
- MetaSounds。
- SoundClass／SoundMix。
- SoundConcurrency。
- Attenuation。
- AudioGameplayVolume（适用时）。
- 引擎音频组件生命周期。

平台只做定义、语义解析、作用域、句柄、预加载、降级和诊断，不重造混音器。

## 5. 3A 需要的公共音频语义

至少覆盖机制层：
- OneShot。
- Loop。
- Attached／World Spatial。
- UI。
- Ambience。
- Music Transition（仅契约；音乐导演若形成独立职责再评估）。
- Voice／Dialogue 不在本轮强行并入；其本地化、字幕和流式需求需专项设计。

这些是机制分类，不是项目技能名。

## 6. 项目扩展

DivineBeasts 只提供项目 Catalog、Definition 实例、MetaSounds、SoundWave、Mix/Profile。

如果项目确有稳定公共字段，优先用 `FDivineBeastsSFXMetadata` 组合，而非建立每个技能 SFX C++ 子类。

## 7. 性能和生命周期

- Concurrency／Priority 使用引擎原生机制。
- 循环实例必须可被 World/Owner 销毁可靠停止。
- 预加载使用 GamePlatformData Lease。
- 不持有裸 UObject 跨 World。
- 统计 Voice Count、Active Components、Virtualized Count、Load Failure。
- 具体 CPU／内存／Voice 数预算由目标平台实测批准，不伪造数值。

## 8. 测试与 Review

自动测试：
- Catalog 确定性。
- Scope／Handle Generation。
- Loop Stop 幂等。
- Owner/World 销毁清理。
- Fallback。
- 缺失资源失败。
- Server 产物不引用客户端音频模块。

人工 Review：
- Spatial attenuation。
- Occlusion。
- Concurrency。
- UI／World 隔离。
- Music／Ambience 切换（实现后）。
- 5v5 并发音频可读性。
- 不同质量档和设备。

## 9. 实施顺序

1. Definition／Catalog／Resolver 纯逻辑。
2. GamePlatformData Lease。
3. UE Audio Provider。
4. Presentation Bridge。
5. Pool/lifecycle/diagnostics。
6. 项目内容包。
7. Review Map、Client Cook、人工审核、性能实测。
