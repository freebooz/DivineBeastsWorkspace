# PresentationModel（表现模型）

统一链路：

Gameplay Fact（玩法事实） → Semantic（表现语义） → Typed Context（类型化上下文） → Catalog Resolution（目录解析） → Provider Channel（提供者通道） + DefinitionId（逻辑定义ID） → GamePlatformPresentation Dispatcher（平台分发） → VFX/SFX/UI/Animation/Camera Provider（对应表现提供者）。

项目Presentation只负责Semantic/Context/Catalog层。

ProviderMissing（提供者缺失）是安全降级，不影响Gameplay权威结果。Catalog无匹配时仍允许Semantic-only请求继续交给Provider；Catalog同级最高候选歧义时返回InvalidRequest（无效请求），禁止按注册/数组/Hash顺序随机选取。
