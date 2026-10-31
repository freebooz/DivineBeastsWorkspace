# DBAWorldPack_MainArena（SSLM主竞技场世界内容包）

本内容包位于第三层DivineBeasts项目内容范围，仅持有正式MainArena世界地图与世界／体验／角色装配定义，不包含额外C++模块，不重新实现竞技规则、服务器分配或表现播放框架。

当前实际存在的UE5.8原生资产：

- `Content/Maps/L_DBA_MainArena.umap`：主竞技场地图。现有文件仅证明原生资产已落盘，布局、导航、多人出生及性能仍须引擎回读和场景验证。
- `Content/Definitions/DA_DBA_World_MainArena.uasset`：主竞技世界定义。
- `Content/Definitions/DA_DBA_Experience_MainArena.uasset`：主竞技体验装配定义。
- `Content/Definitions/DA_DBA_Pawn_ArenaCharacter.uasset`：竞技角色装配定义。
- `Content/Materials/M_Arena_CheckerStone.uasset`、`M_Arena_BoundarySlate.uasset`：本世界拥有的棋盘石面与深蓝灰边界/掩体材质。2026-10-31视觉改造保持原地图身份，场地500×500米、暗场照明，无地平线辉光；人工视觉、Cook和联机状态分别记录。

运行边界：`DBAWorlds`负责世界类型，`DBAArena`负责项目竞技装配，`GamePlatformArena`负责通用竞技机制；不同人数的1v1—5v5通过正式模式定义配置，不复制五套地图或服务器Target。服务器持有权威地图、碰撞、导航和必要装配数据，纯UI/VFX/音效等内容不得依赖此包的服务器必须加载根目录。

验收要求：分别检查地图与三个定义的资产类别、主资产身份和依赖闭包，执行Editor/Client/Server增量构建、竞技场双端Cook和真实多人准入、结算、退出后的实例清理。现阶段不凭文件数量或静态配置判定这些运行验证已经通过。
