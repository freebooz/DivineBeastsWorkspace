#include "Tests/ApplicationFlowExecutorCases.h"
#include <iostream>

// 本机入口只负责输出测试结果；同一份行为用例也在 UE Automation 中编译执行。
int main()
{
    int Failed = 0;
    const auto Cases = GamePlatform::ApplicationFlow::Tests::GetCases();
    for (const auto& Case : Cases)
    {
        GamePlatform::ApplicationFlow::Tests::FChecks Checks;
        Case.Run(Checks);
        std::cout << (Checks.Failures.empty() ? "PASS " : "FAIL ") << Case.Name << '\n';
        for (const auto& Failure : Checks.Failures) std::cerr << "  " << Failure << '\n';
        if (!Checks.Failures.empty()) ++Failed;
    }
    std::cout << "Cases=" << Cases.size() << " Failed=" << Failed << '\n';
    return Failed == 0 ? 0 : 1;
}
