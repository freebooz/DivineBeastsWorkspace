package main

import (
    "strings"
    "testing"
)

// 这些用例直接约束一次性工具的输入，阻止跨RunId账号、固定默认口令或宽松解析进入真实库。
func TestCredentialsRejectCrossRunAndMalformedInput(t *testing.T) {
    const run = "11111111-1111-4111-8111-111111111111"
    valid := `{"runId":"11111111-1111-4111-8111-111111111111","gameId":"divine-beasts","accounts":{"A":{"accountName":"online_11111111111141118111111111111111_a","password":"test-only-strong-password-A-123456","playerId":""},"B":{"accountName":"online_11111111111141118111111111111111_b","password":"test-only-strong-password-B-123456","playerId":""}}}`
    if _, err := readCredentials(strings.NewReader(valid), run); err != nil { t.Fatal(err) }
    for _, bad := range []string{
        strings.Replace(valid, `"gameId":"divine-beasts"`, `"gameId":"other"`, 1),
        strings.Replace(valid, "_a\"", "_b\"", 1),
        strings.Replace(valid, "test-only-strong-password-A-123456", "short", 1),
        strings.Replace(valid, "test-only-strong-password-A-123456", "test-only-strong-password-B-123456", 1),
        strings.Replace(valid, `"playerId":""`, `"extra":1,"playerId":""`, 1),
        valid + `{}`,
    } {
        if _, err := readCredentials(strings.NewReader(bad), run); err == nil { t.Fatal("must reject unsafe credentials") }
    }
    if _, err := readCredentials(strings.NewReader(valid), "22222222-2222-4222-8222-222222222222"); err == nil { t.Fatal("must reject cross-run credentials") }
    if _, err := readCredentials(strings.NewReader(valid), "../other"); err == nil { t.Fatal("must reject invalid run") }
}
