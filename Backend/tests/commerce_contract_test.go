// 本文件属于业务后端测试层，只读Shared中的交易契约；不修改权威数据、不调用支付服务。
// 调用方为Go测试门禁，Schema片段只在单次测试内存中存在；失败阻止发布，不能替代真实交易验收。
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
	// Windows与Linux读取相同契约应得到相同门禁结果，统一行结束符但不改变契约内容。
	contract := strings.ReplaceAll(string(data), "\r\n", "\n")

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

	// Go的RE2不支持前瞻断言；用相邻同级Schema标题的位置确定片段边界，避免串入下一类型的playerId。
	schemaHeaders := regexp.MustCompile(`(?m)^    ([A-Z][A-Za-z0-9]*):`).FindAllStringSubmatchIndex(contract, -1)
	requestNames := map[string]bool{"CreatePurchaseIntentRequest": true, "CreateOrderRequest": true, "SubmitReceiptRequest": true}
	var requestSchemas []string
	for i, header := range schemaHeaders {
		if !requestNames[contract[header[2]:header[3]]] {
			continue
		}
		end := len(contract)
		if i+1 < len(schemaHeaders) {
			end = schemaHeaders[i+1][0]
		}
		requestSchemas = append(requestSchemas, contract[header[0]:end])
	}
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
