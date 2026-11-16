# 子鼠 V6 定稿图标导入记录

2026-10-19，按用户要求导入下载目录 `SSLM_Rat_V6_ApprovedArt_HeroPack_ImportReady.zip`。ZIP SHA-256为 `703e7e4c04249ad4a3fc8097772c32aff15934ba4c1ef6a1bd9949c9572f7ba3`。原包保持在下载目录；仅接收清单指定的24张原稿及其稳定映射，没有运行包内代码。

## 实际资产与职责

资源归现有第三层英雄内容包 `DBAHeroPack_Rat`，没有新增插件、Profile、播放器或技能授权。1024原稿、64对照与48运行源分别归 `SourceArt/UI/SkillIcons/V6/Masters`、`64`、`48`。所有文件逐一校验原包SHA、PNG真实RGB格式及尺寸；正式48纹理由 Monolith MCP 0.23.0 的 `editor.run_python` 调用真实UE5.8.0编辑器导入、保存，并在后续独立Monolith调用中重载磁盘包。

| 资产名（挂载根 `/DBAHeroPack_Rat/UI/Abilities/`） | 稳定槽位 | 保留的V5名称 |
| --- | --- | --- |
| `T_DBA_Rat_S01_FangCut_V6` | Primary | 影牙斩 |
| `T_DBA_Rat_S02_ShadowBite_V6` | Slot1 | 伏牙刺 |
| `T_DBA_Rat_S03_CrossFangs_V6` | Slot2 | 交牙斩 |
| `T_DBA_Rat_S04_ShadowAssault_V6` | Slot4 | 三影袭 |
| `T_DBA_Rat_S05_CrackSlip_V6` | Slot3 | 穿隙 |
| `T_DBA_Rat_S06_TailVeil_V6` | Slot5 | 敛影 |
| `T_DBA_Rat_S07_TendonBite_V6` | Slot6 | 绊步牙 |
| `T_DBA_Rat_S08_ScentTrace_V6` | Slot7 | 嗅踪 |

正式映射与每张来源SHA见 [导入清单](RatV6_FinalTextureImportManifest_20261018.json)。包内候选V6名称仅作为历史美术元数据，不取代已批准V5名称。旧 `RatV6SkillIconImportManifest_Draft.json` 和冻结稿保留为导入前历史规划，不作为当前导入状态。

## 已执行验证

- 编辑器工程身份为 `Game/DivineBeastsArena.uproject`，UE为 `5.8.0-0+UE5`，现有英雄包已挂载；开始前无脏包。
- 8张原生Texture2D均为48×48、`TC_EDITOR_ICON`（UserInterface2D RGBA）、UI组、sRGB、NoMipmaps、NeverStream、Bilinear及Clamp。复用 `ImportDBAThemeTexturesMonolith.py` 的真实导入属性阻断策略，保持颜色调整为默认值，关闭Legacy Gamma。
- 保存后重载8个包并重新回读；UE原生PNG导出的RGB与原稿逐像素完全一致。
- 新8图经原生Canvas以白色乘色绘制至线性RGBA16F目标，比较18432像素；最大线性RGB误差 `0.00374874789446511`，低于 `0.008` 门限。审查图从真实线性RHI回读执行一次sRGB编码，未使用已知额外Gamma的Monolith widget截图路径。
- 已存在并真实执行 `DivineBeasts.UI.Color.SourceToSlate`：1通过、0失败、0跳过。该测试覆盖现有UI链，不包含本次Rat图；Rat图的独立GPU证据为上面的Canvas路径，不能将其声称为正式HUD的Slate验收。
- 旧Rat包全部6份资产、DBAGameplay中26份Rat资产及V5名称文件共33份SHA保持不变。用户此前未提交的Ox测试修改保持原状。

证据位于 `Saved/Validation/RatV6Import20261019/`：`SourceAudit.json`、`ProtectedBaseline.json`、`NativeImport.json`、`Reload.json`、`ReloadAndRHI.json`、`FinalVerification.json`、各图原生PNG导出与浮点RHI记录，以及 `NativeRHIReview.png`。操作脚本同目录保留中文职责、输入输出、阻断和清理说明，不作为运行时依赖。

## 验收边界与回退

本次完成源图接收和8张原生纹理导入，当前唯一PlaceholderUIProfile、V5显示名、AbilitySet与GAS均未修改；不能把8张美术资源当成8项正式技能已授权。未启动联机前端，未执行C++目标构建、独立编辑器冷启动、Client/Server Cook或人工HUD验收；后续前端验证仍须先核对Village/MainArena战斗版本。

回退对象仅为本清单24张新增源图、8张新增Texture2D及本次文档增量，实际删除前应复查新引用并经UE工具处理。旧V1和原Profile保持原位，禁止按英雄包整目录删除。代码中文说明检查范围为本次Saved操作脚本及本记录，不宣称历史代码已全量审计。
