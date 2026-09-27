# RuntimeTypeOwnership（运行时类型所有权）

DivineBeastsRuntime拥有：项目核心Catalog查询、FDivineBeastsProjectContext、ProjectContext错误枚举、ContractVersion/Compatibility摘要以及Generated Catalog私有适配。

不拥有：十二生肖Hero类型、Character Component、Ability、Combat规则、Arena Match State、World Actor、PCG、Quest、UI/VFX/SFX/Material/Character Mesh。

Generated标准C++头只在Runtime的Private Adapter或未来项目私有协议Adapter中消费，避免Public Gameplay头直接#include protobuf或跨技术DTO。
