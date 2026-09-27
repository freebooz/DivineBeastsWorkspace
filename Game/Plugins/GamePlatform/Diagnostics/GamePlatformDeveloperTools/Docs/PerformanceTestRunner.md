# PerformanceTestRunner（性能测试运行器）

UGamePlatformPerformanceTestRunner 管理预定义 Profile：ScenarioId、Map、ServerRole、PlayerCount、BotCount、Duration、Warmup、Metrics。

Baseline 保存 BuildVersion、ContentRevision、EngineVersion、HardwareProfile、ScenarioId、P50/P95/P99、Samples。

不同 Scenario、HardwareProfile 或 EngineVersion 不直接判性能回归；Samples 缺失也不比较。

平台层通过 FGamePlatformPerformanceExecutorRegistry（性能场景执行器注册表）接收 MobaCommon/DivineBeasts Editor 扩展提供的真实场景执行器；UGamePlatformPerformanceTestRunner 负责参数校验、样本收集、P50/P95/P99 计算、performance.json 导出和兼容基线判断。平台层不写死地图加载、Bot或竞技业务。

当前 Runner 未提供真实 UE5.8 运行环境，也没有项目层性能执行器注册，因此真实性能场景与10k资产基准状态必须保持“未执行”。