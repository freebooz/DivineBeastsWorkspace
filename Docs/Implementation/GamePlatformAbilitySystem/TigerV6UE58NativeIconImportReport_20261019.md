# SSLM 寅虎 V6 八技能 Texture2D 原生导入与冷重载验收报告

> 日期：2026-10-19。工程：DivineBeastsWorkspace。英雄内容包：`DBAHeroPack_Tiger`（寅虎表现资源包）。本报告仅证明真实原生图标导入、属性配置、独立UE冷重载，不宣称技能授权、正式UI Profile或战斗HUD已通过。
> 用户确定美术构图方向：1A、2B、3C、4A、5A、6B、7B、8B。八张高清母版、64×64与48×48 PNG已提前归档于该英雄包 SourceArt，仍待用户对真实新原画最终签收。

## 一、完成范围与唯一所有权

使用正确项目 `Game/DivineBeastsArena.uproject`、Unreal Engine 5.8.0（虚幻引擎）与 Monolith 0.23.0（虚幻编辑器自动化工具），先复核8份独立48像素PNG与SHA-256，再通过 Monolith MCP 标准协议的 `ui_query.import_texture_from_bytes` 逐项创建并保存八个真实引擎 UTexture2D（二位纹理）。`editor_query.run_python` 只做纹理设置、保存与即时回读；没有绕过Monolith用文本改名伪造 .uasset。

引擎长包路径唯一属于第三层英雄包：`/DBAHeroPack_Tiger/UI/Abilities/`；磁盘原生资产位于：
`Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Tiger/Content/UI/Abilities/`。
本次导入操作仅新建八份 V6 Texture2D，未主动写入既有五张V1纹理、唯一 `DA_DBA_Tiger_PlaceholderUIProfile`（现行技能界面配置）、`DBAGameplay` GAS能力授权或公共技能栏蓝图，也未创建重复主资产。需要注意，验证期间发现现有Tiger UI Profile在共享工作树中另有未提交二进制变更，来源尚未确认；该变更将保留原状并排除本次提交，交由相关所有者单独复核。

## 二、8项已保存的真实原生资源

| 位置 | 技能（仍区分V5正式显示名） | 真实输入槽位 | UE Texture2D短名 | 原生资产SHA-256前12位 |
|---|---|---|---|---|
| 1 | 白虎爪（现行V5：白虎爪） | Primary | `T_DBA_Tiger_S01_WhiteTigerClaw_V6` | `e8d0d98b0cf7…` |
| 2 | 裂爪（现行V5：裂甲爪） | Slot1 | `T_DBA_Tiger_S02_ArmorRend_V6` | `dec340010287…` |
| 3 | 双爪袭（现行V5：双爪袭） | Slot2 | `T_DBA_Tiger_S03_DualClawAssault_V6` | `9891d0d0b986…` |
| 4 | 啸岭扑杀（现行V5：虎啸扑） | Slot4 | `T_DBA_Tiger_S04_RidgeHuntUltimate_V6` | `a467b45826e5…` |
| 5 | 扑杀（现行V5：虎扑） | Slot3 | `T_DBA_Tiger_S05_PredatorLeap_V6` | `7b1505632817…` |
| 6 | 虎踞（现行V5：虎踞） | Slot5 | `T_DBA_Tiger_S06_TigerGuard_V6` | `8419dd19fe97…` |
| 7 | 虎啸（现行V5：虎啸） | Slot6 | `T_DBA_Tiger_S07_RoarWave_V6` | `2127881f13bd…` |
| 8 | 开猎（现行V5：开猎） | Slot7 | `T_DBA_Tiger_S08_HuntMark_V6` | `4d2586623193…` |

八项的完整稳定AbilityId（技能唯一身份）、对应原始PNG SHA、原生uasset SHA与软引用对象路径见：
`Docs/Implementation/GamePlatformAbilitySystem/TigerV6MonolithImportQueue_20261018.json`；
`Docs/Implementation/GamePlatformAbilitySystem/TigerV6SkillIconImportManifest_Draft.json`；
`DBAHeroPack_Tiger/Docs/MonolithGenerationManifest.json` 的 `TigerV6SelectedSkillIconImport`（寅虎V6原生资源登记段）。

## 三、原生设置与独立冷重载证据

- 源纹理：真实宽48像素、高48像素，八张不同的源SHA-256，未以64原图缩放显示替代。
- 压缩与分组：`TC_EDITOR_ICON`（User Interface 2D，UI二维无损压缩预设）、`TEXTUREGROUP_UI`（用户界面纹理组）。
- 色彩/采样：`sRGB=true`（彩色图片正确伽马编码）、`TMGS_NO_MIPMAPS`（不生成多级Mip）、`never_stream=true`（不流送）、`TF_BILINEAR`（双线性过滤）、`TA_CLAMP`（X/Y边缘钳制）。
- 真实保存：Monolith在编辑器中逐项创建.uasset并成功保存；独立哈希计算表明8份uasset均存在且不同。
- 冷重载：使用独立 `UnrealEditor-Cmd.exe`（UE命令行编辑器）进程，在 `-DisablePlugins=Monolith -NullRHI -ExecutePythonScript` 只读模式重新加载项目资产，而不借用原有UnrealEditor内存实例。测试脚本 `Tools/Unreal/UI/VerifyTigerV6ImportedTexturesUE58.py` 对八张Texture2D的原生尺寸、属性、AbilityId、槽位、源PNG与uasset哈希逐一校验，日志出现准确标记：
`SSLM_TIGER_V6_NATIVE_EIGHT_TEXTURES_COLD_VERIFY_PASS`。
- 原生日志：`Game/Saved/Logs/TigerV6NativeColdVerify_20261019.log`（本地忽略的运行日志，供审计；不是仓库中的伪造替身）。

## 四、未完成的审批和运行事项

- **美术人工签收未完成**：用户已选8个A/B/C设计方向，所选真实高清原画及48/64图仍需最终视觉确认，尤其注意不能使用旧版候选示意图充当游戏最终审核。
- **本次不修改正式技能界面配置**：没有将V6草案技能ID写进生产Profile；第4格终极为Slot4、第5格移动为Slot3。共享工作树中此Profile存在来源待核实的其他变更，不能据本轮结论宣称当前工作树的二进制Profile整体保持干净。
- **GAS权威授权未改**：图片存在不等于服务器已经授权八技能。不得用UI Profile伪造技能可释放状态，客户端ViewModel仍应从服务器OwnerOnly和原生GAS Spec消费只读状态。
- **正式HUD/新手村、多人竞技和Client/Server Cook仍需另验**：本轮仅针对原生资源导入。尤其客户端包含八张新UI资源且专用服务器Cook剥离纯图标，需要Cook证据；不宣称帧率或真实性能已验收。

## 五、下一阶段阻断条件

先执行八张真实图标人审。正式UI Profile更新必须同时满足经批准的技能显示名与GAS授权合同、Editor中唯一Profile主身份与Texture2D异步软引用。确认后复用当前 `UDivineBeastsAbilityBarViewModel`（技能栏视图模型）与 `UDivineBeastsAbilityBarPanel`（技能栏面板）事件驱动机制，最后开展48×48真实HUD显示、64×64辅助画板、Village（新手村）和MainArena（竞技场）场景、编译、冷重载与Cook测试。不得为本轮纹理替换重新绘制或突破原已锁定65像素技能栏几何、3像素施法条与独立暖金气势条。
