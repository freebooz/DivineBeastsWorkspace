package tests

import (
	"os/exec"
	"testing"
)

// TestGeneratedContractsAreCurrent（生成物新鲜度测试）保证Shared Contract修改后必须重新生成Go/C++绑定。
func TestGeneratedContractsAreCurrent(t *testing.T) {
	cmd := exec.Command("go", "run", "./internal/tools/contractcodegen", "-workspace-root=..", "-check")
	cmd.Dir = ".."
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("Codegen生成物已过期或生成校验失败: %v\n%s", err, output)
	}
}
