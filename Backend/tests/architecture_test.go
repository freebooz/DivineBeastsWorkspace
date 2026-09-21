// Package tests（后端跨服务测试）包含仓库结构和架构边界守卫。
package tests

import (
	"os"
	"path/filepath"
	"sort"
	"testing"
)

// TestFiveThinEntrypoints（五个薄入口结构测试）确保业务后端只有正式五个可部署入口。
func TestFiveThinEntrypoints(t *testing.T) {
	entries, err := os.ReadDir(filepath.Join("..", "cmd"))
	if err != nil {
		t.Fatal(err)
	}
	var got []string
	for _, e := range entries {
		if e.IsDir() {
			got = append(got, e.Name())
		}
	}
	sort.Strings(got)
	want := []string{"gameservercontrolservice", "gatewayservice", "identityservice", "matchservice", "playerdataservice"}
	if len(got) != len(want) {
		t.Fatalf("cmd目录=%v，期望=%v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("cmd目录=%v，期望=%v", got, want)
		}
	}
}

// TestBackendRootLayout（后端根目录结构测试）防止再次引入与正式规划冲突的平行架构根目录。
func TestBackendRootLayout(t *testing.T) {
	root := ".."
	entries, err := os.ReadDir(root)
	if err != nil {
		t.Fatal(err)
	}
	allowedDirs := map[string]bool{
		"cmd": true, "internal": true, "pkg": true, "generated": true,
		"migrations": true, "configs": true, "tests": true,
	}
	forbiddenDirs := map[string]bool{
		"gameplatform": true, "games": true, "Applications": true,
		"Infrastructure": true, "Deployment": true, "Services": true,
	}
	for _, e := range entries {
		if !e.IsDir() {
			continue
		}
		if forbiddenDirs[e.Name()] {
			t.Fatalf("Backend根目录禁止重新引入旧/试验性目录: %s", e.Name())
		}
		if !allowedDirs[e.Name()] {
			t.Fatalf("Backend出现未登记根目录: %s", e.Name())
		}
	}
}

// TestGeneratedOwnership（生成代码所有权测试）确保Backend/generated仅用于跨语言Go绑定生成代码。
func TestGeneratedOwnership(t *testing.T) {
	entries, err := os.ReadDir(filepath.Join("..", "generated"))
	if err != nil {
		t.Fatal(err)
	}
	var got []string
	for _, e := range entries {
		if e.IsDir() {
			got = append(got, e.Name())
		}
	}
	sort.Strings(got)
	want := []string{"divinebeasts", "gameplatform", "openapi", "proto"}
	if len(got) != len(want) {
		t.Fatalf("generated目录=%v，期望=%v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("generated目录=%v，期望=%v", got, want)
		}
	}
}
