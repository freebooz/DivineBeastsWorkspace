# GamePlatformVFX（游戏平台视觉特效插件）

版本：0.1.0（代码纵向切片）  
目标引擎：UE5.8（虚幻引擎5.8）

## 1. 插件定位

`GamePlatformVFX` 位于 `GameFoundation/Presentation`，只负责跨项目可复用的客户端 VFX 机制：

`Request → Resolver → Catalog → Definition → 异步加载 → Niagara → Instance/Handle`

它不负责伤害、命中、治疗、护盾数值、传送、比赛结果，也不拥有《神兽联盟》生肖、技能、皮肤或世界美术。

## 2. 当前源码包已实现

- `UGamePlatformVFXDefinition` 与 10 种 Behavior Definition 类型。
- `UGamePlatformVFXCatalog`、注册/撤销、确定性 Resolver。
- `UGamePlatformVFXWorldSubsystem` 世界生命周期服务。
- 基于 `UAssetManager + FStreamableManager` 的异步 Definition / Niagara 加载。
- `UNiagaraFunctionLibrary::SpawnSystemAtLocation/SpawnSystemAttached` 执行。
- `FGamePlatformVFXHandle` + Generation 防旧世界句柄误用。
- Niagara 参数 Float / Int / Vector / Color 更新。
- Niagara 原生池化模式桥接。
- 基础数量级 Scalability Gate；真正渲染伸缩仍交给 Niagara Effect Type。
- Composite 子 Definition 延时编排的基础实现。
- Definition / Catalog / Composite 编辑器 Data Validation。
- 基础 Automation Test（自动化测试）入口。

## 3. 有意不臆造的集成

当前对话没有提供以下两个插件的真实 C++ 接口头文件：

- `GamePlatformData（游戏平台数据插件）`
- `GamePlatformPresentation（游戏平台表现插件）`

因此源码包采取：

- 资产加载暂用 UE5.8 原生 `UAssetManager/FStreamableManager`，并把调用集中在 `Preloading` 与 `WorldSubsystem`，方便未来替换成 `GamePlatformData` 统一租约。
- `GamePlatformVFXPresentationProvider` 只提供对接缝，不虚构不存在的 Provider Interface；拿到真实接口后应改为实现真实接口。

## 4. 安装

把目录复制到：

`Game/Plugins/GameFoundation/Presentation/GamePlatformVFX/`

然后：

1. 确认项目启用了 `Niagara` 与 `DataValidation` 插件。
2. 在 Asset Manager 配置中扫描 `GamePlatformVFXDefinition` 和 `GamePlatformVFXCatalog` 主资产类型。
3. 用 UE 编辑器创建真实 `.uasset` 资产；不要手工伪造二进制资源。
4. 编译 Editor / Client 目标。
5. 执行 Data Validation、客户端 Cook、服务器 Stage 审计。

## 5. 首条验证链

创建一个 `UGamePlatformVFXInstantDefinition` 资产，设置：

- `StableId = DBA.Test.Instant`
- `NiagaraSystem = 你的测试 Niagara System`

创建 `UGamePlatformVFXCatalog` 资产，将测试 `GameplayTag` 映射到该 Definition。

运行时：

1. `IGamePlatformVFXService::Get(WorldContext)`
2. `RegisterCatalog(Catalog)`
3. 构造 `FGamePlatformVFXRequest`
4. `Play(Request)`
5. 保存 `FGamePlatformVFXHandle`
6. 需要时 `Stop()` 或 `UpdateParameters()`

## 6. 当前验证状态

本包在当前环境中完成了：目录生成、JSON解析检查、代码结构静态检查。  
**没有 UE5.8 引擎源码、UBT 和《神兽联盟》真实工程，因此不能声明已经编译通过、Cook通过或生产验收通过。**

详细说明见 `Docs/Verification.md`。
