# 神兽联盟写实积雪人工验收单（2026-10-11）

## 验收入口

正式工程：DivineBeastsWorkspace/Game/DivineBeastsArena.uproject；引擎UE5.8。独立审核关卡：/Game/Development/Snow/Maps/L_DBA_SnowReview。平台母材质为M_GP_SnowCover_Detailed，神兽联盟实例为MI_DBA_Snow_Detailed，增强降雪Niagara为NS_GP_Weather_Snow_Detailed。

## 人工验收步骤

1. UE编辑器打开上述独立关卡，确认水平地面、石台、25度/50度坡面和垂直墙面的材质引用均为第三层项目实例。不得批量覆盖正式新手村地图。
2. 选中名为SnowReview_WeatherAuthority的蓝图Actor，设置bApplyAtBeginPlay=true、PreviewWeather=HeavySnow（大雪）、PreviewTransitionSeconds=0，在PIE模式运行。蓝图在权威World内通过GamePlatformWeather修改天气，客户端应由天气快照驱动Surface的MPC，而不是在正式World直改材质。
3. 对比平面、坡面和竖直墙：不同倾角积雪符合重力方向，雪层边缘没有方块、重复纹理或明显贴图接缝。
4. 检查Snow微法线、粗糙度、世界遮罩与细节高度：不应只是纯白贴膜；原始地表与雪层自然过渡。
5. 切换成LightSnow（小雪）并重启PIE，观察SnowAmount从小雪到大雪的视觉差别；再切换Clear（晴），观察雪层减少和降雪Niagara停止，检查材质湿润度过渡。
6. 查看近、远景雪花与NE_GP_Snow_GroundDrift（贴地风吹雪粉），验证贴近地面、淡出、透明边缘、视距、地形穿透和不规则地形上的运动。当前雪粉水平漂移速度为固定可配置值，并非随实时风向旋转。
7. 在低中高画质分别检查透明粒子Overdraw（过度绘制）、Shader成本、GPU帧时与帧率，填写真实机器、显卡、驱动、截图、问题单，不预设不存在的合格数值。
8. 关卡负责人在正式Village地图选择落地地形材质后，另作运行验证与编译/客户端Cook/专用服务器Cook验证，不得仅凭SnowReview通过就宣布正式Village已接入。

## 自动化证据的局限

三张原创PNG已完成SHA256、尺寸、无缝性与透明通道检查。真实UE5.8已导入、保存并回读Texture2D、53节点雪母材质、6节点雪粉材质、第三层MI、Niagara与独立UMAP；Niagara复检0错误0警告、valid=true，雪VFX Definition与增强系统引用匹配。

自动化技术预览保存于Game/Saved/Screenshots/SnowReview，实际检查显示材质预览处于默认无雪灰色状态，离屏Niagara帧近乎全黑。因此**这些截图不是视觉验收通过的证明**。必须在PIE真实大雪状态重新捕获可辨识的雪层、粒子画面供人审查。

人工状态：待真人审核。人工签收人、时间、设备配置、截图路径和测试意见均未填写；禁止把自动化测试通过误记为人工通过。

## 回退

保留原版NS_GP_Weather_Snow。若新系统存在质量或性能缺陷，在UE编辑器将DA_GP_VFX_Weather_Snow的niagara_system引用改回原版，不改服务器天气权威和platform.weather.snow@1逻辑ID。
