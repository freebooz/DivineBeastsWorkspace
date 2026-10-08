# 十二生肖角色原型与正式替换方案

## 1. 目标

神兽联盟十二生肖角色采用“稳定逻辑身份”和“可替换表现资源”彻底分离的设计。

角色逻辑身份由 `HeroDefinitionId（英雄定义编号）` 唯一确定，例如：

- `Hero.Zodiac.Rat`：子鼠
- `Hero.Zodiac.Ox`：丑牛
- `Hero.Zodiac.Tiger`：寅虎
- `Hero.Zodiac.Rabbit`：卯兔
- `Hero.Zodiac.Dragon`：辰龙
- `Hero.Zodiac.Snake`：巳蛇
- `Hero.Zodiac.Horse`：午马
- `Hero.Zodiac.Goat`：未羊
- `Hero.Zodiac.Monkey`：申猴
- `Hero.Zodiac.Rooster`：酉鸡
- `Hero.Zodiac.Dog`：戌狗
- `Hero.Zodiac.Boar`：亥猪

这些编号来自 `Shared/Contracts（共享契约）`，不得因为临时模型或正式模型替换而改变。

## 2. 三层职责

### GamePlatformCharacter（平台角色插件）

只保存跨游戏通用能力：

- Hero Definition 基类；
- SpawnEnvelope（出生碰撞包络）；
- Movement（基础移动定义）；
- Character Initializer（角色初始化器）契约；
- Character State View（角色状态只读接口）；
- Character Creation Provider（角色创建提供者）契约；
- Definition 异步加载。

禁止依赖神兽联盟生肖、Manny/Quinn、UI、VFX、具体技能或具体材质。

### DBAGameplay / DivineBeastsCharactersRuntime（神兽联盟玩法层）

只保存项目权威角色逻辑：

- 十二生肖身份；
- HeroDefinitionId 与生肖映射；
- `UDivineBeastsHeroDefinition（神兽联盟英雄定义）`；
- CharacterId、SpawnGeneration、AvatarGeneration；
- Definition Version / ContentRevision 一致性门禁；
- 出生初始化适配器；
- 角色创建 Schema。

这里不保存 SkeletalMesh、Material、Animation、VFX、SFX 等表现硬引用。

### DBAClient / DivineBeastsPresentationClient（神兽联盟客户端表现层）

负责：

- `UDivineBeastsCharacterAppearanceProfile（角色外观配置）`；
- HeroDefinitionId → Appearance Profile 映射；
- SkeletalMesh / Material / AnimInstance 异步加载；
- Manny/Quinn 开发占位；
- 生肖颜色区分；
- 将来真实生肖美术替换。

Dedicated Server 不依赖本层表现资源。

## 3. 当前开发占位方案

首次缺少公共占位资源时，生成器会从 `E:\work\Game\DivineBeastsArena` 下自动查找包含 Manny/Quinn 的旧工程，当前可用源为 `DBA_GameClient`。公共资源一旦已经进入版本库，后续生成与验证不再依赖旧工程。

`Generate-ZodiacPrototypeCharacters.ps1（生肖原型角色生成脚本）` 只复制 Manny/Quinn 显示所需的最小依赖到临时 `/Game` 路径，再由 Unreal Editor 逐资产迁移到：

`Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content/`

最小依赖只包含 Manny/Quinn SkeletalMesh、Skeleton、PhysicsAsset、基础材质和纹理；明确不导入旧工程的 Control Rig、Mover 示例和动画蓝图。二进制资源跨挂载点引用由引擎重写，禁止直接在文件系统中给 `.uasset` 改目录。公共内容包只保存一套共享 Manny/Quinn，不复制 12 套纹理，也不为每个生肖复制一套基础网格。

十二生肖采用两套基础轮廓 + 12 个高区分度颜色：

