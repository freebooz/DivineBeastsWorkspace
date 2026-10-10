#pragma once
// 受保护生成作用域内的最后一步。调用方已校验来源、区域、占位与候选；只把选定变换交给UE生成入口。
namespace GamePlatformGameplay::Policy
{
template<class TCandidate, class TRestart>
void RestartAtValidatedTransform(const TCandidate& Candidate, TRestart&& Restart)
{ Restart(Candidate.Transform); }
}
