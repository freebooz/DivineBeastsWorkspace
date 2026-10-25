# 新手村地图全局风源

> 后续用户明确选择“只增强游戏运行时的风力”，本页为上一轮历史记录。地图风源已撤回，当前执行结果见[运行时风力调整](VillageRuntimeWindStrength_20261025.md)。

沿用已批准的Weather→Surface→MPC链，新增一个可选中的原生WindDirectionalSource，标签`GamePlatform.Weather.GlobalWind`。不新增风Subsystem、Actor类型、逐株Tick或第二份风状态。

- 地图源是初始天气的美术配置：Yaw表示吹向的世界方向，Strength为0..1，Speed在此合同中为0..2Hz阵风频率，MaxGustAmount为0..1阵风强度。首次ActivateWeather发布前捕获四个风字段，保留给异步初始预设；首次显式天气/调度过渡后消费初始化优先级，后续快照保持权威。
- Runtime客户端将同一快照投影到原生风源与既有MPC；编辑器通过地图打开、属性变化、根组件TransformUpdated、删除及撤销事件更新预览，SurfaceEditor拥有预览参数写入，禁止写资产默认值。
- 默认新手村源0°、强度0.45、阵风0.25、频率0.15Hz。原有地图Actor/实例/出生点/碰撞/导航保持，先备份，再通过Monolith创建并保存。
- 原生回归覆盖首次配置、后续天气优先、重复源拒绝、客户端同步与退出解绑；完成完整增量目标和双端Cook后再更新验证客户端。运行包、真实准入、人工视觉与GPU测量分别报告。

## 2026-10-25 原生落地与验证

正式新手村已通过Monolith加入唯一原生风源 `Wind_Village_Global_WeatherDriven`，Yaw=0°（吹向世界+X）、Strength=0.45、MaxGustAmount=0.25、Speed=0.15Hz。地图保存前备份位于本轮Native目录，原有Actor、组件、材质及PCG实例变换逐项对照一致。

- 独立Editor PID 50812冷重载：`Saved/Validation/VillageGlobalWindField20261025/Native/Audit_Cold.json`；编辑器实际MPC To=(0,0.45,0.25,0.15)。程序化旋转到90°时MPC立即为90°，恢复0°后通过原场景保护审计。根组件变换事件覆盖程序化移动和旋转。
- 完整Editor：`e78327e1-f014-4ce4-9ad1-b81a8aa129b2`；最终13项原生Automation：`3e8e339e-5541-40ef-b552-052e2e4ccc38`，全部Success且构建输入在测试后复核一致。
- PIE正式地图固定相机：`Native/PIECapture.json`中13个验收帧有效，无无效帧；对应MPC风力0.45098为网络量化后的0.45。`Native/VillageWind_PIE.mp4`由真实连续viewport帧编码；只为PIE取证相机临时设置曝光偏移，未保存世界或改变正式光照。
- 首次抓取有启动未预热帧且日志标记晚于天气初始化，因此未计通过；第二次预热后重新取证通过。视频属于Editor PIE，不等同打包客户端、多人准入、人工方向/根部视觉确认或GPU预算验收。

本轮编译、Cook、实际运行进程和双客户端状态以 `Saved/Validation/VillageGlobalWindField20261025/FinalValidation.json` 为准。未标记真实准入或人工接受的层级继续保留为待验收。
