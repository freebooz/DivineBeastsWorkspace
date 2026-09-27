#pragma once
#include <array>
#include <string>
#include <utility>

namespace DivineBeastsLoading
{
/** 仅记录项目加载的六项事实，不执行流程、不分配服务器，也不产生准入事实。 */
enum class Fact : unsigned char
{ SessionAdmission, ExpectedWorld, ExpectedExperience, CharacterBinding, GameplayData, ProjectReadiness, Count };

/** 游戏线程私有值对象。观察身份在异步工作开始时捕获；Invalidate后不可复活。 */
class ReadinessFacts final
{
public:
    ReadinessFacts(std::string InObservationId, std::string InWorldId, std::string InExperienceId)
        : ObservationId(std::move(InObservationId)), WorldId(std::move(InWorldId)), ExperienceId(std::move(InExperienceId)),
          Active(!ObservationId.empty() && !WorldId.empty() && !ExperienceId.empty()) {}

    /** 调用方仍须核验实际UWorld。两个目标原子写入，不能用观察值修改冻结分配。 */
    bool ObserveWorld(const std::string& Id, const std::string& World, const std::string& Experience)
    {
        if (!Accepts(Id) || World != WorldId || Experience != ExperienceId) { return false; }
        Ready[static_cast<size_t>(Fact::ExpectedWorld)] = true;
        Ready[static_cast<size_t>(Fact::ExpectedExperience)] = true;
        return true;
    }

    /** SessionAdmission仅允许内部会话订阅提供；外部Notify不暴露此通道。 */
    bool Observe(const std::string& Id, Fact Value)
    {
        if (!Accepts(Id) || Value >= Fact::Count || Value == Fact::ExpectedWorld || Value == Fact::ExpectedExperience) { return false; }
        Ready[static_cast<size_t>(Value)] = true;
        return true;
    }
    bool Accepts(const std::string& Id) const { return Active && Id == ObservationId; }
    bool Has(Fact Value) const { return Active && Value < Fact::Count && Ready[static_cast<size_t>(Value)]; }
    bool AllReady() const
    {
        if (!Active) { return false; }
        for (bool Value : Ready) { if (!Value) { return false; } }
        return true;
    }
    void Invalidate() { Active = false; Ready.fill(false); }
private:
    const std::string ObservationId, WorldId, ExperienceId;
    bool Active;
    std::array<bool, static_cast<size_t>(Fact::Count)> Ready{};
};
}
