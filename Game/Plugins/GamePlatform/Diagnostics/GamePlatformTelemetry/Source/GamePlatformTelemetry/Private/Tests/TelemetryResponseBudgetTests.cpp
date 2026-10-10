// 平台遥测私有响应预算回归：验证分块、精确边界、负值及整数溢出；测试无网络和用户数据副作用。
#if defined(GAMEPLATFORM_TELEMETRY_NATIVE_TEST)
#include "Transport/TelemetryResponseBudget.h"
#include <limits>
#include <cstdlib>
int main()
{
    using GamePlatform::Telemetry::CanReceiveBytes;
    if (!CanReceiveBytes(0, 1024, 1024)) return EXIT_FAILURE;
    if (!CanReceiveBytes(1000, 24, 1024)) return EXIT_FAILURE;
    if (CanReceiveBytes(1000, 25, 1024)) return EXIT_FAILURE;
    if (CanReceiveBytes(-1, 1, 1024) || CanReceiveBytes(0, -1, 1024)) return EXIT_FAILURE;
    if (CanReceiveBytes(1025, 0, 1024) || CanReceiveBytes(0, 1, -1)) return EXIT_FAILURE;
    if (CanReceiveBytes(1, std::numeric_limits<long long>::max(), 1024)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
#endif
