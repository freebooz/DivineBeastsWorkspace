# SSLM 湖泊真实感 P0—P1 实施记录（2026-10-24）

工作区：DivineBeastsWorkspace（神兽联盟工作空间）；UE版本：5.8.0；开始时主分支提交 b001de488b78407983caa32c8cd4b1d8ad8f6953。

## 已确认基线

- UE原生独立审核地图：/Game/Development/Water/L_GP_WaterReview。原生读取到23个Actor，包括WaterBodyLake（湖泊）、WaterBodyRiver（河流）、WaterZone（水体渲染区域）、瀑布及4个权威WaterRegion（水域规则）。
- 平台自定义母材质：/GamePlatformSurface/Materials/Water/M_GP_Water_Master（已有吸收、散射、深浅水色、粗糙度参数）。
- 项目新手村材质MI_DBA_Village_Lake（新手村湖面材质实例）经UE原生反射确认，继承MI_GP_Lake_Native（平台原生湖面实例）和官方Water_Material_Lake（官方湖体母材质）。不可直接把平台自定义母材质当成完全兼容的官方Lake渲染替代。
- 本阶段只开发独立湖泊画质候选，不更改正式L_Village_Start、现有水体权威、地形高度、PCG、导航、碰撞和服务器Cook。

## P1工具与独立资产合同

1. Tools/Water/AuthorLakePhotorealP1UE58.py（UE原生水体审核作者）。默认inspect只读；materials创建清澈和深湖两种MaterialInstance；material_verify复查材质；map_copy仅复制审核地图；map_bind只调整复制图；map_verify独立回读；capture以真实D3D12捕获原始／清澈／深湖各三个镜头。
2. Tools/Water/RunLakePhotorealP1.ps1（Windows受控执行脚本）。固定UE5.8环境，检查项目、进程互斥、旧日志冲突、UE原生成功标记和LogPython错误。UE进程退出0而Python脚本报错，也会判定失败。
3. 拟创建清澈版 /Game/Development/Water/Materials/MI_SSLM_LakeClear_P1，深湖版 /Game/Development/Water/Materials/MI_SSLM_LakeDeep_P1，两者只继承已验证的官方兼容Water实例，不覆盖任何原始资源。
4. 拟复制地图 /Game/Development/Water/Maps/L_SSLM_LakePhotorealP1，独立进程绑定清澈版。第三个阶段读取原始、清澈、深湖三种表现的审核摄像机，生成九张1280x720真实RHI对照图，默认不保存动态对比时的地图变化。
5. 全部审查日志与未来截图归 Saved/Validation/WaterLakePhotorealP1_20261024（临时证据目录），源代码与资源不可凭历史日志自动宣称通过。

## 当前实际执行状态

- 原生资产反射已完成，Python脚本语法及默认inspect模式通过。
- 首次写入预检发现原设想的Water Normal Flatness标量不存在。实际官方Water实例提供River Normal Flatness（河流水纹法线平整度），已按真实参数名修正。
- 首次写入的 UE 进程退出0，但出现LogPython: Error，未创建任何P1材质。后续脚本新增日志错误门禁，不会把进程退出0误判为资产创建成功。
- 继续执行被合法并发保护门禁拦截：已有其他UE编辑器PID 25836（正式新手村VFX审核）运行，本任务未终止其进程，也未对同一正式项目强行启动第二个资产写入者。
- 当前未完成：两份真实MaterialInstance资产、独立地图复制与绑定、原生冷重载、真实GPU图片及像素审查、Shader性能、Client/Server Cook和人工视觉签收。禁止将作者脚本当作已生成UE效果。
- 当前不提交、不推送；完整正式关卡迁入需待独立图人工审核。

## 可重复执行指令

在没有其他UnrealEditor.exe、UnrealEditor-Cmd.exe或UnrealBuildTool.exe进程，且锁定的UE5.8工具链可用时，按以下次序分别启动：

1. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode inspect
2. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode materials -RunId P1Materials
3. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode material_verify -RunId P1MaterialVerify
4. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode map_copy -RunId P1MapCopy
5. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode map_bind -RunId P1MapBind
6. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode map_verify -RunId P1MapVerify
7. ./Tools/Water/RunLakePhotorealP1.ps1 -Mode capture -RunId P1Capture

真实截图还必须通过非黑帧、纹理细节、多版本差异与人工视觉审核；UE RHI取图不等于帧时与GPU预算验收。禁止覆盖历史证据或未经同意迁入正式新手村。
