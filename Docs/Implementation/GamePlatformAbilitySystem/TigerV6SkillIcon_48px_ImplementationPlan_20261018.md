# SSLM 寅虎 V6 八技能 48×48 图标制作、UE5.8导入与技能栏接入计划

> 2026-10-18。基于真实 DivineBeastsWorkspace 工程源码与历史技能 V6 审查稿；性质为生产方案/资源导入清单，不代表原画、48图、UE资产或HUD验收完成。
> 唯一美术规范：`Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md`，配合`Docs/Implementation/UITextureVisualPolicy.md`、`AGENTS.md`；禁止引入旧五行伤害/破元/共鸣/王印规则。

## 1. 冻结权威身份与工程现状

英雄为`Hero.Zodiac.Tiger`（寅虎·啸岭白君），定位爆发猎杀/目标破甲，独立被动为「裂甲」，不占8槽。V6为审查候选稿，尚非全量签审的正式服务器技能表。三项与V5已批准显示名不同：V6「裂爪」对应生产名「裂甲爪」；V6「啸岭扑杀」对应「虎啸扑」；V6「扑杀」对应「虎扑」。稳定AbilityId不得随显示名重命名。

当前实施注册表 `V6SkillImplementationRegistry_20261018.json` 已明示96项V6技能尚未正式授予、原生V6定义资源未生成，不能把新图标加进服务器未授权技能栏来宣称8技能运行完成。

原有英雄包 `Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Tiger` 已有5张V1原画（1254×1254）与对应5个引擎Texture2D及旧UI Profile。全部保留、不覆盖、无主资产身份冲突。当前运行UI是 `UDivineBeastsAbilityBarPanel`、`UDivineBeastsAbilityBarViewModel`、`UDivineBeastsAbilityUIProfile`，直接复用，不新造平台或项目独立播放器/资源管理器。

## 2. 八技能美术与资产对照（严格按屏幕显示顺序）

| 位置 | V6技能 | 当前V5名 | 输入槽 | 稳定技能ID | 图标纹理名 |
|---|---|---|---|---|---|
| 1 | 白虎爪 | 白虎爪 | `Primary` | `dba.ability.tiger_eight_claw_strike@1` | `T_DBA_Tiger_S01_WhiteTigerClaw_V6` |
| 2 | 裂爪 | 裂甲爪 | `Slot1` | `dba.ability.tiger_eight_golden_rake@1` | `T_DBA_Tiger_S02_ArmorRend_V6` |
| 3 | 双爪袭 | 双爪袭 | `Slot2` | `dba.ability.tiger_eight_tiger_pounce@1` | `T_DBA_Tiger_S03_DualClawAssault_V6` |
| 4 | 啸岭扑杀 | 虎啸扑 | `Slot4` | `dba.ability.tiger_eight_tiger_roar_storm@1` | `T_DBA_Tiger_S04_RidgeHuntUltimate_V6` |
| 5 | 扑杀 | 虎扑 | `Slot3` | `dba.ability.tiger_eight_leaping_claw@1` | `T_DBA_Tiger_S05_PredatorLeap_V6` |
| 6 | 虎踞 | 虎踞 | `Slot5` | `dba.ability.tiger_eight_regal_guard@1` | `T_DBA_Tiger_S06_TigerGuard_V6` |
| 7 | 虎啸 | 虎啸 | `Slot6` | `dba.ability.tiger_eight_roar_shock@1` | `T_DBA_Tiger_S07_RoarWave_V6` |
| 8 | 开猎 | 开猎 | `Slot7` | `dba.ability.tiger_eight_battle_fervor@1` | `T_DBA_Tiger_S08_HuntMark_V6` |

第4个屏幕位置必须绑定终极`Slot4`，第5个才是移动`Slot3`。图标主体均不同：白虎爪=单爪；裂爪=破甲；双爪袭=X双段；啸岭扑杀=吼后凌空重爪；扑杀=侧身位移；虎踞=正面护屏；虎啸=扇形声浪；开猎=虎瞳唯一猎物标记。全套个性化中文图像生成提示词见同目录`TigerV6SkillIconImportManifest_Draft.json`之每个`generationPromptZh`字段。

## 3. 图像质量与资源位置

- **一技能一张独立高清原稿**，建议1024×1024或2048×2048，绝不一次拼八宫格切作正式图标。图像生成只是原始草图，必须做轮廓、材质、干净边缘、色阶、构图人工二次精修。
- 强调白金/暖金白虎血统、玄墨青深底，参考游戏的鲜明辨识与电影感，但不复制他人现成图标。最终完整48×48 PNG，不允许1024图仅靠UMG缩到48冒充。无快捷键、技能边框、冷却蒙层、字体、Logo、颗粒、过锐化、噪点。
- 缩小前校验源sRGB；使用有色彩管理的高品质Lanczos滤波，包含透明区域时先预乘Alpha，避免白边。48像素实尺寸评审为强制条件，同时在24像素辅助检测主体是否可辨认，最终清边由人工审核，不通过不得发版。
- 文件位置：高清图`DBAHeroPack_Tiger/SourceArt/UI/SkillIcons/V6/Masters/`；真48 PNG `DBAHeroPack_Tiger/SourceArt/UI/SkillIcons/V6/48/`；原生引擎Texture2D `DBAHeroPack_Tiger/Content/UI/Abilities/`。新图标软对象路径为`/DBAHeroPack_Tiger/UI/Abilities/{AssetName}.{AssetName}`。保存原图与成品SHA-256、尺寸、颜色空间及审核签字。

## 4. 可执行门禁与实施顺序

### G0：识别编辑器/Monolith、冻结并行文件

