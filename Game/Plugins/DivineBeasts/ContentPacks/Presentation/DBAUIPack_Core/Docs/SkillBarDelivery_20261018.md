# SSLM 技能栏精修与透明纹理接入

用户于2026-10-18审核通过 v02 拆分稿并要求继续执行。项目保持 `Game/DivineBeastsArena.uproject` 身份，原位替换现有公共战斗 HUD。源图和审稿包保存在 `Saved/ArtReview/AbilityBar_20261018_v02`；参考原图2172×724，SHA256为 `4aaf70e028e56053ee19253241daf338fdd33686e309cbbcc97195dbf2fb11bd`。

## 文件与所有权

- `SourceArt/UI/SkillBar/Color/`（彩色原稿）：用户指定的13张 RGBA PNG，保留审核后的像素。
- `SourceArt/UI/SkillBar/Masks/`（独立遮罩）：对应13张线性灰度 Alpha PNG。
- `SourceArt/UI/SkillBar/AssemblyLayout.json`（布局合同）：727×94逻辑像素、底部居中锚点及各部件坐标。
- `SourceArt/UI/SkillBar/TextureManifest.json`（制作清单）：尺寸和原图区域；生成阶段的相对证据路径以 `ProvenanceRoot` 为根。
- `Content/UI/Textures/SkillBar/`（引擎纹理）：26份真实 Texture2D，由 Monolith 导入和保存。
- `Content/UI/Combat/WBP_DBA_UI_SkillBarMomentum.uasset`（气势投影）：新增的事件消费控件；继承既有 `DivineBeastsPlayerStatusPanel`。
- `Content/UI/Combat/WBP_DBA_UI_AbilityBar.uasset`（技能栏装配）、`WBP_DBA_UI_AbilitySlot.uasset`（既有技能格）、`WBP_DBA_UI_CombatHUD.uasset`（战斗根布局）与 `Content/UI/Components/WBP_DBA_UI_CastBar.uasset`（施法显示）：沿用稳定资产身份，通过 Monolith 修改。
- `Docs/SkillBarTextureImportManifest.json`（导入身份与哈希）、`Docs/MonolithGenerationManifest.json`（原生制作证据）：分别记录纹理与控件验收。

通用机制继续属于 GamePlatformUI；公共视觉属于 DBAUIPack_Core；业务 ViewModel、本地玩家路由和适配属于 DBAClient。本次没有新增插件、平行技能系统或业务轮询。

## 透明度、尺寸与布局

彩色图启用 sRGB，独立 Alpha 关闭 sRGB；全部使用无损 UI 压缩 `TC_EditorIcon`、`TEXTUREGROUP_UI`、无 Mip、Clamp 与双线性采样。调色参数保持默认，控件纹理乘色为白色。RGBA 的 Alpha 已完整，当前普通 Image/Brush 直接使用 RGBA；独立遮罩供需要单独透明度输入的材质使用，禁止将其再次乘到已有 Alpha 上造成透明度平方。

技能与功能槽48×48逻辑像素；图标纹理32×32，界面中24×24。施法与气势轨道高3逻辑像素。技能栏727×94，在 CombatHUD 内锚定底部中心，位置为零；根裁切只去除审核稿记录的极低 Alpha 重采样尾迹。字体和输入控件不整体缩放。现有状态条、头像移至技能栏上方，保留实际数据绑定。

原图八个技能框中，五个仍对应 `PrimaryAbilitySlot`、`ActiveAbilitySlot1`、`UltimateAbilitySlot`、`ActiveAbilitySlot2`、`PassiveAbilitySlot`；另外三格仅显示空背景。五个真实槽改为自身可命中，各自只覆盖48×48区域，修复图标/文字不接收指针后槽位无法点击或显示提示的问题；被动和未授权槽仍由原生命令门禁拒绝施法。不能将空格视作新增可用技能。无 Pawn 或技能未授权时仍遵守既有禁用显示。

## 事件与交互

施法沿用 `GamePlatformCastProgressWidget` 的状态通知及引擎动画。气势通过 `DivineBeastsPlayerStatusPanel` 的当前 Pawn / ASC ViewModel 事件进入蓝图：比例钳制至0—1，投影到527像素轨道；无有效容量时隐藏游标。首次快照、换 Pawn 和来源清理沿用原生类，新增蓝图没有 Tick 或计时器轮询。

背包按钮调用拥有者 LocalPlayer 的既有背包入口，再次点击关闭页面或取消在途打开。组队按钮明确打开 `UI.Party.Panel`，本地已有同一面板时复用，不增加业务订阅。齿轮直接调用 `OpenSystemSettings`，关闭本协调器的背包/日志后进入真实设置页；从菜单进入时保留父菜单，直接进入时关闭设置恢复世界输入。左右键与前后导航在三个按钮之间显式连接，边缘允许退出；悬停添加细碧绿轮廓，常态保留审核纹理。蓝图路由已保存重载；配套原生代码等待共享编辑器空闲后的增量编译，当前运行DLL尚未获得这些修复。

技能加载仍为英雄定义 → 服务器 AbilitySet/GAS 授权 → OwnerOnly 快照 → 本地 ViewModel → 已保存技能格。十二生肖的60条授权配置与UIProfile逐项核对一致，输入为普通攻击、被动、Slot1、Slot2、Slot4；猪生肖的正式资源名为 `Boar`。名称、描述、等级和禁用原因按槽位与技能双重身份投影，详情单独改变也会刷新；空槽与换角色清除旧详情。重复授权槽全部拒绝，显示和点击共享唯一性检查。连续冷却保留引擎材质动画，文字仅显示冷却状态，不新增业务轮询或冻结的秒数倒计时。

## 当前验证与边界

已通过：26份纹理导入阻断审计、26份 UE 原生导出与审核 PNG 的逐通道完全一致检查、31份资产保存和重载、5个控件蓝图零错误与零警告编译。新增功能按钮的静态可访问性检查零问题。

本轮已通过：技能栏按钮蓝图再次编译零错误零警告、保存和独立重载；十二生肖60条配置映射核验；`ValidateZodiacAbilityIntegration.py` 52/52静态断言。新增加三个原生回归：`DivineBeasts.UI.Combat.AbilityDetailsProjection`、`DivineBeasts.UI.Combat.AbilityGrantIdentity`、`DivineBeasts.UI.SystemMenu.SkillBarSettingsClose`，覆盖详情事件、冲突授权和实际CommonUI Router输入模式恢复，尚未编译运行。

待完成：`DivineBeastsUIClient` 最小增量编译、上述三个新测试及必要的技能租约/重构/按钮路由回归；执行 `DivineBeasts.UI.Color.SourceToSlate`、`DivineBeasts.UI.SkillBar.EventProjection` 与 `DivineBeasts.UI.SkillBar.RHIBottomFlush`。共享编辑器仍由动画、治疗特效任务使用，按已批准的空闲后重启流程继续。实际客户端交互、Cook/Stage 和最终人工视觉验收单独记录，源图预览及静态审计不能替代这些结论。

操作前的四个既有控件文件备份位于 `Saved/Validation/SkillBar20261018/Before/`；回退时仅恢复本次对应资产并用引擎重载，不覆盖工作区中其他任务的改动。
