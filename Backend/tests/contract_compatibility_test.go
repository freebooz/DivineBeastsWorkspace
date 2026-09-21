package tests

import (
	"encoding/json"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"

	generatedgp "divinebeasts/backend/generated/gameplatform"
)

type contractVersionFile struct {
	ContractVersion string `json:"contractVersion"`
}

type compatibilityMatrix struct {
	Current struct {
		ContractVersion       string `json:"contractVersion"`
		BackendVersion        string `json:"backendVersion"`
		ClientNetworkProtocol int    `json:"clientNetworkProtocol"`
		ServerNetworkProtocol int    `json:"serverNetworkProtocol"`
	} `json:"current"`
	Supported []struct {
		ContractMajor         int    `json:"contractMajor"`
		MinContractVersion    string `json:"minContractVersion"`
		MaxContractVersion    string `json:"maxContractVersion"`
		ClientNetworkProtocol int    `json:"clientNetworkProtocol"`
		ServerNetworkProtocol int    `json:"serverNetworkProtocol"`
	} `json:"supported"`
}

// TestClientServerBackendContractCompatibility（Client/Server/Backend协议兼容测试）锁定三个消费者看到的ContractVersion一致。
func TestClientServerBackendContractCompatibility(t *testing.T) {
	sharedRoot := filepath.Join("..", "..", "Shared")
	var version contractVersionFile
	readJSONFile(t, filepath.Join(sharedRoot, "Docs", "contract-version.json"), &version)
	if version.ContractVersion != generatedgp.ContractVersion {
		t.Fatalf("Go生成ContractVersion=%s，Shared=%s", generatedgp.ContractVersion, version.ContractVersion)
	}
	cpp, err := os.ReadFile(filepath.Join(sharedRoot, "Generated", "Cpp", "GamePlatform", "ContractVersion.generated.hpp"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(cpp), `ContractVersion = "`+version.ContractVersion+`"`) {
		t.Fatalf("UE C++生成ContractVersion未同步为%s", version.ContractVersion)
	}
	var matrix compatibilityMatrix
	readJSONFile(t, filepath.Join(sharedRoot, "Docs", "compatibility-matrix.json"), &matrix)
	if matrix.Current.ContractVersion != version.ContractVersion {
		t.Fatalf("CompatibilityMatrix ContractVersion=%s，期望=%s", matrix.Current.ContractVersion, version.ContractVersion)
	}
	if matrix.Current.BackendVersion == "" || matrix.Current.ClientNetworkProtocol <= 0 || matrix.Current.ServerNetworkProtocol <= 0 {
		t.Fatal("CompatibilityMatrix缺少Backend/Client/Server当前版本信息")
	}
	major := semverMajor(t, version.ContractVersion)
	covered := false
	for _, supported := range matrix.Supported {
		if supported.ContractMajor != major {
			continue
		}
		if supported.ClientNetworkProtocol != matrix.Current.ClientNetworkProtocol || supported.ServerNetworkProtocol != matrix.Current.ServerNetworkProtocol {
			continue
		}
		if versionInRange(t, version.ContractVersion, supported.MinContractVersion, supported.MaxContractVersion) {
			covered = true
			break
		}
	}
	if !covered {
		t.Fatalf("当前Client/Server/Backend组合未被supported兼容矩阵覆盖: contract=%s clientProtocol=%d serverProtocol=%d", version.ContractVersion, matrix.Current.ClientNetworkProtocol, matrix.Current.ServerNetworkProtocol)
	}
}

// TestProtoEvolutionRules（Proto演进规则测试）验证关键字段号和三服务器模型，防止兼容性破坏。
func TestProtoEvolutionRules(t *testing.T) {
	sharedRoot := filepath.Join("..", "..", "Shared", "Contracts")
	gameServer := mustRead(t, filepath.Join(sharedRoot, "GamePlatform", "Proto", "game-server-control.proto"))
	transfer := mustRead(t, filepath.Join(sharedRoot, "GamePlatform", "Proto", "server-transfer.proto"))
	for _, expected := range []string{
		"string experience_id = 13;",
		"string assignment_id = 9;",
		"string assignment_type = 14;",
		"string assignment_id = 16;",
		"string destination_experience_id = 17;",
	} {
		if !strings.Contains(gameServer+transfer, expected) {
			t.Fatalf("缺少兼容新增字段: %s", expected)
		}
	}
	if strings.Contains(gameServer+transfer, "GameServer.Role.Lobby") {
		t.Fatal("跨语言协议禁止重新引入独立Lobby ServerRole")
	}
}

// TestOpenAPIOperationIDsUnique（OpenAPI操作ID唯一测试）避免生成客户端方法冲突。
func TestOpenAPIOperationIDsUnique(t *testing.T) {
	root := filepath.Join("..", "..", "Shared", "Contracts")
	pattern := regexp.MustCompile(`(?m)^\s*operationId:\s*([^\s#]+)`)
	seen := map[string]string{}
	err := filepath.WalkDir(root, func(path string, entry os.DirEntry, err error) error {
		if err != nil || entry.IsDir() || !(strings.HasSuffix(path, ".yaml") || strings.HasSuffix(path, ".yml")) {
			return err
		}
		content, readErr := os.ReadFile(path)
		if readErr != nil {
			return readErr
		}
		for _, match := range pattern.FindAllStringSubmatch(string(content), -1) {
			id := match[1]
			if previous, exists := seen[id]; exists {
				t.Fatalf("重复operationId=%s: %s 与 %s", id, previous, path)
			}
			seen[id] = path
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(seen) == 0 {
		t.Fatal("未发现OpenAPI operationId")
	}
}

func semverMajor(t *testing.T, value string) int {
	t.Helper()
	parts := strings.Split(value, ".")
	if len(parts) != 3 {
		t.Fatalf("版本不是x.y.z格式: %s", value)
	}
	major, err := strconv.Atoi(parts[0])
	if err != nil {
		t.Fatalf("版本Major非法: %s", value)
	}
	return major
}

func versionInRange(t *testing.T, current, min, max string) bool {
	t.Helper()
	cur := parseSemver(t, current)
	minimum := parseSemver(t, min)
	if compareSemver(cur, minimum) < 0 {
		return false
	}
	if strings.HasSuffix(max, ".x") {
		maxParts := strings.Split(max, ".")
		if len(maxParts) != 3 {
			t.Fatalf("maxContractVersion非法: %s", max)
		}
		major, err := strconv.Atoi(maxParts[0])
		if err != nil {
			t.Fatalf("maxContractVersion Major非法: %s", max)
		}
		minor, err := strconv.Atoi(maxParts[1])
		if err != nil {
			t.Fatalf("maxContractVersion Minor非法: %s", max)
		}
		return cur[0] == major && cur[1] == minor
	}
	maximum := parseSemver(t, max)
	return compareSemver(cur, maximum) <= 0
}

func parseSemver(t *testing.T, value string) [3]int {
	t.Helper()
	parts := strings.Split(value, ".")
	if len(parts) != 3 {
		t.Fatalf("版本不是x.y.z格式: %s", value)
	}
	var result [3]int
	for i, part := range parts {
		n, err := strconv.Atoi(part)
		if err != nil {
			t.Fatalf("版本字段非法: %s", value)
		}
		result[i] = n
	}
	return result
}

func compareSemver(left, right [3]int) int {
	for i := range left {
		if left[i] < right[i] {
			return -1
		}
		if left[i] > right[i] {
			return 1
		}
	}
	return 0
}

func readJSONFile(t *testing.T, path string, value any) {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if err := json.Unmarshal(data, value); err != nil {
		t.Fatal(err)
	}
}

func mustRead(t *testing.T, path string) string {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	return string(data)
}
