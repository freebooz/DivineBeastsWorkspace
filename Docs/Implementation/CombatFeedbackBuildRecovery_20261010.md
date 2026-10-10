# 战斗反馈 UE5.8 构建恢复、定向验证与资产门禁

日期：2026-10-10。工作空间：`DivineBeastsWorkspace（神兽联盟工作空间）`。引擎：`F:/UnrealEngine-5.8.0-release`（UE5.8 源码引擎）。

本文仅记录实际工具返回的退出码、文件及其限制。详细架构、P0—P8状态见 `Docs/Implementation/CombatFeedbackWorkOrders_20261009.md`。除自动构建生成中间产物外，不修改引擎源码、插件身份或其它任务资源。

## 一、阻断根因核对

- `UnrealBuildTool（虚幻构建工具）` 源码 `Modes/BuildMode.cs` 明确在`-NoEngineChanges（禁止修改引擎输出）`启用时检查已有Engine产物是否需要重写；若是，则返回`FailedDueToEngineChange`，并提示通过 IDE 完整构建。编辑器在启动阶段自动编译带有该保护，不等于本项目战斗反馈源码出现C++语法错误。
- 对`DivineBeastsArenaEditor Win64 Development`（编辑器开发构建目标）执行`-Module=GamePlatformAnimation -WriteOutdatedActions=...（导出编译动作）`，结果25项动作：22项MSVC编译、3项链接，其中引擎Core的共享PCH/Unity目标占大多数。加`-UsePrecompiled`仍未消除对过期Engine Core产物的修改请求，因此不能把此参数当作有效修复。
- `-NoUBA`在此引擎`Executors/ExecutorFactory.cs`中只令`UBAExecutor`关闭Detour（进程拦截），仍保留UBA（构建加速器）调度；不能宣称它改用了独立本机LocalExecutor。

## 二、真实已完成的构建

正式命令（在当前锁定工作空间执行，展示核心选项，非另一份工程）：

```powershell
& "F:\UnrealEngine-5.8.0-release\Engine\Build\BatchFiles\Build.bat" `
  DivineBeastsArenaEditor Win64 Development `
  "-Project=E:\poject\feebooz\DivineBeastsWorkspace\Game\DivineBeastsArena.uproject" `
  "-Module=GamePlatformAnimation" -WaitMutex -NoUBA -MaxParallelActions=3 -NoHotReloadFromIDE
