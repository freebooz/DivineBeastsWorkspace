# SSLM蓝图与角色材质复用优化记录

## 范围和执行计划

1. 复核此前GlooPrint原生扫描，优先消除实际重复；不因节点数少或已有函数仍机械提取蓝图。
2. 读取8份角色材质的实际节点、输入、参数、PBR连线及设置，逐项核对与既有消散公式一致。有人工修改或不同算法时停止覆盖。
3. 将重复的消散进度、原透明度保留、原发光叠加提取为3个共享材质函数，放在现有项目公共表现内容包Character目录。算法、现有公共噪声函数、GP参数接口保持不变。
4. 逐材质备份后替换24处内联节点，保留节点位置，断言全部连线、参数默认值、材质输出及设置等价，再原生编译保存。
5. 原生重新加载验证共享函数签名/代码、8份材质及其完整合同，记录实际编译结果；源级作者入口同步使用共享函数，避免新增角色再次复制算法。

## 分层与收益

消散生命周期、计时参数设置仍由已有平台机制负责。本次仅整理项目角色材质的表现适配，沿用已经存在的项目内容包依赖，不新建播放器、框架、插件或公开运行API。

函数提取改善维护性，统一未来修改入口。GPU执行仍是等价内联计算，不能据此宣称帧率或GPU耗时提高。未执行打包运行和人工视觉验收时，不能替代该证据。

## 资源清单

- `/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_CharacterDissolveProgress`：Enabled、Now、Start、Duration、Preview → Result。
- `/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_CharacterDissolveMask`：Old、Progress、Mask → Result。
- `/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_CharacterDissolveEmission`：Old、Edge、Strength、Progress → Result。
- 既有`MF_DBA_CharacterDissolve`的NoiseMask、DissolveProgress、EdgeWidth、EdgeColor及Mask/Emissive合同保持不变。

## 当前状态

2026-10-27已通过正式UE5.8/Monolith执行，PID61536，实际工程`Game/DivineBeastsArena.uproject`。本次实改集中于材质复用，蓝图玩法、服务器逻辑及UI行为保持原状。

- 原生读取8份材质，24段Custom公式逐字规范化比对原实现通过；未覆盖不同算法。
- 3份真实MaterialFunction已创建、保存，8份原材质已替换并保存；原始文件保存在证据目录`Backup/`，未发布第二套角色材质。
- 8份材质完整节点连线、公开根输入、参数默认值、混合模式、着色模型、双面及裁剪设置等价断言通过。
- 8/8材质真实Shader编译完成，编译错误为0。前后全部8项Shader静态统计完全一致；没有GPU耗时或FPS提升声明。
- 11个包通过`EditorLoadingAndSavingUtils.reload_packages`真实从磁盘重载，再次核对函数代码、输入顺序/类型、输出连接、材质合同及保存哈希，全部通过。
- 补充独立UE5.8冷进程PID58748验证通过：3函数及8材质实际加载，函数代码/类型、连线/参数合同及文件哈希匹配。首次冷读被未就绪AssetRegistry的`does_asset_exist`前置判断误拦；已改为直接原生加载，并由发布任务同步扫描目录后重跑，保留原始失败记录，没有补建或改写资产。
- 编辑器已交回原掌心审核地图，地图及内容脏包均为空，后续GAS/Cook由已协调的发布任务串行继续。
- 作者脚本也改为调用同一共享函数，新接入角色不再复制这三段算法。

## Shader静态计数

以下各项优化前后完全一致，不等同于逐帧GPU测量。

| 材质 | 像素Shader指令数 | 采样器数 |
| --- | ---: | ---: |
| PrototypeCharacterColor | 357 | 4 |
| Mannequin | 381 | 7 |
| Arissa | 369 | 7 |
| kachujin_MAT | 369 | 7 |
| kachujin_MAT_ | 383 | 3 |
| Wolf | 359 | 5 |
| Vampire_MAT1 | 374 | 8 |
| Vampire_MAT_Transparent | 374 | 8 |

## 蓝图审查边界

复用刚完成的GlooPrint原生审查结果：591个蓝图、60张地图，失败0；审查记录中最大图18个节点。该记录证明图结构和编译状态，不能证明已完成全部蓝图运行性能分析。本次没有发现足以支持改写这些小图行为的证据，实际可合并项为8份材质中的重复适配计算。

## 证据和未完成项

工作空间`Saved/Validation/BlueprintMaterialOptimization20261027/`：

- `NativeBefore.json`、`NativeAfter.json`、`NativeReloaded.json`：真实引擎节点、参数及连接快照。
- `Material_0.json`至`Material_7.json`：前后完整合同、备份路径、Shader计数及文件哈希。
- `CompilationStats.json`：8份材质真实编译状态及错误检查。
- `ReloadVerification.json`：11包磁盘重载后的合同复核。
- `ColdVerification.json`、`NativeCold.json`：独立UE进程加载验证结果和完整快照。原始失败及重跑日志位于相邻`SingleImplementationConvergence20261027/CharacterCold*.log`。
- `EditorHandoff.json`：已交还的PID、地图、11份完整资产清单及脏包状态。
- `MaterialsAfter.png`：真实UE材质球预览，不能替代角色运行截图或人工验收。
- `MaterialsBefore.png`：首次抓图发生于预览Shader/环境未就绪时，显示占位材质，**本图无效，禁止用于视觉前后对比或等价结论**。

本次没有执行打包客户端、多人死亡消散运行验收、逐帧GPU计时或人工视觉签收。算法和图合同等价、原生编译和重载通过，不扩展为上述未完成阶段已经通过。
