# SSLM 寅虎 V6 八技能图标——24候选、人工优选、UE5.8实装实施方案 V2.0

> 日期：2026-10-18；工程：DivineBeastsWorkspace（主分支）；目标：八个技能，每个分别3张独立1024以上候选图，人工选定后精修8张正式图标，完成UE5.8真实HUD验收。本文件**取代旧版8图直接生产的实施顺序**，不变更原八技能的权威身份。
>
> 受控来源：`Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json`（历史V6效果）；`Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json`（稳定输入及现实授权状态）；`Docs/Design/ApprovedSkillDisplayNames_V5.json`（仍生效的正式显示名）。
>
> 当前状态：**仅完成八技能美术冻结、24候选构图指令、选择记录模板和技术方案；新候选图未出稿、未签审、未精修、未导入引擎。**前一轮出现含错误Unity流程与错误技能名的示意图，绝不可认作实际技能图标。HWebCodex目前没有可用的Monolith MCP服务调用路由，不凭代理进程存在宣称能够创建原生Texture2D。

## 一、目标与资源责任

1. 本轮V6的8技能在**美术设计范围**锁定：①白虎爪、②裂爪、③双爪袭、④啸岭扑杀、⑤扑杀、⑥虎踞、⑦虎啸、⑧开猎。被动裂甲另行处理，不占八个技能槽；V6冻结不等于批准服务器授予或更改正式V5显示名称。冻结身份和V2一级候选数值的逐项机器记录见 `TigerV6EightSkillsArtLock_20261018.json`。
2. **24张原稿**：每个技能恰好A/B/C 3种**独立构图**，每张至少原生1024×1024，不接受将同一画面裁切成三张或同图翻色作为3候选。具体24张全中文、美术差异明确的提示词、目标路径及当前存在性见 `TigerV6IconCandidates_24_20261018.json`。
3. **8张最终母版**：选定的8个候选各自重新精修为 ≥1024×1024 原画，保存母版。由同一份母版分别制作8张64×64审查PNG及8张48×48正式PNG，不能用先缩成48再放到64冒充高质版本。
4. UI源资源归属只在三层插件的第三层 `DBAHeroPack_Tiger`（虎英雄内容包），平台层 `GamePlatformUI`不硬引用Tiger。候选/终稿源艺术保存在SourceArt（艺术源文件）；由Monolith真正导入的Texture2D保存在内容包 `Content/UI/Abilities`（引擎技能图标）。严格保留此前V1原画和5个旧版Texture2D，不因V6直接覆盖。
5. 唯一正式 `UDivineBeastsAbilityUIProfile`（技能界面配置主资产）遵循已有 `DivineBeastsAbilityUIProfile:Hero.Zodiac.Tiger` 身份；如现有 `DA_DBA_Tiger_PlaceholderUIProfile`仍承载该身份，应在实际引擎/AssetManager核验后**原位更新**，不并存第二份重复主身份。技能图标只作为客户端数据软引用，不进入Dedicated Server Cook。

## 二、严格九阶段实施

### 1. 冻结8技能定义

以 `TigerV6EightSkillsArtLock_20261018.json` 为唯一图标设计输入，校验V6注册表的SkillId/AbilityId、Name V6、Name V5、Slot、Effect均一一对应。八码视觉顺序的真实输入身份为 `Primary, Slot1, Slot2, Slot4, Slot3, Slot5, Slot6, Slot7`：第4格终极与第5格移动不可错位。冻结后禁止因美术方便更改范围、目标资格、多段命中、伤害、气势等规则。任何更名须走单独签审，不通过编辑纹理文件暗改客户端显示名。

**完成证据**：8技能注册表静态一致性测试通过；美术锁定状态为 ART_DIRECTION_FROZEN_NOT_SERVER_APPROVED，不能写 SERVER_APPROVED。

### 2. 每技能3张候选（24张）

按 `TigerV6IconCandidates_24_20261018.json` 中S01-A~S08-C逐张生成，每项三张必须改变观看角度、主视觉主体或动势布局，并保持同一个动作机制和白虎辨识。出稿时避免拼图、图中文字、边框、廉价橙金同质化、过锐化与噪点。各图分别保存于：
`DBAHeroPack_Tiger/SourceArt/UI/SkillIcons/V6/Candidates/S01/` 等按技能编号细分目录。禁止拿错误流程信息图裁小当候选。

