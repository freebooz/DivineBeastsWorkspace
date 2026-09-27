# GamePlatformLoading 本轮验证记录

日期：2026-09-21。任务为第六插件及必要项目接线，未进入下一插件。正式工程仍是`Game/DivineBeastsArena.uproject`。**未达到完整交付验收；不能发布生产。**

## 2026-09-27 最新复核增量

当前正式工程仍为 `Game/DivineBeastsArena.uproject`。本节是最新状态；下方2026-09-21失败表保留为历史证据，不再代表当前模块编译状态。

| 项目 | 2026-09-27状态 | 最新证据与边界 |
| --- | --- | --- |
| Loading生产算法Debug | 通过 | `Saved/Validation/GamePlatformLoading/NativeCurrent`；1/1 CTest，46条断言 |
| Loading生产算法Release | 通过 | 同目录；1/1 CTest，46条断言 |
| Editor模块构建 | 通过 | UE5.8 `DivineBeastsArenaEditor -Module=GamePlatformLoading`，退出0 |
| Client模块构建 | 通过 | UE5.8 `DivineBeastsArenaClient -Module=GamePlatformLoading`，退出0 |
| Server模块构建 | 通过 | UE5.8 `DivineBeastsArenaServer -Module=GamePlatformLoading`，退出0 |
| 按需Ticker/性能诊断 | 代码与原生策略通过 | Idle/Active/Retained调度决策有原生断言；UE实际20Hz/2Hz和诊断数值仍待Automation/运行压测 |
| UE服务自动化 | 未执行 | 测试源码已编译；锁定源码引擎当前没有可启动 `UnrealEditor.exe` / `UnrealEditor-Cmd.exe` |
| 真实Data租约/世界 | 未执行 | 当前工程真实 `.uasset/.umap` 为0，不伪造Foundation资产 |
| Session真实准入 | 未执行 | GamePlatformSession公开准入服务仍是前置；Loading不模拟成功 |
| Client/Server Cook/Stage | 未执行 | 模块编译不等于资源剥离和产物审计 |
| 人工审查 | 未执行 | ManualReview继续保留人工签名栏 |

性能方面已改为：Idle不注册Ticker；Running以50ms/20Hz采样；Ready且仍持有资源时以500ms/2Hz检查弱Owner；释放后停表。任务图最多256任务、单任务64直接依赖；订阅/自定义工厂各128上限。上述数值是安全和调度边界，不是最终3A性能预算。

## 2026-09-21 历史范围与前置

新增唯一插件`Game/Plugins/GamePlatform/Application/GamePlatformLoading`，单Runtime模块；实现任务DAG、正权重单调进度、必需/可选/实际回退屏障、操作/尝试代次、取消和超时、私有实例服务、Data独占租约、目标世界声明。主工程Ready工厂接入新的`DBALoadingFlowNode`，使用公开Flow事件和Loading句柄。

没有新增业务后端接口，没有复制Data资产管理器，没有HTTP、Travel或会话替身。Session公开服务及真实准入未完成；FoundationSessionLoading前置阻塞。Online和Foundation有并行修改，未覆盖，也没有把它们的实现状态当作本轮运行证据。

UE实际版本5.8.0、CL0、CompatibleCL0、Branch UE5。原生测试本轮实际使用CMake4.3.2、Visual Studio18 2026生成器、MSVC19.51.36248.0；这不是UE构建工具链验收。工作树起始复核HEAD为695d7cf、main分支；本轮没有自行提交、推送、部署或变更引擎版本。

## 2026-09-21 历史分项结果

证据路径均相对仓库根，保存在忽略的`Saved/Validation/`，不伪造下载包或截图。

