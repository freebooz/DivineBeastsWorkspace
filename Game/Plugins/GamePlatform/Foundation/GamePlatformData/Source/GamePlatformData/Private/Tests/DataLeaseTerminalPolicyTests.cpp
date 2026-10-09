// 职责：在无UE宿主的原生测试中验证数据租约终态判定；属于平台数据层，只消费生产策略。
// 预期：认证失败/未来代次拒绝，真实已签发且不在活跃表中的旧代次可无限重复释放；不维护终身历史。
#if defined(GAMEPLATFORM_DATA_NATIVE_TERMINAL_TEST)
#include "Ownership/DataLeaseTerminalPolicy.h"
#include <iostream>
#include <cstdint>
int main()
{
    int failures = 0;
    const auto check = [&failures](bool value, const char* message) { if (!value) { ++failures; std::cerr << message << '\n'; } };
    using GamePlatform::Data::CanTreatMissingLeaseAsReleased;
    check(!CanTreatMissingLeaseAsReleased(false, 1, 100), "ForgedProofRejected");
    check(!CanTreatMissingLeaseAsReleased(true, 0, 100), "ZeroGenerationRejected");
    check(!CanTreatMissingLeaseAsReleased(true, 101, 100), "FutureGenerationRejected");
    check(CanTreatMissingLeaseAsReleased(true, 1, 100), "OldAuthenticLeaseRemainsIdempotent");
    check(CanTreatMissingLeaseAsReleased(true, 100, 100), "LatestReleasedLeaseIsValid");
    for (std::uint64_t generation = 1; generation <= 100000; ++generation)
        check(CanTreatMissingLeaseAsReleased(true, generation, 100000), "LongLivedScopeNeedsNoHistory");
    std::cout << "Cases=100005 Failed=" << failures << '\n';
    return failures ? 1 : 0;
}
#endif
