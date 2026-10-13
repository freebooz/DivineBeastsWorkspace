package gateway

// 可信背包授权与事件幂等结果必须在内部接口返回稳定状态码，
// 不把拒绝访问伪装成服务宕机，也不泄漏SQL或服务器凭据。
import "testing"

func TestTrustedInventoryErrorStatusMapping(t *testing.T) {
    cases := []struct {
        code ServiceError
        status int
    }{
        {"INVENTORY_TRUST_DENIED", 403},
        {"INVENTORY_GRANT_NOT_ALLOWED", 403},
        {"INVENTORY_CONSUME_NOT_ALLOWED", 403},
        {"INVENTORY_EVENT_ID_CONFLICT", 409},
        {"INVENTORY_FULL", 409},
        {"INVENTORY_REVISION_CONFLICT", 409},
        {"UNKNOWN_TRUSTED_INTERNAL_DETAILS", 503},
    }
    for _, test := range cases {
        status, code := OnlineErrorStatus(test.code)
        if status != test.status {
            t.Fatalf("错误码状态不符: code=%s expected=%d actual=%d", test.code, test.status, status)
        }
        if test.status==503 && code!="SERVICE_UNAVAILABLE" {
            t.Fatalf("未知内部错误不得暴露原始名称: %s", code)
        }
    }
}
