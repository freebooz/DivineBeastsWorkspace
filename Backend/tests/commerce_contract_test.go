package tests

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

// TestCommerceContract（交易契约门禁）验证跨语言唯一真源包含恢复所需接口、精确整数线格式和认证身份边界。
// 若该测试失败，客户端与后端可能对金额精度、幂等身份或玩家归属产生不同解释，禁止继续发布交易能力。
func TestCommerceContract(t *testing.T) {
	contractPath := filepath.Join("..", "..", "Shared", "Contracts", "GamePlatform", "OpenAPI", "commerce.openapi.yaml")
	data, err := os.ReadFile(contractPath)
	if err != nil {
		t.Fatalf("读取Commerce唯一契约失败: %v", err)
	}
	contract := string(data)

	for _, required := range []string{
		"/v1/commerce/catalog:",
		"/v1/commerce/purchase-intents:",
		"/v1/commerce/orders:",
		"/v1/commerce/orders/{orderId}:",
		"/v1/commerce/orders/{orderId}/receipts:",
		"/v1/commerce/orders/{orderId}/reconcile:",
		"CommerceInt64:",
		"pattern: '^-?(0|[1-9][0-9]{0,18})$'",
		"errorCode:",
		"receiptSubmissionId:",
	} {
		if !strings.Contains(contract, required) {
			t.Errorf("Commerce契约缺少必需语义: %s", required)
		}
	}

	requestSchemaPattern := regexp.MustCompile(`(?ms)^    (CreatePurchaseIntentRequest|CreateOrderRequest|SubmitReceiptRequest):.*?(?=^    [A-Z]|\z)`)
	requestSchemas := requestSchemaPattern.FindAllString(contract, -1)
	if len(requestSchemas) != 3 {
		t.Fatalf("未找到三个交易写请求Schema，实际=%d", len(requestSchemas))
	}
	for _, schema := range requestSchemas {
		if strings.Contains(strings.ToLower(schema), "playerid") {
			t.Errorf("交易写请求不得接受客户端playerId: %s", schema)
		}
	}

	operationPattern := regexp.MustCompile(`(?m)^      operationId: ([A-Za-z][A-Za-z0-9]*)$`)
	seen := map[string]struct{}{}
	for _, match := range operationPattern.FindAllStringSubmatch(contract, -1) {
		if _, exists := seen[match[1]]; exists {
			t.Errorf("Commerce契约operationId重复: %s", match[1])
		}
		seen[match[1]] = struct{}{}
	}
	if len(seen) != 7 {
		t.Errorf("Commerce契约应声明7个操作，实际=%d", len(seen))
	}
}