每张候选记录来源、1024真实尺寸、SHA-256、设计意图、生成/人工修订步骤；失败不冒充已出图。八项视觉差异：单爪／破甲钢片／交叉双爪／咆哮重扑／轻身跃扑／面向防御／扇形声浪／单猎物虎瞳。

**完成证据**：24份不同SHA的真实原稿，8组×3，均通过基本尺寸及视觉QA；缺图则此阶段为未完成。

### 3. 人工优选（强制阻断）

按技能顺序向用户呈现3张真实独立候选，每项可选A/B/C，亦可驳回三张要求重画。建议审核维度：主体是否符合技能60%，白虎与3A品质25%，64与48可读性15%；此权重只是评审辅助，不自动替代人工选择。必须在真实64、48试缩后再确认最终候选，64与48仅在阶段3用于候选可读性快速预览，并不代表最终资源已输出。

将结果录入 `TigerV6IconSelections_20261018.json` 的 `selectedCandidateId`（选中候选编号）、`reviewedBy`（审核人）、`reviewedAt`（审核时间）和结论。未明确选择的技能不进入步骤4，不允许程序自动取A为默认方案。人工可要求重新生成并追加修订历史，不能悄悄覆盖先前审过的原稿。

**完成证据**：8项独立人工签收，所有SXX-A/B/C原图可追溯；不存在虚构人工通过。

### 4. 精修8张正式图标

为8个已通过的候选分别完成重新构图、细节与色彩精修、前后景控制、视觉主次、抗锯齿。高清原稿输出 `SourceArt/UI/SkillIcons/V6/Masters/<正式纹理名>_Master_1024.png`，源稿实际尺寸≥1024。将终稿分别从**同一高清母版**制作64×64审查版和48×48游戏版；RGBA需要预乘Alpha缩放避免暗边/白边，彩色源采用正确sRGB编码。不采用无限锐化、2次Gamma补偿或噪点掩饰。正式PNG不包含任何技能框、键位、灰显/冷却层。

**完成证据**：8母版 + 8独立64PNG + 8独立48PNG，三类哈希、来源对应与人审记录齐备。

### 5. 导入虎英雄内容包

只有在第三步人工优选及第四步真正完成后，才使用UE5.8内的Monolith MCP `ui.import_texture_from_bytes`（从字节创建原生纹理）将**八张48×48正式PNG**导入 `DBAHeroPack_Tiger/Content/UI/Abilities/`，不要将测试用64PNG占用正式资产身份。高品质UI Texture2D设置：sRGB=true、UI二维图像压缩、UI LOD组、无Mip、非流送、Bilinear、Clamp，枚举依UE5.8实际引擎反射回读确认。

**完成证据**：8份真正的.uasset，各自能冷重载，实际源尺寸48、属性/路径/源图SHA逐项匹配，并有 MonolithGenerationManifest 的真实引擎操作证据。无法连接Monolith则停在此处，不伪造资产。

### 6. 建立 Ability UI Profile（复用唯一正式实例）

在UE5.8只读检查Tiger现有UI Profile真实主身份、反射字段及AssetManager扫描范围。**更新唯一实例而非新建重复实例**；为八项 `FDivineBeastsAbilityUIEntry`（技能界面表现条目）填写精确AbilityId、经过审批的DisplayName、非空中文Description、对应新Texture2D的软引用。视觉审核通过不代表V6技能可发放，若权威技能尚未批准则通过开发演示预览而非修改生产配置。

**完成证据**：引擎实际保存、`ValidateProfile`（配置校验）通过，按主身份只查出一个Profile，按AbilityId查到八个互不重复的Icon路径。

### 7. 接入技能栏 ViewModel

直接使用 `UDivineBeastsAbilityBarViewModel`（项目客户端只读技能栏视图模型）、`UDivineBeastsAbilityBarPanel`（技能栏面板）现有GAS OwnerOnly授权链和GamePlatformData异步租约。画刷读取新的Texture2D，技能状态继续从真实Grant/Spec/Avatar代次驱动。冷却、已禁用、可释放、键位、手柄/触屏状态通过UI层独立绘制，不能烘到PNG。无资源时只显示中性空状态；不能因8图存在就伪造可释放技能。