确认真实 UE5.8 编辑器打开`Game/DivineBeastsArena.uproject`及其插件挂载点，记录5张旧图和旧UI Profile资产指纹。WebCodex在本轮读取到UE5.8 Editor和Monolith代理进程，但HWebCodex暴露的MCP Server与Runner插件列表为空；代理进程存在不等于可调用Monolith服务。未取得受支持的Monolith MCP路由时不得越权直接造UI纹理.uasset。主分支有其他并行未提交修改，未经协调不覆盖。

### G1：样板验收（先裂爪、啸岭扑杀）

各独立制作一张大于等于1024的高清原稿。裂爪检查虎爪真的劈开护甲；啸岭扑杀检查虎啸+跃扑重爪与大招层级。经真人看48显示图、底色、亮暗场景和3A材质合格后再做另六张。美术失败退回源画调整，绝不批量制作96生肖资产放大返工。

### G2：48像素原图生产与自动验收

每张产出高清Master、独立48PNG、放大4倍的最近邻边缘检查图（仅供审核）。验证每个文件是真PNG、正好48×48、图像非空、8张PNG哈希互不相同、清晰轮廓、不出现白/黑Alpha边；源画未被覆盖。建立正常、冷却、灰显及暗亮背景对照板，在真实48显示确认色彩不是靠后期锐化修补。

### G3：经Monolith MCP导入真实Texture2D

通过`ui.import_texture_from_bytes`创建八个真实原生纹理；通过`editor.run_python`（编辑器原生属性设置和回读）设置 UI LOD组、UserInterface2D UI压缩、sRGB=true、无Mip、NeverStream=true、Bilinear与Clamp。枚举值须使用UE5.8实际反射类型核对，不依靠JSON虚构字符串。保存后独立冷重载确认源X/Y=48，记录8个完整路径、属性、原稿/PNG/.uasset哈希。禁止文本占位.uasset。原生AssetRegistry见到文件不等于已验证可显示。

### G4：先建立审核演示，不冒充已授权技能

将8张Texture2D挂接已有DemoHub开发审核用例，以真正48屏幕尺寸查看八图、独立单图、无/有冷却、暗亮背景、聚焦和灰显。记录逐图通过/驳回以及美术负责人结论。在正式GAS授权前仅允许只读预览，不能新建冲突的`DivineBeastsAbilityUIProfile:Hero.Zodiac.Tiger`主资产。

### G5：八项正式GAS验收后再绑定唯一Tiger UI Profile

正式技能实现由DBAGameplay/GAS与服务器共同验证GA/GE、Definition、AbilitySet、OwnerOnly授权和对应输入Tag。此后以Monolith修改唯一Tiger UI Profile，每条`FDivineBeastsAbilityUIEntry`精确写入稳定AbilityId、**已批准**DisplayName与非空中文Description、对应Icon的`TSoftObjectPtr<UTexture2D>`。`ValidateProfile`必须无重复、无空ID、无空描述，客户端数据服务异步加载并持有图标租约。

真实链路：已授予OwnerOnly技能及GAS Spec→`UDivineBeastsAbilityLoadoutComponent`→`UDivineBeastsAbilityBarViewModel`→GamePlatformData世界租约加载Profile与8纹理→`FindEntry(AbilityId)`→`FGamePlatformUISlotState.Icon`→技能面板UMG显示。只显示授权技能，绝不在Widget中自己GiveAbility；失败保持空图；异步回调比对Hero、Avatar/LoadGeneration、对象生命周期，切角色、销毁、Travel撤销各自租约。公共GamePlatformUI不反向依赖Tiger包。

正式技能栏保留经审核的底部几何与纯装配：总高不超65px、图标48px、底部齐平、3px施法条独立无遮挡、血条下能量蓝条与独立暖金气势条；快捷键、冷却遮罩和灰显通过事件驱动叠层显示，不能烘在图标中。图标不能触发改变技能栏形状、重做技能框或重复加载监听器。

### G6：真实运行、回归、Cook与证据

经UE5.8验证8纹理真实可打开、属性正确、Widget编译保存冷重载、用户人工验收；Village新手村和MainArena多角色HUD真实显示，明亮绿色草地/深色竞技背景都能辨认。用两客户端+权威服务器验证八槽身份、启用/灰显、冷却、换英雄、重连和角色销毁；不让Dedicated Server加载纯客户端UI纹理，实测客户端/服务器Cook。实际截图使用客户端截图或RHI回读；禁用已知重复Gamma的`capture_scene_preview(widget)`。静态脚本验证、文档和文件存在不能替代引擎验收。

### G7：签审与提交

更新Tiger英雄包现有`Docs/MonolithGenerationManifest.json`，存每图PNG/.uasset哈希、真实导入/属性/冷重载、技能ID/槽位、审核结论；最后只提交已核对的Tiger改动到主分支，不捎带其他并行任务的工作树内容。任何美术生成失败、Monolith不可用或V6 GAS未批准，则相应阶段仍标为未完成。

## 5. 本轮实际状态（防止把计划冒充交付）

**已完成**：项目V6八技能/槽位/ID与V5名字分歧核对、客户端绑定链路审查、Tiger V1原图/UE5纹理存在性确认、机器可读清单与生产流程方案。

**尚未完成**：V6八张全新高清原画、八张48PNG及真实Texture2D、人工图标验收、更新唯一正式Tiger Profile、八项V6服务器授予与真实UE运行验收。本次尝试的外部出图服务未成功返回图像（一个提示账号套餐不足，另一个出现内部错误）；绝不以现有V1原画或低质量占位替换未批准V6。Monolith代理在进程中可见但HWebCodex当前没有MCP提供者连接，原生UI资源导入仍受阻。
