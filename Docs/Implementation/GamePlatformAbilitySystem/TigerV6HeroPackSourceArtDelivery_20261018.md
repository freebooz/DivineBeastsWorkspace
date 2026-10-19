# SSLM 寅虎 V6 八技能所选纹理：英雄包源资源交付记录

> 2026-10-18。范围仅为已选A/B/C方向对应的**8张独立新原画及64/48像素图片**，并非24张A/B/C原画全部交付，更不是UE5.8原生Texture2D或游戏HUD正式验收。
> 工作空间：`DivineBeastsWorkspace`。插件：`DBAHeroPack_Tiger`（项目第三层寅虎表现内容包）。

## 已实际保存的原图与缩图

用户确定构图：**1A、2B、3C、4A、5A、6B、7B、8B**。每项已有1024×1024独立PNG候选源图、对应1024母版，以及由母版独立重采样的64×64和48×48 PNG。源原图保存至 `SourceArt/UI/SkillIcons/V6/Candidates/Sxx/`，母版保存至 `Masters/`，两种UI缩图保存至 `64/` 和 `48/`，审核画板保存至 `Review/TigerV6_8Icons_64_48_VisualReview.png`（均相对 `DBAHeroPack_Tiger`）。
已使用可复现的 `BuildTigerV6SelectedIconTextures.ps1`（所选图标缩制脚本），在sRGB↔线性光转换与预乘Alpha下使用Lanczos3重采样。S03-C首稿存在白色圆角，已将旧稿移入 `Review/Rejected/`，新稿重新生成；S05-A候选含预制金色装饰框，只在Master阶段裁去边框，原候选源图未覆盖。上述措施不代表艺术效果已获得用户最终签收。

| 显示序号 | 技能V6名称 | 构图 | 目标引擎纹理名称 | 真实48 PNG SHA-256 |
|---|---|---|---|---|
| 1 | 白虎爪 | A | `T_DBA_Tiger_S01_WhiteTigerClaw_V6` | `e3817fc2665ca0a25543f58378efc37b714d82fa5df32ae03e66aafad60aadae` |
| 2 | 裂爪 | B | `T_DBA_Tiger_S02_ArmorRend_V6` | `2e615194fb3326440e642c71e72cdbc408e6536a329801a2c3ab29ca9a907c9f` |
| 3 | 双爪袭 | C | `T_DBA_Tiger_S03_DualClawAssault_V6` | `331675770575e431b3ea4ff2636e2409fde5e1cec1cadbd3419a4b62d49ed010` |
| 4 | 啸岭扑杀 | A | `T_DBA_Tiger_S04_RidgeHuntUltimate_V6` | `89d4df19d67bbfa7152681c10e315c64b57ce024a56a877b3e0a7c607094c7cc` |
| 5 | 扑杀 | A | `T_DBA_Tiger_S05_PredatorLeap_V6` | `1f555d3a7c1ec877fc5f99a67c458e764b05546fdd90e13c81225e6cddda41be` |
| 6 | 虎踞 | B | `T_DBA_Tiger_S06_TigerGuard_V6` | `657991df65e16f72643ca4ee44a53f7c754d49e9deb4008bfe689eba6f43aa47` |
| 7 | 虎啸 | B | `T_DBA_Tiger_S07_RoarWave_V6` | `68d0a083459f46be854131cc10aadaaab9425d8cddad9afe508f35608f09ab91` |
| 8 | 开猎 | B | `T_DBA_Tiger_S08_HuntMark_V6` | `d61a48be90d9e8503c8275653c684ea5a3f1d27b7449755af8b0f06b2e4e9857` |

运行端最后的**八个48×48原生Texture2D**拟存于 `/DBAHeroPack_Tiger/UI/Abilities/`，但上表只证明现有PNG。完整母版/64/48/候选来源SHA、实际槽位和稳定AbilityId见 `TigerV6SkillIconImportManifest_Draft.json`（纹理导入清单）。`TigerV6MonolithImportQueue_20261018.json`（编辑器导入动作清单）按八项真实PNG列出一一对应的目标原生对象路径。

## 已做的真实性检查

1. `pwsh -File Tools/Unreal/Abilities/BuildTigerV6SelectedIconTextures.ps1 -VerifyOnly`：八份母版与双尺寸存在且像素尺寸正确。
2. `pwsh -File Tools/Unreal/Abilities/ValidateTigerV6SkillIcons.ps1 -VerifyImages`：8张48×48 PNG格式、尺寸、稳定技能ID及哈希唯一性通过。
3. `pwsh -File Tools/Unreal/Abilities/ValidateTigerV6ArtPipeline.ps1 -VerifySelectedStaging`：用户8个选择、对应的真实候选1024、母版、64/48及实际SHA登记均通过；额外16项未选候选尚无独立图片，不宣称24图完成。
4. `Review/TigerV6_8Icons_64_48_VisualReview.png` 为离线PNG评审画板，**不是真实游戏截图，也不能替代人审及UE运行证据**。

## UE5.8原生纹理导入阻断

本次HWebCodex Runner的 `mcp_tool.list` 及 `plugin_tool.list` 均未返回可调用的Monolith Provider；`UnrealEditor` 当前不在进程列表，仅出现 `monolith_proxy` 代理进程，不能推定已连接正式编辑器。项目 `AGENTS.md` 规定UI原生资产必须用Monolith MCP创建/保存/冷重载，因此**没有创建或伪造任何新V6 .uasset**，没有更改现有 `DA_DBA_Tiger_PlaceholderUIProfile.uasset`，也没有改 GAS 授权和技能栏。

恢复编辑器与Monolith服务后，应严格按动作队列逐一用 `ui.import_texture_from_bytes` 创建8个真Texture2D，随后经 `editor.run_python` 对UI二维压缩、UI组、sRGB、NoMips、NeverStream、双线性和Clamp进行设置及引擎回读、保存/冷重载；确保只有客户端/Editor加载，不进入Dedicated Server最小Cook。用户对生成的**八张真实原画**进行最终人审通过、且服务器V6授权获批后，才允许在唯一Tiger UI Profile内按稳定AbilityId增补/更新映射。已有 `UDivineBeastsAbilityBarViewModel`（客户端技能栏视图模型）应继续复用异步资源租约及OwnerOnly授权，不新建平行服务。

目前状态：**源图片已入寅虎英雄包；引擎原生纹理、正式Profile绑定、新手村/战斗HUD与Cook验收尚未完成。**

## 2026-10-19 追加：UE5.8原生导入完成

此前2026-10-18“Editor未启动／Monolith无Provider”的描述是当日阻断历史记录。2026-10-19 已在正确工程启动UE5.8编辑器，直接通过本机Monolith MCP代理调用 `ui.import_texture_from_bytes` 完成八张正式48像素Texture2D导入，并用 `editor.run_python` 设置且保存原生纹理。独立UE5.8命令行进程对八份新资产均通过冷重载。具体原生资产路径、SHA与验收标记详见 `TigerV6UE58NativeIconImportReport_20261019.md`。当前艺术终审、唯一Tiger UI Profile生产绑定、GAS授权及Village/HUD仍未完成。