| 生肖 | 原型网格 | 开发识别色 |
| --- | --- | --- |
| 子鼠 Rat | Manny | 蓝灰（#607D8B） |
| 丑牛 Ox | Quinn | 棕褐（#795548） |
| 寅虎 Tiger | Manny | 橙色（#FF8C00） |
| 卯兔 Rabbit | Quinn | 粉红（#F48FB1） |
| 辰龙 Dragon | Manny | 青蓝（#00ACC1） |
| 巳蛇 Snake | Quinn | 翠绿（#2E7D32） |
| 午马 Horse | Manny | 赤红（#D32F2F） |
| 未羊 Goat | Quinn | 金色（#D4AF37） |
| 申猴 Monkey | Manny | 紫色（#7E57C2） |
| 酉鸡 Rooster | Quinn | 金黄（#FDD835） |
| 戌狗 Dog | Manny | 深蓝（#3949AB） |
| 亥猪 Boar | Quinn | 紫红（#AD1457） |

颜色仅用于开发识别，不代表五行、阵营、稀有度或任何玩法语义。

正式原型资产由生成器在各 `DBAHeroPack_*` 中创建独立材质实例，统一使用 `PrototypeTint（原型色调）`；当正式 Profile 尚未生成而进入非 Shipping 回退路径时，则对 UE5 Mannequin 原材质使用 `Paint Tint（涂装色调）`。两种路径都由 `UMaterialInstanceDynamic（动态材质实例）` 安全装配，因此：

- 不复制纹理；
- 不增加 12 套材质资源；
- 身份切换时可安全重新装配；
- 正式角色 Profile 关闭 `bDevelopmentPlaceholder` 后自动绕过占位染色。

## 4. 稳定 Appearance Profile 路径

正式资源按生肖保持稳定逻辑路径：

```text
/DBAHeroPack_Rat/Characters/DA_Appearance_Zodiac_Rat
/DBAHeroPack_Ox/Characters/DA_Appearance_Zodiac_Ox
...
/DBAHeroPack_Boar/Characters/DA_Appearance_Zodiac_Boar
```

开发阶段这些正式 Profile 尚不存在时，非 Shipping 构建自动创建瞬态 Manny/Quinn 占位 Profile。

Shipping 构建禁止该回退：正式发布必须交付真实 Hero Definition 和真实 Appearance Profile，从而避免把开发占位角色带进正式包。

## 4.1 原型资产生成命令

在工作空间根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\\Tools\\Unreal\\Characters\\Generate-ZodiacPrototypeCharacters.ps1
```

脚本默认自动探测当前环境：

- `WorkspaceRoot（工作空间根目录）`：根据脚本所在的 `DivineBeastsWorkspace/Tools/Unreal/Characters` 自动向上解析，不写死盘符；
- `EngineRoot（引擎根目录）`：优先探测 `F:\UnrealEngine-5.8.0-release`，其次探测 `D:\UnrealEngine-5.8.0-release`，也可显式传入；
- `SourceProjectRoot（旧资源源项目）`：仅在公共 Manny/Quinn 尚未落盘时需要，可从 `E:\work\Game\DivineBeastsArena` 自动搜索，也可显式传入。

生成器负责创建/更新：

- `DBAContentPack_Common` 中唯一一套共享 Manny/Quinn 最小依赖；
- 12 个 `DBAHeroPack_*` 中的生肖材质实例与 Appearance Profile；
- `DBAGameplay/Definitions` 中的 12 个 Hero Definition；
- 成功后清理 `/Game` 临时 Mannequin 迁移目录；
- 资产存在性和 12/12/12 数量校验。

只做快速验收、不启动 Unreal Editor 时使用：

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Unreal\Characters\Generate-ZodiacPrototypeCharacters.ps1 -ValidateOnly
```

当前验收基线要求：公共 Manny、Quinn、Skeleton、PhysicsAsset 均存在，且 Hero Definition / Appearance Profile / 原型颜色材质分别为 `12 / 12 / 12`。

2026-10-01补充：资产存在不代表引用完整。公共两网格必须关联`SK_Mannequin_Skeleton`（当前68根骨骼，含root/pelvis/head）和既定物理资产，母材质必须启用SkeletalMesh用途，纹理采样不得为空。十二Profile必须为Pitch=0、Yaw=-90、Roll=0；Python生成器使用具名`unreal.Rotator`参数，避免把位置参数次序误当C++次序。`RepairCharacterPreviewAssets.py`只修复既定表现资产，骨架只读字段先由Monolith属性动作恢复；`Tests/Assets/ValidateCharacterPreviewAssets.py`通过锁定编辑器执行只读回归，不连接后端或修改权威定义。预览舞台的脚底高度只调整本地组件，不改变Profile中用于角色胶囊的偏移。原型仍没有正式动画蓝图，模型/材质/骨架修复不代表正式美术已交付。

