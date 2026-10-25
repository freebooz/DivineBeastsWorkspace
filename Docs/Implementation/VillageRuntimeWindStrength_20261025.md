# 新手村运行时风力调整

> 后续用户再次要求增强风力并改为阴天光照，当前目标已提高至0.70；本页为0.45阶段记录，见[阴天光照与增强风力](VillageOvercastLighting_20261025.md)。

用户在异步确认中选择“只增强游戏运行时的风力”，以此取代上一轮地图风源方案。

仅将 `/DBAWorldPack_Village/Weather/Definitions/DA_DBA_Weather_Clear` 的 WindIntensity 从0.25提高到0.45。原有风向0°、阵风0.20、频率0.10Hz和其他天气字段逐字对照保持；运行时继续由服务器天气快照经Surface共享MPC驱动全部已接入植物。

通过Monolith精确移除上一轮创建的 `Wind_Village_Global_WeatherDriven`，保留当时场景中的其他Actor、PlayerStart、材质、PCG实例和碰撞/导航。保存前备份两个真实资产，独立Editor冷重载确认地图风源数量为0。

取证和交付记录位于 `Saved/Validation/VillageRuntimeWindStrength20261025`。源码及构建产品一致时复用上一轮完整Editor/Client/Server构建与13项原生回归；内容变化执行新一轮双端Cook/Stage，不重复编译未变化C++。真实联机准入、人工植物方向/根部视觉和GPU预算验收分别保留状态，不由冷重载或打包成功代替。

## 验证结果

- 独立Editor冷重载通过：地图风源数量0，晴天Definition仅WindIntensity变化。
- 正式新手村PIE运行时探针通过：单一天气复制Actor，MPC目标(0,0.45098,0.20,0.10)，无需地图风源。前两次只因Python子系统入口/私有属性读取不可用，改用真实共享材质参数读取后通过，未修改游戏代码。
- 新Client Cook/Stage：`09455477-33ab-409e-adb8-62a6f9b8d374`；新Server Cook/Stage：`cb66e13b-b7e6-4399-a710-1f9dd832610d`，资源及专服剥离门禁通过。18项植物风材质资产保持原哈希。
- 实际Village/MainArena已更新并核对新包，Village内72项技能序列化检查通过。两个更新后客户端已打开登录页，真实联机准入及人工视觉仍待确认。
- 最终分层记录：`Saved/Validation/VillageRuntimeWindStrength20261025/FinalValidation.json`。
