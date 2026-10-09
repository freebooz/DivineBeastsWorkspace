#pragma once
#include <cstdint>

// 职责：平台数据层无状态终态判定；只接受门面已经校验的签发证明，不处理网络授权。
// 调用方先排除仍在活跃表的完整句柄，再以当前作用域最高签发代次判断已释放状态。
namespace GamePlatform::Data
{
/** 真实签发且不在活跃表的租约保持幂等；伪造/零/未来代次失败，不积累终身释放历史。 */
inline bool CanTreatMissingLeaseAsReleased(bool bIssuerProofMatches, std::uint64_t generation, std::uint64_t lastIssuedGeneration)
{
    return bIssuerProofMatches && generation > 0 && generation <= lastIssuedGeneration;
}
}
