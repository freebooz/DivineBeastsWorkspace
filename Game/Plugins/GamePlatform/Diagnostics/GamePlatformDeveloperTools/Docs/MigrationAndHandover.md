# MigrationAndHandover（迁移与交接）

执行前对当前工作区搜索历史 352～360 类型，未发现现存实现，因此不能把旧资料中的“已有源码”视为当前通过证据。本轮按 UE5.8/现行三层架构重新实现。

迁移表：

| 旧类型 | 当前搜索结果 | 当前处理 | 新文件/实现 | UE5.8证据 |
| --- | --- | --- | --- | --- |
| UGamePlatformNamingValidator | 未发现 | 重构 | Validation/GamePlatformEditorValidators | 源码已接 UEditorValidatorBase；真实编译未执行 |
| UGamePlatformDependencyValidator | 未发现 | 重构 | Validation/GamePlatformStaticValidators | 静态脚本可验证；UE编译未执行 |
| UGamePlatformClientLeakValidator | 未发现 | 重构 | Validation/GamePlatformStaticValidators + ValidateCook.ps1 | 真实Cook工件未执行 |
| UGamePlatformServerLeakValidator | 未发现 | 重构 | Validation/GamePlatformStaticValidators + ValidateCook.ps1 | 真实Cook工件未执行 |
| UGamePlatformContentValidator | 未发现 | 重构 | Validation/GamePlatformEditorValidators | DataValidation运行未执行 |
| UGamePlatformReviewSubsystem | 未发现 | 重构 | Review/GamePlatformReviewTypes | 人工审查待执行 |
| UGamePlatformReviewCase | 未发现 | 重构 | Review/GamePlatformReviewTypes | 人工用例待执行 |
| UGamePlatformReviewReport | 未发现 | 重构 | Review/GamePlatformReviewTypes | 人工报告待执行 |
| UGamePlatformPerformanceTestRunner | 未发现 | 重构 | Performance/GamePlatformPerformanceTestRunner | 实际性能场景未执行 |

下一层扩展原则：MobaCommon/DivineBeasts 的 Editor 模块可以单向依赖 DeveloperTools 注册 Arena/Hero/DBA 规则；GameFoundation DeveloperTools 不能反向依赖它们。