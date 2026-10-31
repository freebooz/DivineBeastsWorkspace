# SSLM 主竞技场开发内容

本内容插件属于第三层世界内容，只拥有真实地图、世界/体验/Pawn定义和静态导航。规则和网络归DBAArena及Go控制面；不包含UI、凭据或第二套匹配器。

地图 `/DBAWorldPack_MainArena/Maps/L_DBA_MainArena` 由Monolith正式Editor创建、保存。2026-10-31按用户参考图调整为黑色背景、深灰蓝棋盘石面和暗场冷光，地面扩大至50000×50000厘米（500×500米）。双方出生x为-2300/+2300，同队y间距350，z为120，四处掩体保留中央位置。每队有5个唯一标签team-a:0至4、team-b:0至4，五种人数按固定Roster顺序选择。9个BlockAll静态网格提供地面、四周边界和掩体；全场导航7803Tiles、脏区0，两队中间出生点之间真实非Partial路径4600厘米，两条全场对角路径均可通行。

当前世界自有材质为`Materials/M_Arena_CheckerStone`及`M_Arena_BoundarySlate`。棋盘格100厘米，主光5.5 lux、补光0.22 lux，固定曝光Min/MaxBrightness=3.0（本工程未启用扩展亮度范围，不能视为EV100）。按用户最终指令删除地平线辉光、背景球及远地面/边界自发光渐变。最终保存重载、材质原生编译和编辑器人工检查入口见`Docs/Implementation/MainArenaReferenceDesign_20261031.md`及`Saved/Validation/MainArenaReference20261031`。

当前白盒地图和候选技能仅开发联调。客户端/服务器必须同时使用显式CustomConfig及-DBADevelopmentArena，Shipping/Test禁止放行；没有标为ProductionReady。默认打包仅包含服务器安全定义，不纳入开发地图；VillageDevelopmentClient/Server显式加入地图进行双端联调。

制作证据见ContentGenerationManifest.json及Saved/Validation/PartyArena20261015/Content。保存、导航校验不能替代冷读、Cook、网络和人工视觉，尚未完成的阶段保持false。
