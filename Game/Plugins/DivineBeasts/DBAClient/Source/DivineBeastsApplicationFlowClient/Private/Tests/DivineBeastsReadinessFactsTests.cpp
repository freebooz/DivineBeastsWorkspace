// 显式原生测试入口；UE编译此文件时不定义main，不伪装为引擎自动化测试。
#if defined(DBA_READINESS_NATIVE_TEST)
#include "Loading/DivineBeastsReadinessFacts.h"
#include <cstdlib>
#include <iostream>

using DivineBeastsLoading::ReadinessFacts;
using DivineBeastsLoading::Fact;
static void Require(bool Value, const char* Message)
{
    // 不使用assert，Release构建也必须执行全部断言。
    if (!Value) { std::cerr << Message << '\n'; std::exit(1); }
}
int main()
{
    ReadinessFacts Facts("transfer-a", "world-a", "experience-a");
    Require(!Facts.Observe("transfer-old", Fact::SessionAdmission), "旧转移不能报告准入");
    Require(!Facts.Observe("", Fact::SessionAdmission), "空身份不能报告准入");
    Require(!Facts.Observe("transfer-a", Fact::ExpectedWorld), "普通事实接口不能绕过目标核对");
    Require(!Facts.Observe("transfer-a", Fact::ExpectedExperience), "体验必须由目标核对写入");
    Require(!Facts.Observe("transfer-a", static_cast<Fact>(99)), "越界事实必须拒绝");
    Require(!Facts.ObserveWorld("transfer-a", "world-other", "experience-a"), "错误世界不能覆盖分配");
    Require(!Facts.ObserveWorld("transfer-a", "world-a", "experience-other"), "错误体验不能就绪");
    Require(!Facts.Has(Fact::ExpectedWorld), "拒绝后不能留下部分世界就绪");
    Require(!Facts.ObserveWorld("transfer-old", "world-a", "experience-a"), "旧世界回调不能写入新任务");
    Require(Facts.ObserveWorld("transfer-a", "world-a", "experience-a"), "正确目标应接受");
    Require(Facts.Observe("transfer-a", Fact::CharacterBinding), "角色绑定应接受");
    Require(Facts.Observe("transfer-a", Fact::GameplayData), "玩法数据应接受");
    Require(Facts.Observe("transfer-a", Fact::ProjectReadiness), "项目就绪应接受");
    Require(!Facts.AllReady(), "只有地图与本地事实不能替代准入");
    Require(Facts.Observe("transfer-a", Fact::SessionAdmission), "独立准入事实应接受");
    Require(Facts.AllReady(), "六个必需事实共同满足");
    Require(Facts.Observe("transfer-a", Fact::SessionAdmission), "同代次重复通知幂等");
    Facts.Invalidate();
    Require(!Facts.AllReady() && !Facts.Has(Fact::SessionAdmission), "取消必须撤销已就绪事实");
    Require(!Facts.Observe("transfer-a", Fact::SessionAdmission), "取消后不能复活");
    Require(!Facts.ObserveWorld("transfer-a", "world-a", "experience-a"), "取消后的世界回调拒绝");
    ReadinessFacts Next("transfer-b", "world-a", "experience-a");
    Require(!Next.Observe("transfer-a", Fact::GameplayData), "相同世界的新操作仍拒绝旧身份");
    Require(!Next.AllReady(), "新操作不能继承旧操作就绪");
    for (auto Invalid : {ReadinessFacts("", "world", "experience"),
                         ReadinessFacts("transfer", "", "experience"),
                         ReadinessFacts("transfer", "world", "")})
    {
        Require(!Invalid.Observe("transfer", Fact::SessionAdmission), "缺少必要身份不可开始");
        Require(!Invalid.AllReady(), "非法输入不可就绪");
    }
    std::cout << "Readiness identity, target, admission and cancellation checks passed\n";
    return 0;
}
#endif
