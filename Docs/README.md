# 《神兽联盟》工作空间文档

更新日期：2026-09-21。以下分别列出现行规范、实现文档与历史来源；不能用目录规划或聊天中的完成描述替代真实验证结果。

## 现行规则与规划

- [全局工程规则](../AGENTS.md)：命名、中文注释、分层、权限及验证规则。
- [解决方案总体规划](Architecture/解决方案总体规划.md)：产品基线、三角色服务器、三层插件和分期交付。
- [解决方案总体目录规划说明](Architecture/解决方案总体目录规划说明_V1.3.0.md)：正式路径、中文职责和目录维护要求。
- [插件开发规范](../Game/Plugins/插件开发规范.md)：UE 插件依赖、生命周期和交付门禁。

## 历史决策与后续插件实施

- [神兽联盟历史对话与插件工程实现参考](Architecture/神兽联盟历史对话与插件工程实现参考.md)：历史方案演变、现行采用方式、ApplicationFlow 合同和后续插件职责。
- [神兽联盟历史对话来源索引](References/神兽联盟历史对话来源索引.md)：本轮读取七个会话、四十八轮消息的来源定位；不声称无遗漏导出全部历史。

## 当前实现与验证

- [GamePlatformApplicationFlow](../Game/Plugins/GameFoundation/Application/GamePlatformApplicationFlow/README.md)：流程执行机制、节点注入、接口示例及原生／UE 验证状态。
- [GamePlatformVFX](../Game/Plugins/GameFoundation/Presentation/GamePlatformVFX/README.md)：已有特效源码交付与待真实工程接入事项。
- [业务后端与共享代码工程化审查报告](Production/业务后端与共享代码工程化审查报告_V1.3.0.md)：后端及协议的审查证据与阻断项。
- [业务后端本地部署说明](Production/业务后端本地部署说明.md)：本地部署与验证说明。
- [文档变更记录](CHANGELOG.md)：本入口启用后的变更记录。

后续新增插件，须同步维护总体目录规划、插件细化目录、接口与验证文档，并在历史参考中记录明确的新决定及其替代关系。规范正文使用中文名称；README、CHANGELOG、AGENTS 等固定入口遵循现有工具约定。

## 原有目录索引

本目录是工作空间唯一的正式文档入口；根目录仅保留工具规则和工程配置。

| 文档 | 说明 |
| --- | --- |
| `Architecture/解决方案总体规划.md` | 三层架构、整合决定、运行流程与分期交付规划。 |
| `Architecture/解决方案总体目录规划说明_V1.3.0.md` | 当前目录、职责与维护规则。 |
| `Architecture/SolutionDirectoryTree_CN_V1.1.0.md` | 历史目录规划索引，仅用于版本追溯。 |
| `Production/业务后端本地部署说明.md` | 后端五服务的 Docker 本地开发部署、验证、排障与停止说明。 |
