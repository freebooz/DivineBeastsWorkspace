# GamePlatformWeather 检查与验收台账

更新时间：2026-10-10。此文件将源码实现、自动化测试、UE原生构建、真实资产、实际世界与联机验证明确分开；不得仅凭源码出现就宣布运行验收。

| 事项 | 测试或证据 | 当前记录 |
| --- | --- | --- |
| 状态合法性、量化、插值、调度数据边界 | `GamePlatform.Weather.Runtime.QuantizationAndTransition` | 测试源码已提供，UE Automation执行待验证 |
| 插件身份、目录、模块、目标 | `Tests/Architecture/DesignBaselineAudit.psm1` | 已更新为47插件基线，待实际运行 |
| UE5.8 Editor/Client/Server | 正式锁定Build.bat输出及目标退出码 | 待执行并回填 |
| 服务器权威与客户端禁写 | Dedicated Server + 客户端测试 | 待验证 |
| 两客户端一致、迟加入、重连 | Village真实服务端网络试验 | 待验证 |
| 真实MPC及雨雪材质 | UE Editor/Commandlet、Material Editor、Shader | 未交付 |
| 真实Niagara与声音 | UE编辑器资产创建、保存、回读 | 未交付 |
| 新手村室内/室外视觉 | 人工UE游戏运行截图及记录 | 待验证 |
| 客户端/服务器Cook差异 | UAT干净Cook、包资产清单 | 待验证 |
| CPU/GPU/内存/网络开销 | Insights、ProfileGPU、stat与网络剖析 | 待验证 |

错误重试、世界销毁、重入、旧快照、资产缺失和自动调度取消均应覆盖回归。Weather客户端不得在Dedicated Server构建出现。真实结果以本轮执行生成的构建日志和系统工具观测为准。
