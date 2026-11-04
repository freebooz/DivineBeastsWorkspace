# SSLM 小地图经典魔兽尺寸调整

地图区域由198×198调整为140×140，右上角整体容器由232×236调整为192×192。采用与现有头像栏一致的经典WoW逻辑尺寸基准，依据[Blizzard导出Minimap.xml镜像](https://github.com/wowgaming/3.3.5-interface-files/blob/main/Minimap.xml)中的Minimap和MinimapCluster定义；项目DPI曲线保持原值1.0。

现有窄金属框保持4单位径向宽度，外径相应改为148；圆形遮罩半径改为70。地图与框同心，标题及北向标识随框定位，三枚22×22缩放按钮沿右下边缘重新排列。固定字体14/11和玩家箭头16均保留，不修改底图、地图范围、UV缩放、移动事件或网络状态。

唯一当前配置为`Tools/Unreal/UI/CompactMinimap.json`，原生作者为`AuthorCompactMinimapUE58.py::layout_assets()`。此前Saved中的198尺寸作者入口已停用；修改前文件与三份资产备份位于`Saved/Validation/MinimapWoW20261103/Backup`。

实际修改的现有资产：WBP_DBA_UI_Minimap、WBP_DBA_UI_CombatHUD、MI_DBA_MinimapFrame。已通过Monolith编译、保存和引擎磁盘重载。既有MinimapImageBinding、MinimapViewport和MinimapProjection三项原生回归通过；独立只读审查无阻断项。记录见同一Saved目录中的Native.verify.json、NativeTests.json、NativeEngineEvidence.log和Review.md。

独立新Editor PID3364冷读通过，三资产哈希与保存结果一致。原生浮点RHI截图为Saved/Validation/MinimapWoW20261103/Minimap.Native.png，192×192画布的实际圆框中线跨度148像素。WidgetComponent缓存几何返回0，按实记录；声明的地图140×140通过原生控件读取核实，不伪造缓存值。截图为真实地图纹理的离线只读快照，未绑定当前玩家，不能代替人工鼠标或联机验收。

统一客户端Cook/Stage `d27c6fc3-6757-42b2-9c26-a43fe569e590` 已通过，绑定完整Editor构建 `89e405d6-d3a3-4346-9013-4e74093a46be` 和颜色RHI门禁 `4643eb6d-cd47-4669-b8a7-a77cd70b7f20`。本任务独立运行VerifyPackage.py，核对三资产源文件哈希仍匹配独立冷读，最终WindowsClient IoStore各有唯一非空ExportBundleData，结果见Saved/Validation/MinimapWoW20261103/ClientPackage.json。当前客户端窗口未重启；须从该新Stage包启动才能显示新尺寸，人工登录及交互验收仍待用户完成。

