# Foundation M0 验证记录（源码交付，UE目标与资产未验证）

日期：2026-09-21。唯一实际工作空间为`E:/poject/feebooz/DivineBeastsWorkspace`，正式工程`Game/DivineBeastsArena.uproject`。结论：**未达到M0基础工程可运行，不能汇总全部通过。**

## 依据及环境

用户确认按00→03补齐并保留旧流程接口，随后明确保留三个历史空插件描述原位。本轮遵守该决定，没有新建另一个宿主、移动/禁用这些插件或修改引擎关联来绕过错误。

原独立设计/任务文件和`AcceptanceChecklist.md`未找到；使用本轮用户逐项要求作为验收依据，不新造一份历史全勾选清单。现行规则和中文规划沿用当前工作树。详见`../Implementation/FoundationM0/ExecutionProgress.md`。

- UE源根：`F:/UnrealEngine-5.8.0-release`；真实`Engine/Build/Build.version`为5.8.0、CL0、CompatibleCL0、BranchName UE5。该目录不是Git仓库，未虚构引擎提交。
- 既有UE构建日志识别Visual Studio工具链14.44.35214、Windows SDK10.0.22621.0；引擎开发导入库仍不完整。此前引擎长构建已停止，没有会话结束后继续构建的承诺。
- 独立C++测试使用CMake和本机VS生成器，不等同UE锁定工具链/UHT验收。Core已记录MSVC19.38；其他套件以各自CMake生成证据为准。
- 主工程真实目标为Editor、Client、Server，均Win64/Development、V7、Unreal5_8。没有新建三服务器业务或第四个插件。

## 分项矩阵

| 要求/检查 | 状态 | 实际执行与证据 | 限定 |
| --- | --- | --- | --- |
| 主工程与三插件源码写入 | 通过 | 当前Source、描述、配置、脚本实际存在；接口见InterfaceContract | 存在不是编译；审查缺陷另记 |
| 整体描述文件有效/插件可发现 | 失败 | `Saved/Validation/FoundationM0/Structure-20260921-162647.log`，3份空描述；正式UBT日志亦失败 | 保留原位，未扩大修改 |
| 本批依赖方向和正式目标静态检查 | 通过 | Core→UE基础；Data→Core；Flow→Data/Core；主工程→三插件；当前合法描述模块身份无重复 | 静态核对，不替代构建 |
| 正式Editor构建/UHT | 失败 | 首次构建退出6，`Saved/Validation/FoundationM0/Host-Editor-First.log` | 失败在规则扫描，未到本批UHT/C++；后续未循环重跑 |
| 正式Client构建 | 未执行 | 同一描述扫描前置阻断 | 没有客户端二进制通过证据 |
| 正式Server构建 | 未执行 | 同一描述扫描前置阻断及引擎库未齐 | 共用服务器目标保留 |
| Core原生行为 | 通过 | Debug/Release各11场景，退出0；`CoreNative/Native-Debug-20260921-161009-215.log`及Release对应日志 | 不涉及UObject/UHT |
| Host生产入口/地图筛选算法 | 通过 | Debug编译/CTest退出0，11断言；`HostNative/Testing/Temporary/LastTest.log` | 无真实切图/图形验证 |
| Data生产账本算法 | 通过 | 初始12断言退出0；增加后的结果见末尾补充 | 不等同真实加载器/资源分组 |
| Flow纯执行器 | 通过 | 原有21场景Debug/Release退出0；新资产兼容扩展结果见末尾补充 | 不等同真实租约/反射集成 |
| 进程脚本负例与清理 | 通过 | 26项退出0，`ScriptTests-1f2c412a-f1de-48b1-982a-a0cb992b13e3/results.json` | 测试受控进程不是游戏进程 |
| UBT实际配置层级/数组 | 通过 | 3项退出0，`ea26b9e1-64c7-4d37-944c-d01e79ded715/PackagingConfig/result.json` | 不等同干净发行Cook剥离 |
| 资产脚本离线行为 | 通过 | 初始Maps25项；最终Maps/Probe/Flow结果见末尾补充 | 未创建假包，不证明UE生成 |
| 三插件UE自动化含负例 | 未执行 | `TestFoundationUnreal.ps1 -Execute`前置退出1：`039200d0-755c-4daf-ae97-0aa30acef007/UnrealTests/result.json` | 是脚本正确拒绝，不是测试通过 |
| 两地图/两定义真实生成 | 未执行 | 均无引擎生成文件 | 禁止文本/字节占位 |
| 引擎内重复生成保护与DataValidation | 未执行 | 依赖已编译反射及真实资产 | 离线替身不能替代 |
| 客户端干净Cook/Stage/启动Ready | 未执行 | 正式构建与四资产前置未满足 | 没有游戏进程通过 |
| 服务端干净Cook/Stage/无玩家流程 | 未执行 | 同上；源码有专用服务器分支 | 日志设计不是运行证据 |
| 缺失定义/加载中取消/重试/跨图清理 | 未执行 | 部分原生与UE测试源码已写；真实场景未运行 | 不删除用户正式资产制造负例 |
| 多PIE/退出互不影响 | 未执行 | 正式工程未启动 | 两个内存实例测试也不代替真实多PIE |
| 三维可见性/观察者操作 | 未执行 | 无图形验收 | NullRHI或日志不能记为通过 |
| 正式发行产物排除开发内容 | 未执行 | 已写NeverCook和显式CustomConfig | 需要实际发行Cook/Stage产物审计 |
| 操作范围/密钥/生产副作用 | 通过 | 未操作Backend/Shared/Deploy、未写密钥/提交/推送/部署/真实支付 | 本批限定范围检查，不是全仓安全审计 |

上表省略根前缀的证据路径全部相对于`Saved/Validation/FoundationM0/`。原始证据不覆盖历史日志。不存在需要通过“不适用”回避的本批必需UE验收。

## 独立复核和修复跟踪

首轮Core静态未发现明确缺陷。Data首轮发现零延迟Ticker同轮重排可能卡住游戏线程、身份大小写账本分裂、后续外部加载需求所有权保护不足，以及运行期测试覆盖不足；均已转实施者修复/补测。完整修复与复核结果须在后续补充确认，不能以本记录掩盖源码缺陷。已释放租约历史记录在常驻实例内线性增长是明确剩余风险，不能简单清空记录破坏幂等合同。

## 退出码和证据解释

- `VerifyFoundation.ps1`：0仅表示其全部必需项均有通过证据；1有失败；2未完成。没有证据的反射、资产、图形、多实例等保持未执行。
- Build/Cook/Run/TestFoundationUnreal：0为所选操作成功；本地脚本拒绝/校验失败为1；缺前置/未显式执行为2；外部非零码保留，即使外部返回2，报告也标失败，不误当跳过。
- 运行脚本记录PID、启动时间、命令参数和本轮RunId，仅清理自身进程；不按名称杀全部Editor，不搜索旧日志证明成功。
- 正式构建首次exit6是实际失败，不是本批三插件已编译。当前历史阻断与环境库缺失不能合并写成“全部只是环境未执行”。

## 后续补充

等待最终可执行检查与复核后追加真实结果；原始失败项不删除。
