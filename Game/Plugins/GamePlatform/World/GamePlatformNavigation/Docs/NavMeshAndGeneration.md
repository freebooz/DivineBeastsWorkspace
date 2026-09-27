# NavMeshAndGeneration（导航网格与生成）

本插件不实现 A* 或第二套 NavMesh；所有路径查询都桥接 UE5.8 Navigation System/Recast NavData。共享代码中不存在自研网格或路径数据库。

`UGamePlatformNavigationWorldSubsystem`通过 `UNavigationSystemV1::GetNavDataForProps`按可信 AgentProperties（代理属性）选择当前世界 NavData，再使用引擎 `ProjectPointToNavigation / TestPathSync / FindPathSync / FindPathAsync`。

本轮没有修改项目全局 Runtime Generation（运行时生成）配置，也没有伪造 NavMeshBoundsVolume。Dynamic Modifier 是否触发 Tile Rebuild、World Partition Streaming 下 NavData 如何更新，必须由真实测试地图配置决定。

当前二进制导航资产为 0，因此 Dedicated Server NavData、Recast配置、SupportedAgents 和 World Partition NavMesh 的运行证据均为未执行。
