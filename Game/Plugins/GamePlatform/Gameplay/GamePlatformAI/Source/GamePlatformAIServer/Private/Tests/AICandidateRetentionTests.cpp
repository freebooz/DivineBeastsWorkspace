// 纯算法回归：过量候选一次有界选择，保留最新K项；比较次数检查只验证本输入，不冒充UE性能实测。
#if defined(GAMEPLATFORM_AI_NATIVE_TEST) && GAMEPLATFORM_AI_NATIVE_TEST
#include "../Perception/AICandidateRetention.h"
#include <cstdlib>
#include <iostream>
#include <vector>
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    std::vector<int> Values; for (int Index = 0; Index < 10000; ++Index) { Values.push_back(Index); }
    std::size_t Comparisons = 0;
    auto Removed = GamePlatformAICandidateRetention::SelectRetained(Values.begin(), Values.end(), 16,
        [&Comparisons](int A, int B) { ++Comparisons; return A > B; });
    Require(Removed - Values.begin() == 16, "exact capacity retained");
    for (auto It = Values.begin(); It != Removed; ++It) { Require(*It >= 9984, "newest candidates retained"); }
    Require(Comparisons < 10000 * 20, "single bounded selection must not perform repeated whole-set scans");
    Require(GamePlatformAICandidateRetention::SelectRetained(Values.begin(), Values.end(), 10001,
        [](int A, int B) { return A > B; }) == Values.end(), "below capacity keeps every candidate");
    Require(GamePlatformAICandidateRetention::SelectRetained(Values.begin(), Values.end(), 0,
        [](int A, int B) { return A > B; }) == Values.begin(), "zero capacity retains none");
    std::cout << "10000 candidates, 16 retained, " << Comparisons << " comparisons\n";
}
#endif
