# DBAHeroPack_Goat（未羊英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Goat/`。仅持有未羊的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：五张已导入技能图标纹理；文件名中的美术名称不代表正式技能身份已经批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

本轮文件系统观察为8份.uasset：外观配置1、原型材质1、图标5、头像1。现有形象仍属于开发/原型表现，不等于正式生肖模型。此数量不是UE重载、Cook、联机或人工艺术验收结果。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。

## 未羊V6八技能美术链路（2026-10-21）

美术定义和资源身份冻结在 `Docs/GoatV6_ArtFreeze_20261020.json`（未羊八技能美术冻结合同），仅用于八张新技能图标的审核制作；早期 V6.0 机制和修改名称仍在人工审核，不覆盖 V5 正式技能身份和现有 GAS（游戏玩法技能系统）授权。

现有真实工作区尚未获得由 ChatGPT 生成的 `SSLM_GoatV6_EightSkillIconAssets_20261019.zip`（未羊八技能纹理交付包），故新建 V6 纹理、Profile 增补、原生审核控件、Village（新手村）及 MainArena（战斗竞技场） HUD 验收均不得标记已完成。其他工作任务已经修改本英雄的唯一 Profile，接入前必须先使用 Monolith（虚幻编辑器操作服务）核实原生条目和备份，禁止直接覆盖。

实际执行入口按以下顺序：

1. 将完整 ZIP 放入工作空间 `artifacts/imports/`（素材传输暂存目录）。使用 `pwsh Tools/Unreal/UI/StageGoatV6IconsFromArchive.ps1 -Mode inspect`（只读源文件检查），然后 `-Mode stage`（SHA 校验后接收 24 张 PNG 与生成接线计划）。默认缺包只报告等待，不以旧图冒充。
2. 在连接到锁定 `Game/DivineBeastsArena.uproject` 的 UE5.8 Monolith 编辑器中，调用 `Tools/Unreal/UI/ImportGoatV6StagedTexturesMonolith.py`（未羊 Texture2D 导入）并明确设置 `GOAT_V6_MODE=import_preview`；完成后以 `verify` 独立核对 8 张 48 像素原生纹理、UI 压缩、sRGB 和无 Mip。
3. 复用通用 `CompleteImportedHeroIconProfilesMonolith.py`（唯一技能界面资料接线），传入 `PROFILE_PLAN=Saved/Validation/GoatV6IconImport20261020/IntegrationPlan.json`（本轮接线计划），按 `inspect → apply → reload → verify` 四阶段操作；遵守备份及旧条目不变约束。不可凭纹理存在自动授予技能。
4. 由 `BindGoatV6ArtPreviewOrderMonolith.py`（羊八槽只读展示顺序工具）执行 `GOAT_PREVIEW_MODE=inspect → apply → verify`，其仅向原有 Profile 绑定八个稳定 AbilityId（技能编号）；ViewModel（技能栏视图模型）仍走已有的异步 GamePlatformData（资源数据服务）及客户端只读展示通路。
5. 使用 `AuditImportedHeroSkillIconSmallSizes.py`（图标 64／48 小尺寸审核工具）读取实际 `IntegrationPlan.json`，检查大小、哈希、重复、近似，输出技术报告和原尺寸图；再由 Monolith 创建 `/DBAHeroPack_Goat/UI/Review/WBP_DBA_Goat_SkillIconReviewV6`（只读审核控件）。玉角刺／净光、铃步及大招／控制图标必须人工确认，不得将自动技术检查当作美术签收。
6. 调用 `PreviewImportedHeroIconsInVillageMonolith.py`（新手村只读真实 HUD 预览），指定 `PREVIEW_HERO=Goat`、本轮独立证据目录，依次执行 `open → show → capture → cleanup`；必须有真实 UE 图片、16 个图标控件实际 64／48 逻辑像素及技能栏八项只读状态记录。
7. 正式 MainArena（竞技对局）只能在对应 GAS 技能获得人工玩法审批、服务器权威授权、客户端／服务器一致的 Cook（资源烘焙）和联机准入后单独验收。只读美术预览不能当作可施法、联机或正式战斗验收。

所有直接改变 UI 的步骤须通过 Monolith 操作 UE5.8 原生资产；脚本文件、设计图、PNG、静态单测都不能代替编辑器编译、保存、冷重载、原生截图和人工签收。