```

- **GamePlatformAnimation（平台动画共享模块）**：实际执行25/25项，包括引擎Core过期目标文件更新、平台模块编译及DLL链接；`UnrealBuildTool`返回`Result: Succeeded`，退出码0，编译总耗时125.57秒。`GamePlatformAnimation/Binaries/Win64/UnrealEditor-GamePlatformAnimation.dll`真实存在。
- **重要限制**：上述是一个定向模块编译，不会自动生成所有缺失的`UnrealEditor.modules（编辑器模块登记文件）`。该插件在本轮首次检查中没有模块登记文件，必须再由真正的完整编辑器装配/引擎重新加载验证，不能仅凭DLL文件将“可加载”写为通过。

## 三、真实 MSVC 单文件定向编译（共20项，全部退出0）

通过`Build.bat DivineBeastsArenaEditor ... -SingleFile=<项目源码绝对路径> -NoUBA -MaxParallelActions=1 -NoHotReloadFromIDE`，逐一执行、每项均返回`Result: Succeeded`：

1. `GamePlatformHitFeedbackProfile.cpp`（通用Profile契约及参数校验）。
2. `MobaHitFeedbackPolicy.cpp`（MOBA分类与连击强度策略）。
3. `DivineBeastsCombatFeedbackCatalog.cpp`（项目Hero/Ability映射目录）。
4. `DivineBeastsCombatUIFeedbackLibrary.cpp`（平台UI反馈请求映射）。
5. `GamePlatformLocalHitstopSubsystem.cpp`（本地视觉顿帧）。
6. `MobaPresentationClientSubsystem.cpp`（MOBA反馈表现调度）。
7. `DivineBeastsArenaCombatFeedbackClientSubsystem.cpp`（竞技客户端按技能解析与异步租约组合根）。
8. `GamePlatformHitFlashWorldSubsystem.cpp`（局部受击闪白）。
9. `GamePlatformCameraHitFeedbackSubsystem.cpp`（本地命中镜头冲击）。
10. `GamePlatformCombatFeedbackWorldSubsystem.cpp`（已确认事实World总线）。
11. `GamePlatformCombatFeedbackDedupeTests.cpp`（GUID及事件类型去重测试源码）。
12. `DivineBeastsHitFeedbackGrantPolicyTests.cpp`（OwnerOnly技能授权变化测试源码）。
13. `DivineBeastsCombatFeedbackCatalogTests.cpp`（项目技能目录校验测试源码）。
14. `DivineBeastsCombatUIFeedbackTests.cpp`（生命/护盾/治疗浮字映射测试源码）。
15. `GamePlatformLocalHitstopTuningTests.cpp`（0/3/6帧策略测试源码）。
16. `GamePlatformActionInputBuffer.cpp`（通用短时输入缓冲）。
17. `GamePlatformActionInputBufferTests.cpp`（按键边沿、序号与生命周期测试源码）。
18. `DivineBeastsInputClientSubsystem.cpp`（本地Pawn/GAS输入缓冲恢复接线）。
19. `MobaHitFeedbackPolicyTests.cpp`（MOBA命中策略与格挡/挥空测试源码）。
20. `GamePlatformCombatFeedbackNetTests.cpp`（权威网络表现事实数值边界测试源码）。


单文件编译不会链接模块DLL，不会向UE注册新的UClass，也不会执行原生自动化测试；每项均应保留“源文件编译通过”而非“模块已完成/游戏可运行”。

## 四、尚未恢复的模块链接及环境现象

- 第二组 Editor `-Module=GamePlatformPresentationCore -Module=DivineBeastsPresentationRuntime -Module=MobaPresentationRuntime`解析出87项构建动作，完成前6项ISPC向量化编译后，3个MSVC `cl.exe`进程长时间无CPU进展且无实际C++子进程；本次自有构建已停止，没有编译成功退出码。

- 为排除多人并发构建争用，另将`GamePlatformPresentationCore`单模块编辑器构建降至`-MaxParallelActions=1`，仍解析出72项包含Engine依赖的动作，首个`Module.GamePlatformData.cpp`的MSVC进程约191秒CPU仅约0.33秒且没有`c1xx.exe`实际编译子进程、日志未继续推进；该自有构建同样已主动停止，不得记为成功。此与单文件MSVC20次编译均通过形成明确对照：根阻断更接近大型Engine/Unity构建执行链，不等价于C++接口本身编译失败。
- 曾见自动Editor启动`FailedDueToEngineChange`；不能通过手工复制DLL、伪造`.modules`、手动变更引擎构建ID或重启有未保存包的他人编辑器来绕过。
- 真正运行仍依赖多个缺失客户端模块以及最新反射注册。脚本`Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py`逐一检查正式模块DLL、`UnrealEditor.modules`内容和BuildId、项目浮字Widget是否存在；首次检查10项Editor模块中5项有DLL、仅4项有一致的磁盘模块装配登记，缺失5个具体DLL，正式`WBP_DBA_UI_FloatingCombatText.uasset`尚未落盘。默认脚本只报告真实现状；`--require-ready`会让未达标门禁以非零退出码结束。
- 只有真正Editor模块完整链接、登记并加载后，才能通过Monolith生成三种Profile、技能Catalog、闪白材质、CameraShake、生肖Niagara/SFX、已绑定浮字Widget，然后编译保存与重载核验。P1/P3/P5/P7/P8资产及多人验收仍有缺口。
- 本轮源文件编译、共享模块链接和引擎自动化测试为三个不同层级的证据：已完成20个`-SingleFile`源文件编译；`GamePlatformAnimation`唯一共享模块DLL编译与链接成功；Editor完整装配、25个已有属性集/策略测试中的新增项执行、Cook以及客户端表现链仍待验证。


## 五、下一步操作与证据要求

- 以不修改引擎源码的构建工作流单独解决引擎Engine Unity/PCH长时间停滞；优先限定`-MaxParallelActions=1`复现，与`-MaxParallelActions=3`对比；记录每一项动作的CPU及调度状态，勿改写成功率。
- 获得正确Editor`.modules`及BuildId一致性后，重新启动正式工程，通过Monolith读取新UClass、先查询所有未保存的资产，再制作真实资源。
- 运行至少一项真正UE Automation，再在Client/Server目标完成最小Cook、双客户端网络事实复核及0/3/6顿帧与5v5压力数据实测。测试期间不得恢复已弃用玩法或用纯视觉推导权威伤害。

