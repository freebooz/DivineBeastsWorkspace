# 写实积雪后续接入与验证记录（2026-10-11）

## 本轮继续检查

对 D:/SnowValidation20261011/FinalV3/SnowClientContainer.csv 和 FinalV3Server/SnowServerContainer.csv 再次执行真实UnrealPak清单门禁：客户端10/10项资源存在，客户端CSV记录881条；服务器必需的Village地图和HeavySnow天气定义存在，CSV记录553条，违规的GamePlatformSurface、GamePlatformVFX、DBAPresentationPack_Core、Development/Snow资源为零。静态三层/Cook配置检查通过，但这不代替PIE、网络与性能验证。

检查 Game/Saved/Validation/SnowVillageMaterialInventory_20261011.json 发现正式 Village 起始地图只有8个Actor，其中Landscape为1个，LandscapeMaterial路径为空，静态网格材质槽为0。正式地形需要独立美术基础材质和客户端视觉雪层装配；不能将DBAPresentationPack_Core纯客户端MI硬引用写入共享Server/Client地图，否则违反Dedicated Server资源隔离。

项目现有GamePlatformWeather天气数据桥接与Surface MPC已经存在，当前缺口不属于另建天气插件；应在DBAClient客户端世界生命周期内通过GamePlatformData软引用获取项目MI，明确仅NM_Client/NM_Standalone或带本地玩家的PIE表现世界应用，世界销毁释放引用。正式LandscapeMaterial覆写涉及UE Landscape的材质重建/组件代理更新及编辑器、客户端兼容，需要针对真实API验证后执行，不能仅以C++ SetMaterial(0)替代地形材质变更。对共享Village世界原始地图继续保持只读。

## 本轮尚未完成的人工门禁

Monolith /health 于本轮9316、9317端口均不可用，不对正在被其他会话使用的编辑器强制结束、重启或覆盖其临时地图。当前未执行新的可见PIE、联机重连、GPU性能采样；不能将此前黑屏离屏捕获认定为合格。正式全目标编辑器/客户端/服务器最新C++编译记录不在本轮重复声称成功，必须依据原始UBT构建日志另行核定。

## 安全完成条件

1. 完成客户端专属的Landscape可渲染基础材质及纯表现雪层适配，确保无对DedicatedServer硬引用。
2. 完成正式Village实际Client/Server启动，独立验证降雪天气的Surface数值、材质显示和粒子生命周期。
3. 重新获得无遮挡实际游戏画面，检查水平/坡面/垂直覆盖、光照质感、雪粉穿地、GPU成本；人工签收。
4. 在所有源代码和绑定资产稳定之后执行当前分支完整三目标UE构建、Client/Server新Cook与IoStore审计，不复用旧构建日期冒充新通过。
