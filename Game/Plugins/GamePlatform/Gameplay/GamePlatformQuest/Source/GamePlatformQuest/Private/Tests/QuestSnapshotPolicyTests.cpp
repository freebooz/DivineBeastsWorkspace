#if defined(GAMEPLATFORM_QUEST_NATIVE_TEST) && GAMEPLATFORM_QUEST_NATIVE_TEST
// 对账容量和显示顺序的行为回归；不把此原生用例当成持久化适配器联调。
#include "../../Public/Types/GamePlatformQuestSnapshotRules.h"
#include <cstdlib>
#include <iostream>
using namespace GamePlatformQuestSnapshotPolicy;
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    // 同一事件同时命中Q1/Q2时，重放所有权只允许精确任务及实例；已提交Q1和新实例均不能被重放。
    Require(!MatchesReplayTarget(1, 101, 2, 202), "committed Q1 must not replay pending Q2 event");
    Require(!MatchesReplayTarget(2, 203, 2, 202), "old quest instance must not mutate new instance");
    Require(MatchesReplayTarget(2, 202, 2, 202), "pending Q2 must retain exact replay ownership");
    Require(IsProgressValid(1.0, 10.0, false), "valid partial objective must load");
    Require(!IsProgressValid(11.0, 10.0, true), "out-of-range progress must fail validation");
    Require(!IsProgressValid(10.0, 10.0, false), "completion flag must agree with progress");
    Require(!CanTransferReplay(16, 1, 16), "full replay queue must retain original accepted event");
    Require(CanTransferReplay(16, 0, 16), "duplicate replay consumes no additional capacity");
    Require(CanTransferReplay(15, 1, 16), "exact remaining capacity accepts transaction");
    Require(!CanTransferReplay(15, 2, 16), "partial replay admission must not commit");
    Require(ShouldAccept(5, 8, false, 5, 7), "same Revision newer progress must reach view");
    Require(!ShouldAccept(5, 7, true, 5, 8), "late same Revision state must not overwrite newer sequence");
    Require(!ShouldAccept(4, 100, true, 5, 8), "old persistence Revision must not overwrite");
    Require(ShouldAccept(6, 0, false, 5, 8), "legacy higher Revision must remain compatible");
}
#endif
