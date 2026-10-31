# SSLM 插件演示场景与 10 号字体交付记录

日期：2026-10-31。用户已批准设计；当前为真实资产、源码、构建和开发包交付记录，人工视听签收保持未通过。

## 已实施

- 审查 70 份插件描述，保留 288 项登记功能、211 个案例和 390 个审核用例的稳定身份；70 个独立插件页面均存在。功能分母仍未知，本次不声明所有插件业务已经实现。
- 通过正式 UE5.8 的 Monolith 修改、编译并保存 WidgetBlueprint；独立进程 PID 43468 回读 163 个 WidgetBlueprint 的 1,939 个有效字体字段全部为 10，3 个共享 CommonTextStyle 同步为 10。登录和角色名输入的 RenderScale 仍为 1。
- 修改运行时动态字体、调试面板局部 Slate 样式，以及 DeveloperTools、BlueprintAutoLayout、GlooPrint 的插件自有菜单文字。借用的引擎菜单保留原 StyleSet；原生诊断 `ScopePreserved=1`。
- 新建 4 个真实关卡：VFX、表面印记、相机效果、内容资产展示。复用地形、水体、天气、雪面、SFX 等真实关卡；最终 80 个案例重新对应 9 个目标场景。包括 Hub 和鸟群地图，当前编目共声明 11 个地图包。
- 水体案例使用具有 23 个 Actor 的 `/Game/Development/Water/L_GP_WaterReview`，没有绑定 `Water/Maps` 下的同名空图。PCG 检查和生成继续保留授权的精确 Hub 世界。
- 唯一 Manifest 派生 Catalog 已原生重建并独立重载，SHA256 为 `0da15c7b6462e5ee0cb80a2863d2180f1a43cdc00acbf8bde2adb0706e02f582`。

## 验证结果

| 门禁 | 实际结果与证据 |
| --- | --- |
| 离线回归 | Manifest 28、既有地图路由 6、新增场景路由 7，共 41 项通过；PCG 宿主回归观察到修复前失败和修复后通过 |
| 原生字体与 Catalog | `Saved/Validation/PluginSceneFontAudit/20261030/Fonts.Verified.json`；独立新进程回读通过 |
| 原生 UI 自动化 | `DivineBeasts.UI.Architecture.BaseClassHierarchy`、`DivineBeasts.UI.Login.NamedWidgetContract`、`DivineBeasts.UI.Login.PasswordRevealLifecycle`、`DivineBeasts.PluginDemos.IndependentPluginPage`，4 项实际执行通过 |
| 完整 Editor 增量目标 | 修正后 `64c5cf5e-78f6-4586-9197-8c3b4e9b8996` 通过；Cook 后只变化两份 FileOpenOrder 生成日志，按同参数刷新证明 `c6b58b8b-0d5d-4b1e-82eb-1e88d6586130` 通过，0 个重新编译动作 |
| 完整 Client 增量目标 | `0e9e973d-8779-4881-97f2-91179f349c0d` 通过 |
| 开发 Client Cook/Stage | `04c284f7-1d58-4e95-a840-95f8c02bb3c1` 退出 0；实际 IoStore 容器清单确认 11/11 地图存在，见 `ClientStageMapAudit.json` |
| 原生场景图片 | 4 个新关卡独立重载后捕获 `VFX.Scene.png`、`SurfaceMarks.Scene.png`、`Camera.Scene.png`、`AssetGallery.Scene.png` |
| 独立运行 | 启动器严格门禁通过，RunId `19c31d5a-092f-41af-a707-5f8d9c66fc89`、PID 46020；实际显示 VFX 独立插件页及当前目标关卡路径。仅为离线 EditorGame，不连接 Village/MainArena |
| 代码审查 | 两项 Important 已修复：PCG 精确 Hub 资格、插件字体覆盖范围；`git diff --check` 通过 |

固定构建参数沿用 UE5.8、Win64/Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`；Editor 加 `-UsePrecompiled -AdditionalPlugins DBAServer`。没有 Clean、Rebuild、提交或推送。所有备份、临时、Cook、Stage 和证据均在工程内。

## 保留的验收边界

- 本轮已看到独立页面真实渲染；“进入案例、执行、停止/取消、返回”的完整点击链、较小窗口和 DPI 变化尚未取得本轮完整运行证据。电脑操作检测到用户输入后未持续抢占窗口；这些项继续保留，不能以离线路由测试替代。
- 4 张场景图证明原生关卡与测试参照物存在，不代表全部 VFX 已播放或视觉质量已人工通过。没有自动填写任何人工通过结论。
- 雪足案例仍需要真实角色与雪接触配置；现有业务、资源和第三方编辑器夹具前置条件继续有效。
- 未执行本轮 GPU/多人压力、动态视听录像、Village/MainArena 网络准入或服务器包验收；本次不认证 Production Ready。
- 新手村地图、植被、AI、水体编辑器桥接等初始用户修改保留。本轮场景与字体工作没有改写新手村湖泊。

完整证据目录：`Saved/Validation/PluginSceneFontAudit/20261030`。构建与 Cook 原始结果：`Saved/Validation/FoundationM0/<RunId>/Build/result.json` 或 `Cook-Client/result.json`；启动原始结果：`Saved/Validation/PluginDemos/Launch/19c31d5a-092f-41af-a707-5f8d9c66fc89/Launch.json`。
