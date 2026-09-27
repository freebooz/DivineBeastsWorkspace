# ReportsAndEvidence（报告与证据）

默认报告目录：Game/Saved/GamePlatformValidation/<RunId>/。

通用报告写入 summary.json、summary.md、issues.json。Architecture Commandlet（架构命令行）额外写 dependency-graph.json；Audit Commandlet（资产审计命令行）额外写 asset-audit.csv；真实性能场景执行成功后写 performance.json。没有对应真实执行证据时不生成伪造的性能/Cook结果文件。

报告目录经过 Normalize/Resolve 与路径逃逸检查，不写入 Source/Content。

状态只依据真实证据：静态源码、UE DataValidation、Build/Cook 工件、人工 Review 和 Performance Baseline 分开记录。未执行不能升级为通过。