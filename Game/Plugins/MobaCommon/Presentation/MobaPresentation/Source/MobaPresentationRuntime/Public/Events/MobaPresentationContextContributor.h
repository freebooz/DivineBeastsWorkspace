// MOBA层双端中立扩展契约：由客户端适配和第三层贡献者实现；不持有用户/世界/资产或播放实例。
// C++公开生命周期符号归MobaPresentationRuntime；调用方负责共享对象持有及注册/注销，不反向链接项目层。
#pragma once

#include "Types/MobaPresentationTypes.h"

/** MOBA表现上下文贡献接口。上层通过低层公开契约扩展中立上下文，不把项目资源或玩法身份写入本层代码。 */
class MOBAPRESENTATIONRUNTIME_API IMobaPresentationContextContributor
{
public:
    /** 建立无状态接口基部；导出定义在本Runtime模块中，跨DLL派生不依赖某个调用点偶然实例化隐式构造。 */
    IMobaPresentationContextContributor();

    /** 通过基类释放具体贡献者；派生者自行释放所有资源。本接口不注销外部服务，也不访问世界或UObject。 */
    virtual ~IMobaPresentationContextContributor();

    /** 游戏线程同步补充本次上下文；InOutContext只在当前调用期间可改，不保存引用或启动播放/网络/权威动作。
     * 调用者在返回前持有贡献者强引用并重验原作用域；实现可同步注销或触发重绑，不能把旧调用的完成当新世界资格。
     * 无返回值；未提供可选字段表示保持原上下文，取消/失效由调用方在边界检查中拒绝剩余处理。 */
    virtual void Contribute(FMobaPresentationContext& InOutContext) const = 0;
};
