# ContractsAndAdapters（契约与适配）

项目角色和世界入口语义定义在 Shared/Contracts/Games/DivineBeasts/OpenAPI/characters.openapi.yaml 与 world-entry.openapi.yaml。

Shared Catalog 还定义 OnboardingState、CharacterStatus、WorldEntryExperienceIDs 和角色名长度；Build/Contracts 负责确定性 C++/Go 生成与 clean regenerate。

UE Generated/External Contracts 只在 Private Adapter 使用，公共 Flow API 不暴露跨技术DTO。