| 项目 | 状态 | 真实证据与边界 |
| --- | --- | --- |
| Loading生产算法Debug | 通过 | `Saved/Validation/GamePlatformLoading/eae0ffc0-d541-40a5-bd0a-8933e54ff0eb/Test-Debug/stdout.log`：1个CTest集合、39断言、退出0 |
| Loading生产算法Release | 通过 | 同RunId的`Test-Release/stdout.log`：39断言、退出0 |
| Core/Data/Flow/Host前置原生回归 | 通过 | `Saved/Validation/GamePlatformLoading/Regression-4a3985fe-9e95-49a4-8831-bfbdde05bbe0`，四份`*-test.log`；Core11、Data20、Flow31场景，Host集合通过；非UE测试 |
| 正式Editor构建 | 失败 | `eae0ffc0-d541-40a5-bd0a-8933e54ff0eb/UBT-Editor.log`，退出6 |
| 正式Client构建 | 失败 | `4d0ea861-d02c-45e6-845f-24fbb49c45a1/UBT-Client.log`，退出6 |
| 正式Server构建 | 失败 | `c67ecc56-bf88-4337-aa0e-ae3b497b678d/UBT-Server.log`，退出6 |
| Loading UHT/UE C++编译与链接 | 未执行 | 三目标都在旧插件描述扫描失败，未到本批源码编译 |
| UE服务生命周期/工厂撤销自动化 | 未执行 | `GamePlatform.Loading.Service.Lifecycle`已写源码，缺可运行编译 |
| UE真实Data保留与释放自动化 | 未执行 | `GamePlatform.Loading.Data.RealLeases`已写源码；两定义实际资产尚未生成 |
| FoundationLoadingOnly真实运行 | 未执行 | 无真实Sandbox/两定义及构建证据；主工程接线不等于运行 |
| FoundationSessionLoading | 未执行 | Session公开快照与真实准入前置缺失，不以假任务替代 |
| 双PIE退出互不影响 | 未执行 | 原生独立对象测试不是双PIE |
| Client/Server干净Cook、Stage与产物审计 | 未执行 | 构建、真实资产前置未满足 |
| Server源码无UI/客户端Session依赖 | 通过 | Loading.uplugin/Build.cs静态依赖清单；不证明剥离产物 |
| 人工运行、三维可见性与签审 | 未执行 | ManualReview保留空签名，无人工批准 |

后三个UBT日志目录前缀均为`Saved/Validation/GamePlatformLoading/`。共同原因是既有描述只有空白内容、JSON解析无Token：Editor首报`Game/Plugins/DivineBeasts/Presentation/DivineBeastsPresentation/DivineBeastsPresentation.uplugin`（现场Length=1），Client/Server首报`MobaPresentation.uplugin`。本轮未删除、移动或禁用它们。不能把失败改写为“Loading已编译”。

## 验证入口与限制

- `Build/Validation/VerifyLoading.ps1`支持Windows PowerShell5.1，显式选择原生与目标，独占日志、限定进程超时、传播外部失败。未有完整验收时返回2而非0。
- `Tests/Integration/Loading/TestLoadingFoundation.ps1`复用现有PowerShell7进程与Automation报告工具，核对本轮两个精确测试身份；本轮未执行，现场无pwsh。
- 首次原生测试先因尚未实现LoadingPolicy头文件编译失败，补入真实内核后Debug/Release通过。首次新脚本在PS5因UTF-8无BOM解析失败，补BOM后真实运行；这些不是UE失败的原因。
- 当前每实例单个未释放操作；Ready保留租约，必须Release。世界事实失效后的历史Ready快照不可当作当前可玩，调用方需查询IsReadyToPlay。

完整十二份材料位于插件README及Docs。25项需求追溯见`Docs/TestingAndEvidence.md`；实施断点见仓库`Docs/Implementation/GamePlatformLoading/ExecutionProgress.md`。后续先修复工程前置再跑UE测试与真实场景，不继续扩建World/UI或声称完整游戏完成。

## 独立复核与用户任务切换

独立只读复核复现了原生测试条件宏在UE `/we4668` 下的编译错误，并指出冻结规格的定义类GC保活缺口、项目世界声明后Pawn可能失效的成功窗口。已分别改为存在性宏判断、操作期TStrongObjectPtr类持有、Flow提交前再次核验项目屏障；UE测试补了配置类GC保活断言，尚未运行。修正后的宏语法检查退出0。

最终再次执行原生Debug/Release，仍为39断言通过；证据`Saved/Validation/GamePlatformLoading/de1cc91d-a3a9-4be2-a8d5-59827337a658`，脚本返回2准确表示完整验收未完成。随后用户明确切换到第八PCG任务，Loading保留上述可续作状态，未宣称完成UE验收。
