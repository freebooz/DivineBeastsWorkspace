# 通用鸟群P4–P7续执行记录

工程：DivineBeastsWorkspace（神兽联盟工作空间）。依据：BirdFlockVFXImplementationPlan.md（鸟群总计划）；本记录保存本轮新增证据，不把历史计划状态当作当前工程事实。

## 核验基线

2026-10-11，本轮开始主分支提交936ae0ab63d55c021facdcca42da20bd9febb1e7。工作区已有UI、地形、圣境场景和鸟群审核脚本的未提交修改，均保留。HWebCodex旧Session已过期，持久执行依据改为本文件与实际源码/资产日志。

实际已存在：平台NM_GP_BirdFlockMotion、NS_GP_BirdFlock_Base、NET_GP_BirdFlock_Ambient；项目三种Niagara系统和三种VFX World Definition；DivineBeastsBirdFlockSceneComponent（项目场景组件）、BirdFlockPresentation（表现请求编排）、项目目录映射、选角与世界回调接入源码。以上均须重新验证，不重复生成。

当前打开的UE5.8主工程编辑器没有未保存资产、没有PIE，World为临时空关卡；它尚未加载新增鸟群反射类。此前Editor链接错误为DLL正被加载（LNK1104），不是鸟群源码编译通过的证明。

## 本轮执行顺序

P4：检查真实Niagara编译及资产引用，完成项目场景蓝图，鸟类/数量在组件详情中可编辑；已有三种World Definition作为正式物种数据资产，避免再建不走Data/Presentation的平行资产管理系统。
P5：验证既有选角、创建角色、Village/OpenWorld事件链路，审核关卡同时提供直接GPU演示与正式场景组件入口。
P6：完成Editor/Client/Server构建，真实参数/资源/表现链路与数量边界测试，真实GPU渲染截图，Cook及服务器排除检查；失败项保留原因。
P7：打开实际鸟群审核场景并提供人工验收步骤；人工确认之前始终标记“待人工验收”。

## 决策与门禁

- 裁决：正式运行只走项目中立表现请求→平台Presentation→Data租约→GamePlatformVFX Niagara执行器；旧ISM Actor仅为历史原型，不复制为正式蓝图父类。
- 裁决：保留既有工作空间和未提交内容，不建立另一份插件源码；增量编辑使用精确目标与快照保护。
- 裁决：如需解除Editor DLL锁，先确认准确工程、无PIE/无未保存资产/无活动资产任务，再尝试正常关闭空闲编辑器；不强杀、不终止他人的Cook/编译进程，完成后恢复编辑器。
- 旧GPUSceneRuntime.json中的五张截图尺寸相同且字节数完全一致，不能据此判定真实鸟群画面合格；本轮需要独立图像检查与新的帧间变化验证。

## 阶段状态

- P4：执行中。
- P5：待验证已有接入及完成蓝图落盘。
- P6：待本轮编译与运行证据。
- P7：待人工视觉确认，禁止自动标记通过。
