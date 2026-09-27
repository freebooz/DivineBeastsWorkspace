# GamePlatformSFX（游戏平台音效插件）

当前状态：**一期关键客户端表现能力未完成**。现有 `GamePlatformSFXClient（ClientOnly）` 仅为模块入口，没有正式 SFX Definition、Catalog、Provider、生命周期或 Review。

目标链路为：

```text
Presentation Request → SFX Provider → Catalog/Resolver → Definition → GamePlatformData Lease → UE Audio/MetaSounds
```

平台优先复用 MetaSounds、SoundConcurrency、Attenuation、SoundClass／SoundMix 等 UE 原生能力，不复制 VFX 分类，也不重造 Audio Mixer。纯音频资源不得进入 Dedicated Server。

详细实施方案见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。
