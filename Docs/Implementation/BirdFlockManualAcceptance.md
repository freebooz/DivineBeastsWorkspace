# 通用鸟群P7人工验收单

工程：DivineBeastsWorkspace（神兽联盟工作空间）｜引擎：UE5.8｜状态：待人工确认。

自动化验证与UE截图是人工判断的证据，不代替人工审美、镜头遮挡和目标设备体验确认。未填写审核人、结论与时间前，P7不得标记通过。

## 一、打开真实审核关卡

优先打开 `/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview`（正式表现链路审核）。场景内三个 `BirdIntegration_*` Actor（海鸥/燕子/雄鹰）都使用同一蓝图，开始PIE后通过真实Presentation/Data/VFX链路生成鸟群。

辅助对比关卡为 `/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview`（直接GPU视觉审核），用于查看纯Niagara效果，不用它代替正式链路验收。

项目共用蓝图：`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockAmbient`。在客户端表现子关卡放置，勿放入专用服务器需要加载的共享权威地图。已有主编辑器正在编辑角色圣境/地形时先保存自己的工作，或另开本地审核窗口；不要覆盖其他人的未保存资源。

## 二、鸟种和数量设置

选择场景中的鸟群Actor，选中其 `BirdFlock` 场景组件。在 `SpeciesId（鸟种）` 下拉中选择 `Seagull（海鸥）`、`Swallow（燕子）` 或 `Eagle（雄鹰）`；`BirdCount（数量）` 设置0–512。`FlockId（鸟群身份）` 保持同一本地玩家范围内唯一，避免多个场景Actor互相替换请求。

编辑普通关卡后保存，再进入PIE观察。运行时也可调用组件 `ApplyBirdFlock（应用当前设置）` 和 `StopBirdFlock（停止本群）`；后台加载失败或非法输入会明确拒绝，不用假资源遮盖错误。0代表停止，不是把鸟缩小至不可见。

三种WorldDefinition（物种数据资产）的默认数量为海鸥32、燕子72、雄鹰5。需要调整路径、半径、高度、速度或风扰动时，在各自 `DA_DBA_VFX_BirdFlock_*` 的 `DefaultParameters（默认参数）` 中修改已列入白名单的字段；不要使用历史ISM原型预设替代正式WorldDefinition。

## 三、人工逐项确认

- [ ] 鸟种可辨：海鸥浅色羽翼，燕子深色细翼与叉尾，雄鹰棕色宽翼；不是三套同色白色模型。
- [ ] 羽翼渲染正常：没有重复共面三角面导致的黑白条斑、深度闪烁或权重丢失；模型尺度合理。
- [ ] 群飞自然：盘旋/掠过/八字与个体相位存在差异，转弯不过度翻滚；远景轮廓可辨。
- [ ] 数量0、1、32、128、512符合设置；保持同一数值超过10分钟不会周期性增生，停止后没有残留鸟群。
- [ ] 相机与地形安全：飞行高度、半径和场景锚点避免穿地面、水面或建筑；当前不具备全场景自主避障，必要时调整安全飞行区域。
- [ ] 角色选择与创建画面：少量远景鸟群不挡角色面部、技能展示、按钮或主要交互；同一预览舞台退出后取消。
- [ ] 新手村/开放世界：生态分布与湖泊/林冠/山崖环境相符，不同视距和画质档位不会突然形成过密前景。
- [ ] 目标设备表现：独占目标设备运行，记录分辨率、画质、CPU/GPU帧耗和同时存在的其他特效，确认项目实际预算。

## 四、本轮已经准备的证据

`Saved/Validation/BirdFlockP4P7/P4AssetsValidation.json`：真实资产/蓝图和依赖方向检查。
`EmitterLifecycle.json`：四个Niagara系统的Self/Once单次生成配置与编译检查。
`GPUReview/GPUReview.json`：15组GPU数量与五张真实1920×1080UE视口截图，文件名以该清单最新条目为准。
`Integration/IntegrationReview.json`：正式蓝图自动启动、三鸟种数量切换、非法输入、停止及世界清理。
`Soak/IntegrationReview.json`：10分钟稳定性运行结果，必须看到最终status=passed；历史失败报告保留在同目录BeforeLifecycleFix文件。
`Automation/index.json`：三项客户端回归测试。
完整Client/Server构建和最终Cook产物目录见 `BirdFlockP4P7Execution_20261011.md`。

## 五、人工结论

审核人：待填写。
审核时间：待填写。
审核设备与分辨率：待填写。
结论：待确认（通过／需要调整／拒绝）。
需调整项：待填写。

当前资源定位是中远景低面数环境鸟群，并非影视级近景羽毛角色。将它用于大幅近景角色展示时，需要另行提升模型、羽毛材质和精细翼部动画，不能以自动化通过替代这项美术要求。
