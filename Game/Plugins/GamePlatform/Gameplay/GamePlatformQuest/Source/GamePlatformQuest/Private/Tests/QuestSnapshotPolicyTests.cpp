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