## 5. 后期真实角色替换

每个生肖真实模型完成后，只修改对应 `DBAHeroPack_<Zodiac>` 的 Appearance Profile：

1. 导入真实 SkeletalMesh；
2. 导入正式材质与纹理；
3. 配置 SkeletonCompatibilityId；
4. 配置 AnimInstanceClass 或项目动画 Profile；
5. 调整 MeshRelativeLocation / Rotation / Scale；
6. 将 `bDevelopmentPlaceholder=false`；
7. 更新 Profile ContentRevision；
8. 通过客户端 Cook 与角色表现测试。

以下内容禁止因美术替换而修改：

- HeroDefinitionId；
- CharacterId；
- Shared/Contracts；
- 后端角色档案；
- GAS 技能身份；
- 网络复制字段；
- SpawnGeneration / AvatarGeneration；
- 角色创建协议；
- 存档键。

因此 Manny/Quinn → 正式十二生肖是纯表现资源替换，不需要数据迁移。

## 6. 当前安全边界

平台角色初始化唯一入口为 `FGamePlatformCharacterInitializationExecutor（平台角色初始化执行器）`。项目层通过 `GamePlatform.CharacterInitializer` 注册 `FDivineBeastsCharacterSpawnInitializer`；Executor 要求当前组合根恰好存在一个初始化器，且自身不执行 `SpawnActor/Possess`。

MainArena 已接入 `FDivineBeastsArenaGameplayLifecycleAdapter（神兽联盟竞技玩法生命周期适配器）`：Assignment 阶段预热 12 个 Server-safe Hero Definition；倒计时结束后通过 UE 标准 `RestartPlayerAtPlayerStart` 创建/控制基础 `ACharacter`，再调用平台 Executor 完成项目角色初始化并确认 `CharacterReady`。所有参赛者初始化成功后，比赛才允许进入 `InProgress`。复活同样复用该链路，并以 SpawnGeneration 防止旧代次覆盖。

项目层没有直接调用 `SpawnActor/Possess`；Manny/Quinn 仍只属于客户端 Appearance，不进入 Dedicated Server。OpenWorld/Village 后续应复用同一 Executor 接入各自平台出生编排。

独立基础设施限制：`GamePlatformGameplay` 中的通用 `AGamePlatformGameModeBase / GameState / PlayerController / PlayerState` 目前仍存在历史声明无实现，单模块链接会报告未解析符号。MainArena 当前使用已实现的 `GamePlatformArena` 生命周期适配边界，不依赖该空实现；但 OpenWorld/Village 若要直接采用这套通用 GameMode，必须另行完成该平台底座。

## 7. 验收要求

开发原型至少满足：

- 12 个生肖稳定 HeroDefinitionId 全部可枚举；
- 非 Shipping 环境缺少真实 Definition 时可使用开发逻辑 Definition；
- 非 Shipping 环境缺少真实 Appearance Profile 时可使用 Manny/Quinn；
- 12 个生肖颜色可明显区分；
- 无角色 Tick；
- CharacterId 仅 OwnerOnly 复制；
- Hero 运行状态原子复制；
- Definition Version / ContentRevision 客户端与服务器一致后才 Ready；
- 异步旧请求不能覆盖新角色；
- 角色创建草稿在 Definition 异步加载完成后再校验；
- Dedicated Server 不需要客户端 Mesh/Material/Animation 执行代码；
- Shipping 禁止开发占位回退。
- MainArena 全员必须在 `InProgress` 前完成标准 Pawn 出生、唯一 Character Initializer 初始化和 `CharacterReady`；
- MainArena Server 定向模块编译必须通过，且项目生命周期适配器源码不得出现直接 `SpawnActor/Possess` 调用。
