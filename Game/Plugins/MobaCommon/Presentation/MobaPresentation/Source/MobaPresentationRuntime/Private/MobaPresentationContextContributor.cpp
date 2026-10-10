// MOBA共享语义接口的DLL生命周期定义：Runtime拥有导出符号，Client和项目层只单向派生消费。
// 接口无状态且不持有世界/资源；使用默认构造析构符合其职责，独立定义用于保证真实跨模块链接。
#include "Events/MobaPresentationContextContributor.h"

// 原隐式构造/内联析构在无人实例化的Runtime翻译单元中未产生客户端需要的导入符号。
// 默认特殊成员保持已有类型、布局及虚析构合同；不以空业务操作冒充实现，也不改变注册与资源所有权。
IMobaPresentationContextContributor::IMobaPresentationContextContributor() = default;
IMobaPresentationContextContributor::~IMobaPresentationContextContributor() = default;
