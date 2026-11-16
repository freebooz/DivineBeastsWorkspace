# 巳蛇 V6 用户指定图像导入记录

2026-10-19按用户要求接收下载目录 `SSLM_SnakeV6_ImportReady.zip`，SHA-256为 `691d9acc62b66000b1edc8082602b33d29ffeeb2bfcb398377db2391c6b089b4`。包内包含24份真实PNG与8项映射，不含UE资产；只读取固定清单与图像，没有执行包内代码。原ZIP及其原始映射保留，未把包内尚未人工终审的图像提升为正式美术批准。

## 实际导入

目标为现有第三层 `DBAHeroPack_Snake`，不新增插件、Profile或播放器。8份1254×1254独立源稿原样置于 `SourceArt/UI/SkillIcons/V6/SelectedMasters`，8份64×64图置于 `Final/64`，8份48×48导入源置于 `Final/48`。PNG逐一验证SHA和真实尺寸，RGBA版本验证Alpha全部255；旧程序化 `Masters/48/Review` 和V1均未覆盖。

Monolith MCP 0.23.0在 `Game/DivineBeastsArena.uproject`、UE `5.8.0-0+UE5` 中执行真实 `AssetImportTask`，创建下列Texture2D，并在后续独立Monolith调用中重载磁盘包。根挂载点统一为 `/DBAHeroPack_Snake/UI/Abilities/`。

| 资产名 | 槽位 | 当前批准V5名称 | 包内V6候选名 |
| --- | --- | --- | --- |
| `T_DBA_Snake_S01_ScalePierce_V6` | Primary | 幽鳞刺 | 幽鳞刺 |
| `T_DBA_Snake_S02_VenomFang_V6` | Slot1 | 毒牙 | 毒牙 |
| `T_DBA_Snake_S03_TwinScaleBolt_V6` | Slot2 | 双鳞弹 | 双鳞弹 |
| `T_DBA_Snake_S04_VenomTide_V6` | Slot4 | 毒阵爆 | 万毒归鳞 |
| `T_DBA_Snake_S05_SerpentDash_V6` | Slot3 | 蛇行 | 蛇行 |
| `T_DBA_Snake_S06_ShedScales_V6` | Slot5 | 蜕鳞 | 蜕鳞 |
| `T_DBA_Snake_S07_CoilSnare_V6` | Slot6 | 蛇纹缚 | 缠纹 |
| `T_DBA_Snake_S08_PoisonTrail_V6` | Slot7 | 毒径 | 封途 |

稳定AbilityId、原稿路径及逐图SHA见 [本次执行清单](SnakeV6SelectedTextureImportManifest_20261019.json)。该清单说明当前导入状态；旧 `SnakeV6SkillIconImportManifest_Draft.json` 和视觉冻结稿保留历史规划与美术审核职责，不作为当前UE导入状态。

## 实测证据

- 操作前确认正确工程、英雄包挂载及0脏包。8张原生Texture2D实际48×48，UI组、`TC_EDITOR_ICON`、sRGB、NoMipmaps、NeverStream、Bilinear、Clamp，Legacy Gamma关闭，所有颜色调整保持默认。
- 复用项目现有 `ImportDBAThemeTexturesMonolith.py` 的属性阻断审计；8张保存包重载后再次回读来源和原生设置，UE原生PNG导出的RGB与源图完全一致。
- 8张以白色乘色通过原生Canvas绘制到线性RGBA16F目标，比较18432个真实RHI像素，最大线性RGB误差 `0.00374874789446511`，小于 `0.008` 门限。`NativeRHIReview.png` 仅将浮点回读执行一次sRGB编码，没有调用已知重复Gamma的Monolith widget截图路径。
- `DivineBeasts.UI.Color.SourceToSlate` 本次实际执行：1通过、0失败、0跳过。它验证现有UI链，本次Snake图的专属GPU检查是Canvas路径；两者均不冒充正式HUD Slate验收。
- 旧包资产、源图草稿、Snake权威技能、V5名称和既有规划共58份SHA未变；其他英雄与音效链的既有/并行修改未覆盖。

证据及有中文职责说明的操作脚本位于 `Saved/Validation/SnakeV6Import20261019/`：`SourceAudit.json`、`ProtectedBaseline.json`、`OriginalManifest.json`、`NativeImport.json`、`Reload.json`、`ReloadAndRHI.json`、`FinalVerification.json`、各图UE PNG导出及浮点RHI记录。本包 `Docs/MonolithGenerationManifest.json` 追加本次记录，保留历史记录。

## 剩余验收与回退

8张资源已导入；正式UIProfile绑定、V6名称及GAS授权未变。未执行独立编辑器冷启动、Client/Server构建与Cook、联机前端或人工美术/HUD验收。未来启动前端仍须执行Village和MainArena战斗版本一致性门禁。

回退仅针对24张新增源PNG、8张新增Texture2D及本次文档增量；实际删除前核对新增引用并通过UE工具处理，不按英雄包整目录删除。原草稿、V1、Profile与权威资产保持原位。中文说明审核覆盖本次操作脚本和交付记录，未宣称历史工程全量合规。
