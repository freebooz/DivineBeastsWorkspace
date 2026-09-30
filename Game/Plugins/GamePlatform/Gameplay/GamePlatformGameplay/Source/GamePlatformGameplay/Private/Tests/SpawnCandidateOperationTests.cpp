#if defined(GAMEPLAY_NATIVE_TESTS)
// 以有偏移的候选验证最后生成入口使用已校验坐标，不使用来源Actor的另一个位置。
#include "../Policies/SpawnCandidateOperation.h"
#include <cstdlib>
#include <iostream>
struct FSourceFixture { double GetActorTransform() const { return 10; } };
struct FWeakSourceFixture { const FSourceFixture* Get() const { return &Value; } FSourceFixture Value; };
struct FCandidateFixture { FWeakSourceFixture Source; double Transform = 110; };
int main()
{
    const FCandidateFixture Candidate;
    double SpawnedAt = 0;
    GamePlatformGameplay::Policy::RestartAtValidatedTransform(Candidate, [&](double Location) { SpawnedAt = Location; });
    if (SpawnedAt != Candidate.Transform) { std::cerr << "spawn must use validated candidate transform, including custom offset\n"; return 1; }
    return 0;
}
#endif
