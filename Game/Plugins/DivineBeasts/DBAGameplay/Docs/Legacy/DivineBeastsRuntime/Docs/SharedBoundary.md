# SharedBoundary（共享契约边界）

跨UE/Go的项目身份唯一真源位于Shared/Contracts/Games/DivineBeasts。

新增核心源包括Project Catalog（项目目录）、ServerRole/Experience/ArenaMode Schema、ProjectContext Schema、contract-version、compatibility-matrix、无RPC Service的divine-beasts-context.proto，以及不声明HTTP path的OpenAPI Catalog Components。

Generated C++与Generated Go由同一源生成。Runtime不手写第二份完整Role/Experience/ArenaMode表。

Generated Contract DTO不能进入通用Gameplay公共头。标准C++生成头只由Runtime Private Adapter或未来Application/Online/Server私有Adapter消费。
