# SSLM 湖边茅屋与柳树导入（2026-10-24）

用户依次要求将下载目录的 `ThatchHut_Low`、`WeepingWillow_Med` 导入并放在新手村湖边。本次通过直接 Monolith MCP 和锁定 UE5.8 原生 SpeedTree9 导入器完成，没有使用 HWebCodex。

## 实际资源与摆放

正式地图为 `/DBAWorldPack_Village/Maps/L_Village_Start`，在湖西北岸、林间路内侧新增两个 StaticMeshActor，均保持原生 Scale=(1,1,1)、Rotation=(0,0,0)，未改地形或现有森林。

| 对象 | Actor 标签 | 位置（UE 厘米） | 原生网格 |
| --- | --- | --- | --- |
| 茅屋 | Village_ThatchHut_Lakeside | (-9300, 7400, 365.327172) | /DBAWorldPack_Village/Environment/Buildings/ThatchHut/SM_DBA_Village_ThatchHut_Low |
| 柳树 | Village_WeepingWillow_Lakeside | (-7800, 7600, -6.586461) | /DBAFoliagePack_Core/Foliage/Trees/WeepingWillow/SM_DBA_Foliage_WeepingWillow_Med |

茅屋沿用原高脚结构，最低支脚嵌地约5厘米；原生包围盒约8.44×12.62×9.49米，9,603三角面，7个材质。采用真实网格静态碰撞，避免简单凸体封住模型空隙。柳树根节点按地形碰撞高度下沉5厘米，未把下垂枝叶的最低点误当树根；原包围盒约13.56×11.77×13.09米，403,098三角面，5个材质，作为装饰摆放并显式关闭碰撞。

植物资产归既有 `DBAFoliagePack_Core`，地图只拥有放置；Village 内容插件补充对植物包的显式依赖。未新增插件、运行生成器、自动散布规则或业务代码。

## 导入合同

- 两个 ST9 与25张配套PNG原图完整归档，逐文件SHA256与Downloads原文件一致。茅屋来源15文件，柳树来源12文件，记录分别归两包的 `Docs/ThatchHutImport_20261024.json`、`Docs/WeepingWillowImport_20261024.json`。
- 保留源RGB及Alpha；颜色/透射纹理采用sRGB，法线RGB+粗糙度Alpha采用线性TC_Default，避免BC5丢失打包Alpha；最小Alpha调整为0。材质复用引擎SpeedTree9父链。
- 本机导入器创建了法线纹理，但柳树实例的NormalRoughness仍指向EmptyNormal；已显式接入5张原法线/粗糙度，叶片Atlas另接Subsurface。茅屋14张纹理、柳树11张纹理均已核对真实参数。
- 通过引擎规范命名和保存，共39个实际原生资产，无该目录重定向残留。

## 验证与边界

39个资产原生DataValidation：零错误、零警告。39个包全部卸载重载，切换至前端地图后重新打开保存的正式新手村，核对模型、材质、位置与尺度。原32个Actor的变换及107棵原有ISM树木实例逐项一致，两个PlayerStart保持不变。

证据在 `Saved/Validation/ThatchHut20261024`：`ProtectedSceneBefore.json`、原地图备份、`AuditBeforeSave.json`、`AuditReload.json`、`AuditFinal.json`、`NativeAssetValidation.json`、`LakesideFinalReload.png`。最终截图为真实Editor GPU视口，未调色、改世界灯光或隐藏原有树木；先前偏黑的离屏SceneCapture仅为诊断，不作为最终显示证据。所有临时截图相机已销毁。

此次完成编辑器原生资产导入和正式地图保存。未执行Cook/Stage、客户端/服务器部署、联机准入、导航重建或GPU性能验收；柳树源文件仅含一个LOD且面数较高，不把单株编辑器可见当作量产散布性能通过。未改动现有天气/雪/音乐或其它开发任务。

## 后续人工验证准备

用户随后在编辑器调整了茅屋位置、尺寸及朝向，并复制为6株柳树；本轮保留全部现状，不恢复导入时的初始变换。最新位置快照归`Saved/Validation/LakesideClients20261024/dfabe834-625e-4fe3-902f-0f306c1f1966/LivePlacements.json`。后续加入的两种蕨类PCG实例也保留。

对实际无碰撞的6株柳树及2个蕨类HISM，通过Monolith原生反射设置`AlwaysLoadOnServer=false`，保留`AlwaysLoadOnClient=true`，解决纯装饰网格被正式地图带入专服Cook的引用错误。此为本地图已烘焙组件标记；以后重新生成实例仍需重新核对真实Cook剥离结果。

双端完整增量构建`12fe69ab-1402-4557-b688-d76a69277752`通过。后续双端Cook/Stage各自曾通过，但地图及服务器INI被并行工作持续更新，最终输入一致性门禁拒绝启动。当前未替换旧专服、未创建新客户端；等待稳定的发布窗口后再次同步，不将单独通过的Cook当成联机验收。

本轮证据和启动脚本归上述Saved目录；因E盘不足，后续独占Cook/Stage输出通过Saved下的新目录联接写入系统临时目录`SSLM-LakesideClients-dfabe834`，准确物理路径记录在`PhysicalOutputPaths.json`。没有删除或移动旧包、项目、账号或角色数据。