**完成证据**：客户端真实事件驱动，重复HUD打开、切虎/其他角色、旅行、死亡、重连、多LocalPlayer都不会加载错误英雄图标，租约正确释放。

### 8. 64×64 / 48×48 小尺寸验收

两套独立PNG在相同sRGB与白色Brush乘色下进行原生显示测试：
- 64×64：测试放大适配与图形轮廓，保证轻微放大不模糊、不显像素边，也不能因高分辨率版过多小细节而失焦。
- 48×48：正式HUD目标尺寸；正面草地/树林、深青技能槽、明亮技能背景、灰显、冷却蒙层、受损高对比环境下均能一眼辨认动作和图标区别。
- 逐项检查图边缘、PNG透明、同一技能64与48视觉一致、8张互不近似、正常亮度与终极强度分级。图标本身不做边框。若48不合格应回到母版精修，不能只用编辑器硬锐化。
- 当前正式技能条几何（总高≤65px、底边平齐、3px无遮挡施法条、独立暖金气势条）不能为64测试强行改变；64测试可在DemoHub独立审核视图中进行。

**完成证据**：8×2真实尺寸原生截图及审核结论；64素材仅为审查资产，正式48纹理仍唯一。

### 9. 新手村 / 战斗HUD验证

UE5.8客户端真实进入Village新手村并选择寅虎，检查8槽布局、48px大小、技能名称和热键、可用/禁用冷却以及地图光照下清晰度。真实战斗HUD继续检查基本攻击、两次爪击区分、终极/移动槽、虎啸/防御/开猎状态，在实际OwnerOnly服务端授权下逐项激活。多人MainArena验证图标资源不被服务器加载，Client Cook包含8纹理，Dedicated Server Cook排除纯UI图标。

必须留真实客户端截图、场景/镜头/曝光/渲染设置、UE构建和Cook日志、Profile和Widget冷重载结果、原生RHI色彩回读或可信截图；禁用已知错误Gamma的Monolith widget模式预览。不得把DemoHub只读预览称为完整可释放战斗技能。

**完成证据**：用户确认新手村与战斗HUD均符合美术与交互要求，再标记任务完成并提交该范围的资源与代码。

## 三、工程脚本与状态管理

- 冻结设计：`Docs/Implementation/GamePlatformAbilitySystem/TigerV6EightSkillsArtLock_20261018.json`（美术用V6八技能唯一身份）。
- 24候选：`Docs/Implementation/GamePlatformAbilitySystem/TigerV6IconCandidates_24_20261018.json`（8×3独立构图、中文提示词、来源）。
- 人工选择：`Docs/Implementation/GamePlatformAbilitySystem/TigerV6IconSelections_20261018.json`（无人工审核不允许填写默认A）。
- 原生纹理最终清单：现有 `Docs/Implementation/GamePlatformAbilitySystem/TigerV6SkillIconImportManifest_Draft.json`（8个48正式Texture2D对象路径，当前全部未导入）。
- 检查脚本：`Tools/Unreal/Abilities/ValidateTigerV6ArtPipeline.ps1`（冻结+24项静态一致性；逐步打开真实候选/签审/64与48文件校验）。
- 原有 `ValidateTigerV6SkillIcons.ps1` 是V1阶段48像素静态门禁，保留以避免并行任务兼容破坏。新脚本扩展候选审核与双尺寸，最终需双脚本通过才继续原生UE签审。

## 四、现状、阻断与提交边界

本轮已完成严格设计冻结及24种独立候选构图提案。当前候选真实出图受限，**图片生成不能输出合格可用的8×3独立图标**，此前图像生成得到的是Unity流程信息图而非《神兽联盟》正确独立图标，不能导入。不允许为追求“24张”而裁切这些图片、套色复刻V1或通过程序制造低质量图伪装3A级美术原稿。

Monolith代理虽可能可见，缺少此HWebCodex会话可调用的MCP Provider；要真正建立UI Profile、导入Texture2D、修改Widget、场景运行仍必须连接对应UE5.8服务，并确认运行中的真实工程。这些是明确未完成门禁。

工作区存在其他并行任务修改，提交时严格限定本轮Tiger V6文档与校验脚本，**不提交、重置或覆盖无关未提交资源**。冻结资产与候选提示词可先纳入主分支；24候选实际生成和人工优选之前，不触动唯一生产Tiger Profile、现有GAS或RootLayout。
