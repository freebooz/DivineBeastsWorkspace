// 平台服务器准入接收预算回归：直接验证生产使用的增量预算策略，不发网络请求。
#include "Server/AdmissionResponseBudget.h"
#if defined(GAMEPLATFORM_ADMISSION_NATIVE_TEST)
#include <cassert>
#include <cstdint>
#include <iostream>
int main()
{
    using GamePlatform::Server::CanAcceptAdmissionResponseBytes;
    // 32KiB来自已有准入契约；精确边界可接收，后一字节、负值与整数溢出均拒绝。
    assert(CanAcceptAdmissionResponseBytes(0, 32768, 32768));
    assert(CanAcceptAdmissionResponseBytes(32767, 1, 32768));
    assert(!CanAcceptAdmissionResponseBytes(32768, 1, 32768));
    assert(!CanAcceptAdmissionResponseBytes(0, 32769, 32768));
    assert(!CanAcceptAdmissionResponseBytes(-1, 1, 32768));
    assert(!CanAcceptAdmissionResponseBytes(0, 0, 32768));
    assert(!CanAcceptAdmissionResponseBytes(INT64_MAX - 1, 2, INT64_MAX));
    std::cout << "admission response budget: 7 assertions passed\n";
}
#endif
