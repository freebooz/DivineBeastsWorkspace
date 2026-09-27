// Package main（契约代码生成工具）从Shared/Contracts生成Go/C++稳定目录绑定，并可调用官方Proto/OpenAPI生成器。
// 运行方式（从Backend目录）：go run ./internal/tools/contractcodegen -workspace-root=.. [-check] [-official]
package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"go/format"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type contractVersion struct {
	ContractVersion string `json:"contractVersion"`
	BackendModule   string `json:"backendModule"`
}

// toolVersions（代码生成工具版本锁）描述正式Codegen流水线必须使用的生成器版本。
type toolVersions struct {
	Protoc          string `json:"protoc"`
	ProtocGenGo     string `json:"protocGenGo"`
	ProtocGenGoGrpc string `json:"protocGenGoGrpc"`
	OAPICodegen     string `json:"oapiCodegen"`
}

type stringSchema struct {
	Enum []string `json:"enum"`
}

type roleExperienceMapping struct {
	ServerRole  string   `json:"serverRole"`
	Experiences []string `json:"experiences"`
}

type serverCatalogSchema struct {
	ProjectIdentity     projectIdentity         `json:"x-project-identity"`
	CatalogVersion      int                     `json:"x-catalog-version"`
	RoleExperienceMap   []roleExperienceMapping `json:"x-role-experience-map"`
	ArenaModeContextMap []arenaModeMapping      `json:"x-arena-mode-context-map"`
}

type projectIdentity struct {
	GameID    string `json:"gameId"`
	ProjectID string `json:"projectId"`
}

type arenaModeMapping struct {
	ArenaMode  string `json:"arenaMode"`
	ServerRole string `json:"serverRole"`
	Experience string `json:"experience"`
}

type compatibilityMatrix struct {
	Current struct {
		ContractVersion string `json:"contractVersion"`
	} `json:"current"`
	Supported []struct {
		MinimumInclusive string `json:"minContractVersion"`
		MaximumInclusive string `json:"maxContractVersion"`
	} `json:"supported"`
}

type contractCompatibility struct {
	MinimumInclusive string
	MaximumExclusive string
}

type divineBeastsCatalog struct {
	gameID                     string
	projectID                  string
	contractVersion            string
	catalogVersion             int
	generatedRevision          string
	serverRoles                []string
	experiences                []string
	arenaModes                 []string
	experienceMappings         []roleExperienceMapping
	arenaModeMappings          []arenaModeMapping
	clientServerCompatibility  contractCompatibility
	serverBackendCompatibility contractCompatibility
}

type openAPIRoute struct {
	OperationID string
	Method      string
	Path        string
}

type contractMetadata struct {
	OpenAPIOperations []string
	OpenAPIRoutes     []openAPIRoute
	ProtoServices     []string
	ProtoRPCs         []string
}

var (
	operationIDPattern          = regexp.MustCompile(`(?m)^\s*operationId:\s*([^\s#]+)`)
	openAPIPathPattern          = regexp.MustCompile(`^\s{2}(/[^:]+):\s*$`)
	openAPIMethodPattern        = regexp.MustCompile(`^\s{4}(get|post|put|patch|delete|options|head):\s*$`)
	openAPIOperationLinePattern = regexp.MustCompile(`^\s{6,}operationId:\s*([^\s#]+)`)
	servicePattern              = regexp.MustCompile(`(?m)^\s*service\s+([A-Za-z0-9_]+)\s*\{`)
	rpcPattern                  = regexp.MustCompile(`(?m)^\s*rpc\s+([A-Za-z0-9_]+)\s*\(`)
)

func main() {
	workspaceRoot := flag.String("workspace-root", "..", "DivineBeastsWorkspace根目录")
	check := flag.Bool("check", false, "只校验生成物是否最新")
	official := flag.Bool("official", false, "额外运行官方protoc/oapi-codegen生成器")
	flag.Parse()

	root, err := filepath.Abs(*workspaceRoot)
	must(err)
	shared := filepath.Join(root, "Shared")
	backend := filepath.Join(root, "Backend")
	if *official {
		validateOfficialTools(root)
	}

	var version contractVersion
	readJSON(filepath.Join(shared, "Docs", "contract-version.json"), &version)
	if version.ContractVersion == "" {
		panic("Shared/Docs/contract-version.json缺少contractVersion")
	}
	var roles, experiences, arenaModes stringSchema
	var serverCatalog serverCatalogSchema
	var compatibility compatibilityMatrix
	readJSON(filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "Schemas", "server-role.schema.json"), &roles)
	readJSON(filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "Schemas", "experience.schema.json"), &experiences)
	readJSON(filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "Schemas", "arena-mode.schema.json"), &arenaModes)
	readJSON(filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "Schemas", "server-catalog.schema.json"), &serverCatalog)
	readJSON(filepath.Join(shared, "Docs", "compatibility-matrix.json"), &compatibility)
	if compatibility.Current.ContractVersion != version.ContractVersion {
		panic("compatibility-matrix.json current.contractVersion必须与Shared/Docs/contract-version.json一致")
	}
	validateRoleExperienceMappings(roles.Enum, experiences.Enum, serverCatalog.RoleExperienceMap)
	validateArenaModeMappings(arenaModes.Enum, experiences.Enum, roles.Enum, serverCatalog.RoleExperienceMap, serverCatalog.ArenaModeContextMap)
	if serverCatalog.ProjectIdentity.GameID == "" || serverCatalog.ProjectIdentity.ProjectID == "" {
		panic("server-catalog.schema.json缺少x-project-identity中的gameId或projectId")
	}
	if serverCatalog.CatalogVersion < 1 {
		panic("server-catalog.schema.json中的x-catalog-version必须为正整数")
	}
	compatibilityRange := compatibilityRangeFromMatrix(compatibility, version.ContractVersion)
	contractInputs := readDivineBeastsCatalogInputs(shared)
	catalog := divineBeastsCatalog{
		gameID: serverCatalog.ProjectIdentity.GameID, projectID: serverCatalog.ProjectIdentity.ProjectID,
		contractVersion: version.ContractVersion, catalogVersion: serverCatalog.CatalogVersion,
		generatedRevision: generateRevision(contractInputs), serverRoles: roles.Enum, experiences: experiences.Enum,
		arenaModes: arenaModes.Enum, experienceMappings: serverCatalog.RoleExperienceMap,
		arenaModeMappings:         serverCatalog.ArenaModeContextMap,
		clientServerCompatibility: compatibilityRange, serverBackendCompatibility: compatibilityRange,
	}

	gpMeta := scanContracts(filepath.Join(shared, "Contracts", "GamePlatform"))
	dbMeta := scanContracts(filepath.Join(shared, "Contracts", "Games", "DivineBeasts"))

	outputs := map[string][]byte{
		filepath.Join(backend, "generated", "gameplatform", "contracts_generated.go"):                           formatGo(generateGamePlatformGo(version.ContractVersion, gpMeta)),
		filepath.Join(backend, "generated", "divinebeasts", "catalog_generated.go"):                             formatGo(generateDivineBeastsGo(catalog)),
		filepath.Join(backend, "generated", "divinebeasts", "contracts_generated.go"):                           formatGo(generateMetadataGo("divinebeasts", "神兽联盟共享协议生成元数据", dbMeta)),
		filepath.Join(shared, "Generated", "Cpp", "GamePlatform", "ContractVersion.generated.hpp"):              generateContractVersionCpp(version.ContractVersion),
		filepath.Join(shared, "Generated", "Cpp", "GamePlatform", "ContractRegistry.generated.hpp"):             generateMetadataCpp("GamePlatform::Contracts", gpMeta),
		filepath.Join(shared, "Generated", "Cpp", "Games", "DivineBeasts", "DivineBeastsCatalog.generated.hpp"): generateDivineBeastsCpp(catalog),
		filepath.Join(shared, "Generated", "Cpp", "Games", "DivineBeasts", "ContractRegistry.generated.hpp"):    generateMetadataCpp("DivineBeasts::Contracts", dbMeta),
		filepath.Join(shared, "Generated", "Cpp", "UEConsumerModules.generated.json"):                           generateConsumerMap(),
	}

	stale := false
	for path, content := range outputs {
		if *check {
			current, err := os.ReadFile(path)
			if err != nil || !bytes.Equal(current, content) {
				fmt.Fprintf(os.Stderr, "生成物过期: %s\n", path)
				stale = true
			}
			continue
		}
		must(os.MkdirAll(filepath.Dir(path), 0o755))
		must(os.WriteFile(path, content, 0o644))
		fmt.Println("generated", path)
	}
	if stale {
		os.Exit(2)
	}
	if *official {
		runOfficial(root)
	}
}

func scanContracts(root string) contractMetadata {
	var result contractMetadata
	_ = filepath.WalkDir(root, func(path string, entry os.DirEntry, err error) error {
		if err != nil || entry.IsDir() {
			return err
		}
		content, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		switch strings.ToLower(filepath.Ext(path)) {
		case ".yaml", ".yml":
			for _, match := range operationIDPattern.FindAllSubmatch(content, -1) {
				result.OpenAPIOperations = append(result.OpenAPIOperations, string(match[1]))
			}
			result.OpenAPIRoutes = append(result.OpenAPIRoutes, parseOpenAPIRoutes(content)...)
		case ".proto":
			for _, match := range servicePattern.FindAllSubmatch(content, -1) {
				result.ProtoServices = append(result.ProtoServices, string(match[1]))
			}
			for _, match := range rpcPattern.FindAllSubmatch(content, -1) {
				result.ProtoRPCs = append(result.ProtoRPCs, string(match[1]))
			}
		}
		return nil
	})
	result.OpenAPIOperations = uniqueSorted(result.OpenAPIOperations)
	result.OpenAPIRoutes = uniqueSortedRoutes(result.OpenAPIRoutes)
	result.ProtoServices = uniqueSorted(result.ProtoServices)
	result.ProtoRPCs = uniqueSorted(result.ProtoRPCs)
	return result
}

func parseOpenAPIRoutes(content []byte) []openAPIRoute {
	lines := strings.Split(string(content), "\n")
	currentPath := ""
	currentMethod := ""
	var routes []openAPIRoute
	for _, line := range lines {
		if match := openAPIPathPattern.FindStringSubmatch(line); len(match) == 2 {
			currentPath = match[1]
			currentMethod = ""
			continue
		}
		if match := openAPIMethodPattern.FindStringSubmatch(line); len(match) == 2 && currentPath != "" {
			currentMethod = strings.ToUpper(match[1])
			continue
		}
		if match := openAPIOperationLinePattern.FindStringSubmatch(line); len(match) == 2 && currentPath != "" && currentMethod != "" {
			routes = append(routes, openAPIRoute{OperationID: match[1], Method: currentMethod, Path: currentPath})
		}
	}
	return routes
}

func uniqueSortedRoutes(values []openAPIRoute) []openAPIRoute {
	seen := map[string]openAPIRoute{}
	for _, value := range values {
		if value.OperationID == "" || value.Method == "" || value.Path == "" {
			continue
		}
		if previous, ok := seen[value.OperationID]; ok && previous != value {
			panic(fmt.Sprintf("OpenAPI operationId映射冲突: %s", value.OperationID))
		}
		seen[value.OperationID] = value
	}
	out := make([]openAPIRoute, 0, len(seen))
	for _, value := range seen {
		out = append(out, value)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].OperationID < out[j].OperationID })
	return out
}

func generateGamePlatformGo(version string, meta contractMetadata) []byte {
	var b strings.Builder
	b.WriteString("// Code generated from Shared/Contracts/GamePlatform. DO NOT EDIT.\n")
	b.WriteString("// Package gameplatform（游戏平台共享Go绑定）保存公共契约版本和生成注册表。\npackage gameplatform\n\n")
	fmt.Fprintf(&b, "const ContractVersion = %q // ContractVersion（公共跨语言契约版本）。\n\n", version)
	writeGoSlice(&b, "OpenAPIOperations", "OpenAPI OperationId（HTTP操作编号）", meta.OpenAPIOperations)
	writeGoRoutes(&b, meta.OpenAPIRoutes)
	writeGoSlice(&b, "ProtoServices", "Proto Services（跨语言RPC服务）", meta.ProtoServices)
	writeGoSlice(&b, "ProtoRPCs", "Proto RPCs（跨语言RPC方法）", meta.ProtoRPCs)
	return []byte(b.String())
}

func generateMetadataGo(pkg, comment string, meta contractMetadata) []byte {
	var b strings.Builder
	fmt.Fprintf(&b, "// Code generated from Shared Contracts. DO NOT EDIT.\n// Package %s（%s）。\npackage %s\n\n", pkg, comment, pkg)
	writeGoSlice(&b, "OpenAPIOperations", "OpenAPI OperationId（HTTP操作编号）", meta.OpenAPIOperations)
	writeGoRoutes(&b, meta.OpenAPIRoutes)
	writeGoSlice(&b, "ProtoServices", "Proto Services（项目RPC服务）", meta.ProtoServices)
	writeGoSlice(&b, "ProtoRPCs", "Proto RPCs（项目RPC方法）", meta.ProtoRPCs)
	return []byte(b.String())
}

func writeGoRoutes(b *strings.Builder, routes []openAPIRoute) {
	b.WriteString("// OpenAPIRoute（OpenAPI路由）由Shared契约生成，供Transport/Adapter避免手工复制HTTP路径。\n")
	b.WriteString("type OpenAPIRoute struct { OperationID, Method, Path string }\n\n")
	b.WriteString("// OpenAPIRoutes（OpenAPI路由目录）保存OperationId、HTTP方法和路径。\nvar OpenAPIRoutes = []OpenAPIRoute{\n")
	for _, route := range routes {
		fmt.Fprintf(b, "\t{OperationID: %q, Method: %q, Path: %q},\n", route.OperationID, route.Method, route.Path)
	}
	b.WriteString("}\n\n")
}

func writeGoSlice(b *strings.Builder, name, comment string, values []string) {
	fmt.Fprintf(b, "// %s（%s）。\nvar %s = []string{\n", name, comment, name)
	for _, value := range values {
		fmt.Fprintf(b, "\t%q,\n", value)
	}
	b.WriteString("}\n\n")
}

func generateDivineBeastsGo(catalog divineBeastsCatalog) []byte {
	var b strings.Builder
	b.WriteString("// Code generated from Shared/Contracts/Games/DivineBeasts. DO NOT EDIT.\n")
	b.WriteString("// Package divinebeasts（神兽联盟共享Go绑定）保存项目身份、版本与玩法目录。\npackage divinebeasts\n\nconst (\n")
	fmt.Fprintf(&b, "\tGameID = %q\n\tProjectID = %q\n\tContractVersion = %q\n\tGeneratedRevision = %q\n\tCatalogVersion = %d\n", catalog.gameID, catalog.projectID, catalog.contractVersion, catalog.generatedRevision, catalog.catalogVersion)
	fmt.Fprintf(&b, "\tClientServerMinimumContractVersion = %q\n\tClientServerMaximumExclusiveContractVersion = %q\n", catalog.clientServerCompatibility.MinimumInclusive, catalog.clientServerCompatibility.MaximumExclusive)
	fmt.Fprintf(&b, "\tServerBackendMinimumContractVersion = %q\n\tServerBackendMaximumExclusiveContractVersion = %q\n", catalog.serverBackendCompatibility.MinimumInclusive, catalog.serverBackendCompatibility.MaximumExclusive)
	for _, value := range append(append(append([]string{}, catalog.serverRoles...), catalog.experiences...), catalog.arenaModes...) {
		fmt.Fprintf(&b, "\t%s = %q\n", goConstName(value), value)
	}
	b.WriteString(")\n\n// ExperienceServerRoles（体验对应的服务器角色）由Shared ServerCatalog生成，包含保留的兼容体验映射。\nvar ExperienceServerRoles = map[string]string{\n")
	for _, mapping := range catalog.experienceMappings {
		for _, experienceID := range mapping.Experiences {
			fmt.Fprintf(&b, "\t%q: %q,\n", experienceID, mapping.ServerRole)
		}
	}
	b.WriteString("}\n\n// ArenaModeContexts（竞技模式对应的权威角色与体验）由Shared ServerCatalog生成。\ntype ArenaModeContext struct { ServerRoleID, ExperienceID string }\nvar ArenaModeContexts = map[string]ArenaModeContext{\n")
	for _, mapping := range catalog.arenaModeMappings {
		fmt.Fprintf(&b, "\t%q: {ServerRoleID: %q, ExperienceID: %q},\n", mapping.ArenaMode, mapping.ServerRole, mapping.Experience)
	}
	b.WriteString("}\n")
	return []byte(b.String())
}

func generateContractVersionCpp(version string) []byte {
	return []byte(fmt.Sprintf("// Code generated from Shared/Contracts/GamePlatform. DO NOT EDIT.\n#pragma once\n#include <string_view>\nnamespace GamePlatform::Contracts { inline constexpr std::string_view ContractVersion = %q; }\n", version))
}

func generateMetadataCpp(namespace string, meta contractMetadata) []byte {
	var b strings.Builder
	b.WriteString("// Code generated from Shared Contracts. DO NOT EDIT.\n#pragma once\n#include <array>\n#include <string_view>\n")
	fmt.Fprintf(&b, "namespace %s {\n", namespace)
	writeCppArray(&b, "OpenAPIOperations", meta.OpenAPIOperations)
	writeCppRoutes(&b, meta.OpenAPIRoutes)
	writeCppArray(&b, "ProtoServices", meta.ProtoServices)
	writeCppArray(&b, "ProtoRPCs", meta.ProtoRPCs)
	b.WriteString("}\n")
	return []byte(b.String())
}

func writeCppRoutes(b *strings.Builder, routes []openAPIRoute) {
	b.WriteString("struct OpenAPIRoute { std::string_view OperationId; std::string_view Method; std::string_view Path; };\n")
	fmt.Fprintf(b, "inline constexpr std::array<OpenAPIRoute, %d> OpenAPIRoutes = {", len(routes))
	for i, route := range routes {
		if i > 0 {
			b.WriteString(", ")
		}
		fmt.Fprintf(b, "OpenAPIRoute{%q, %q, %q}", route.OperationID, route.Method, route.Path)
	}
	b.WriteString("};\n")
}

func writeCppArray(b *strings.Builder, name string, values []string) {
	fmt.Fprintf(b, "inline constexpr std::array<std::string_view, %d> %s = {", len(values), name)
	for i, value := range values {
		if i > 0 {
			b.WriteString(", ")
		}
		fmt.Fprintf(b, "%q", value)
	}
	b.WriteString("};\n")
}

func generateDivineBeastsCpp(catalog divineBeastsCatalog) []byte {
	var b strings.Builder
	b.WriteString("// Code generated from Shared/Contracts/Games/DivineBeasts and Shared/Docs. DO NOT EDIT.\n#pragma once\n#include <array>\n#include <string_view>\nnamespace DivineBeasts::Contracts {\n")
	fmt.Fprintf(&b, "inline constexpr const char* GameId = %q;\ninline constexpr const char* ProjectId = %q;\ninline constexpr const char* ContractVersion = %q;\ninline constexpr int CatalogVersion = %d;\ninline constexpr const char* GeneratedRevision = %q;\n", catalog.gameID, catalog.projectID, catalog.contractVersion, catalog.catalogVersion, catalog.generatedRevision)
	fmt.Fprintf(&b, "inline constexpr const char* ClientServerMinimumContractVersion = %q;\ninline constexpr const char* ClientServerMaximumExclusiveContractVersion = %q;\n", catalog.clientServerCompatibility.MinimumInclusive, catalog.clientServerCompatibility.MaximumExclusive)
	fmt.Fprintf(&b, "inline constexpr const char* ServerBackendMinimumContractVersion = %q;\ninline constexpr const char* ServerBackendMaximumExclusiveContractVersion = %q;\n", catalog.serverBackendCompatibility.MinimumInclusive, catalog.serverBackendCompatibility.MaximumExclusive)
	for _, value := range append(append(append([]string{}, catalog.serverRoles...), catalog.experiences...), catalog.arenaModes...) {
		fmt.Fprintf(&b, "inline constexpr std::string_view %s = %q;\n", cppConstName(value), value)
	}
	writeCppCStringArray(&b, "ServerRoles", catalog.serverRoles)
	writeCppCStringArray(&b, "ExperienceIds", catalog.experiences)
	writeCppCStringArray(&b, "ArenaModeIds", catalog.arenaModes)
	count := 0
	for _, mapping := range catalog.experienceMappings {
		count += len(mapping.Experiences)
	}
	fmt.Fprintf(&b, "struct ExperienceServerRoleMapping { const char* ExperienceId; const char* ServerRoleId; };\ninline constexpr std::array<ExperienceServerRoleMapping, %d> ExperienceMappings = {{", count)
	first := true
	for _, mapping := range catalog.experienceMappings {
		for _, experienceID := range mapping.Experiences {
			if !first {
				b.WriteString(", ")
			}
			fmt.Fprintf(&b, "ExperienceServerRoleMapping{%q, %q}", experienceID, mapping.ServerRole)
			first = false
		}
	}
	b.WriteString("}};\ninline constexpr const auto& ExperienceServerRoleMappings = ExperienceMappings;\n")
	fmt.Fprintf(&b, "struct ArenaModeContextMapping { const char* ArenaModeId; const char* ServerRoleId; const char* ExperienceId; };\ninline constexpr std::array<ArenaModeContextMapping, %d> ArenaModeMappings = {{", len(catalog.arenaModeMappings))
	for i, mapping := range catalog.arenaModeMappings {
		if i > 0 {
			b.WriteString(", ")
		}
		fmt.Fprintf(&b, "ArenaModeContextMapping{%q, %q, %q}", mapping.ArenaMode, mapping.ServerRole, mapping.Experience)
	}
	b.WriteString("}};\n")
	b.WriteString("}\n")
	return []byte(b.String())
}

func writeCppCStringArray(b *strings.Builder, name string, values []string) {
	fmt.Fprintf(b, "inline constexpr std::array<const char*, %d> %s = {{", len(values), name)
	for i, value := range values {
		if i > 0 {
			b.WriteString(", ")
		}
		fmt.Fprintf(b, "%q", value)
	}
	b.WriteString("}};\n")
}

func readDivineBeastsCatalogInputs(sharedRoot string) map[string][]byte {
	paths := []string{
		"Contracts/Games/DivineBeasts/Schemas/server-role.schema.json",
		"Contracts/Games/DivineBeasts/Schemas/experience.schema.json",
		"Contracts/Games/DivineBeasts/Schemas/arena-mode.schema.json",
		"Contracts/Games/DivineBeasts/Schemas/server-catalog.schema.json",
		"Docs/contract-version.json",
		"Docs/compatibility-matrix.json",
	}
	inputs := make(map[string][]byte, len(paths))
	for _, relativePath := range paths {
		inputs[relativePath] = mustReadFile(filepath.Join(sharedRoot, filepath.FromSlash(relativePath)))
	}
	return inputs
}

func mustReadFile(path string) []byte {
	content, err := os.ReadFile(path)
	if err != nil {
		panic(err)
	}
	return content
}

func generateRevision(inputs map[string][]byte) string {
	paths := make([]string, 0, len(inputs))
	for path := range inputs {
		paths = append(paths, filepath.ToSlash(path))
	}
	sort.Strings(paths)
	hash := sha256.New()
	for _, path := range paths {
		_, _ = hash.Write([]byte(path))
		_, _ = hash.Write([]byte{0})
		_, _ = hash.Write(inputs[path])
		_, _ = hash.Write([]byte{0})
	}
	return hex.EncodeToString(hash.Sum(nil))
}

func compatibilityRangeFromMatrix(matrix compatibilityMatrix, currentVersion string) contractCompatibility {
	if len(matrix.Supported) != 1 {
		panic("兼容矩阵必须恰有一个连续supported区间，UE运行时当前只表达单一半开区间")
	}
	entry := matrix.Supported[0]
	maximumExclusive, err := inclusiveMaxToExclusive(entry.MaximumInclusive)
	if err != nil {
		panic(fmt.Sprintf("compatibility-matrix.json的maxContractVersion无效: %v", err))
	}
	minimum, err := parseSemanticVersion(entry.MinimumInclusive)
	if err != nil {
		panic(fmt.Sprintf("compatibility-matrix.json的minContractVersion无效: %v", err))
	}
	maximum, err := parseSemanticVersion(maximumExclusive)
	if err != nil || compareSemanticVersion(minimum, maximum) >= 0 {
		panic("兼容矩阵的半开版本区间必须非空且有序")
	}
	current, err := parseSemanticVersion(currentVersion)
	if err != nil || compareSemanticVersion(current, minimum) < 0 || compareSemanticVersion(current, maximum) >= 0 {
		panic("Shared当前契约版本不在兼容矩阵声明的半开范围内")
	}
	return contractCompatibility{MinimumInclusive: entry.MinimumInclusive, MaximumExclusive: maximumExclusive}
}

func inclusiveMaxToExclusive(version string) (string, error) {
	parts := strings.Split(version, ".")
	if len(parts) == 2 && parts[1] == "x" {
		major, err := strconv.Atoi(parts[0])
		if err != nil || major < 0 {
			return "", fmt.Errorf("无效主版本%q", parts[0])
		}
		if major == int(^uint(0)>>1) {
			return "", fmt.Errorf("主版本溢出: %q", version)
		}
		return fmt.Sprintf("%d.0.0", major+1), nil
	}
	if len(parts) != 3 {
		return "", fmt.Errorf("期望完整的M.m.p、M.m.x或M.x，实际为%q", version)
	}
	major, err := strconv.Atoi(parts[0])
	if err != nil || major < 0 {
		return "", fmt.Errorf("无效主版本%q", parts[0])
	}
	minor, err := strconv.Atoi(parts[1])
	if err != nil || minor < 0 {
		return "", fmt.Errorf("无效次版本%q", parts[1])
	}
	if parts[2] == "x" {
		if minor == int(^uint(0)>>1) {
			return "", fmt.Errorf("次版本溢出: %q", version)
		}
		return fmt.Sprintf("%d.%d.0", major, minor+1), nil
	}
	patch, err := strconv.Atoi(parts[2])
	if err != nil || patch < 0 {
		return "", fmt.Errorf("无效补丁版本%q", parts[2])
	}
	if patch == int(^uint(0)>>1) {
		return "", fmt.Errorf("补丁版本溢出: %q", version)
	}
	return fmt.Sprintf("%d.%d.%d", major, minor, patch+1), nil
}

type semanticVersion struct {
	Major int
	Minor int
	Patch int
}

func parseSemanticVersion(version string) (semanticVersion, error) {
	parts := strings.Split(version, ".")
	if len(parts) != 3 {
		return semanticVersion{}, fmt.Errorf("期望完整M.m.p版本，实际为%q", version)
	}
	values := [3]int{}
	for index, part := range parts {
		value, err := strconv.Atoi(part)
		if err != nil || value < 0 {
			return semanticVersion{}, fmt.Errorf("无效版本段%q", part)
		}
		values[index] = value
	}
	return semanticVersion{Major: values[0], Minor: values[1], Patch: values[2]}, nil
}

func compareSemanticVersion(left, right semanticVersion) int {
	if left.Major != right.Major {
		if left.Major < right.Major {
			return -1
		}
		return 1
	}
	if left.Minor != right.Minor {
		if left.Minor < right.Minor {
			return -1
		}
		return 1
	}
	if left.Patch != right.Patch {
		if left.Patch < right.Patch {
			return -1
		}
		return 1
	}
	return 0
}

func validateArenaModeMappings(modes, experiences, roles []string, experienceMappings []roleExperienceMapping, mappings []arenaModeMapping) {
	modeSet := make(map[string]struct{}, len(modes))
	for _, mode := range modes {
		modeSet[mode] = struct{}{}
	}
	experienceSet := make(map[string]struct{}, len(experiences))
	for _, experience := range experiences {
		experienceSet[experience] = struct{}{}
	}
	roleSet := make(map[string]struct{}, len(roles))
	for _, role := range roles {
		roleSet[role] = struct{}{}
	}
	experienceRoles := make(map[string]string, len(experiences))
	for _, mapping := range experienceMappings {
		for _, experience := range mapping.Experiences {
			experienceRoles[experience] = mapping.ServerRole
		}
	}
	seen := make(map[string]struct{}, len(mappings))
	for _, mapping := range mappings {
		if _, ok := modeSet[mapping.ArenaMode]; !ok {
			panic(fmt.Sprintf("ServerCatalog竞技模式映射包含未声明模式: %s", mapping.ArenaMode))
		}
		if _, duplicate := seen[mapping.ArenaMode]; duplicate {
			panic(fmt.Sprintf("ServerCatalog竞技模式映射重复: %s", mapping.ArenaMode))
		}
		seen[mapping.ArenaMode] = struct{}{}
		if _, ok := roleSet[mapping.ServerRole]; !ok {
			panic(fmt.Sprintf("ServerCatalog竞技模式映射包含未声明角色: %s", mapping.ServerRole))
		}
		if _, ok := experienceSet[mapping.Experience]; !ok {
			panic(fmt.Sprintf("ServerCatalog竞技模式映射包含未声明体验: %s", mapping.Experience))
		}
		if experienceRoles[mapping.Experience] != mapping.ServerRole {
			panic(fmt.Sprintf("ServerCatalog竞技模式映射角色与体验冲突: %s", mapping.ArenaMode))
		}
	}
	if len(seen) != len(modeSet) {
		panic("ServerCatalog竞技模式映射未覆盖全部ArenaMode")
	}
}

// validateRoleExperienceMappings（校验角色体验映射）拒绝未知、重复或未覆盖的契约ID，避免跨端静默采用不同目录。
func validateRoleExperienceMappings(roles, experiences []string, mappings []roleExperienceMapping) {
	roleSet := make(map[string]struct{}, len(roles))
	for _, role := range roles {
		roleSet[role] = struct{}{}
	}
	experienceSet := make(map[string]struct{}, len(experiences))
	for _, experience := range experiences {
		experienceSet[experience] = struct{}{}
	}
	seenRoles := make(map[string]struct{}, len(roles))
	seenExperiences := make(map[string]struct{}, len(experiences))
	for _, mapping := range mappings {
		if _, ok := roleSet[mapping.ServerRole]; !ok {
			panic(fmt.Sprintf("ServerCatalog映射包含未声明角色: %s", mapping.ServerRole))
		}
		if _, duplicate := seenRoles[mapping.ServerRole]; duplicate {
			panic(fmt.Sprintf("ServerCatalog角色映射重复: %s", mapping.ServerRole))
		}
		seenRoles[mapping.ServerRole] = struct{}{}
		if len(mapping.Experiences) == 0 {
			panic(fmt.Sprintf("ServerCatalog角色没有体验映射: %s", mapping.ServerRole))
		}
		for _, experienceID := range mapping.Experiences {
			if _, ok := experienceSet[experienceID]; !ok {
				panic(fmt.Sprintf("ServerCatalog映射包含未声明体验: %s", experienceID))
			}
			if _, duplicate := seenExperiences[experienceID]; duplicate {
				panic(fmt.Sprintf("ServerCatalog体验映射重复: %s", experienceID))
			}
			seenExperiences[experienceID] = struct{}{}
		}
	}
	if len(seenRoles) != len(roleSet) {
		panic("ServerCatalog角色映射未覆盖全部正式角色")
	}
	if len(seenExperiences) != len(experienceSet) {
		panic("ServerCatalog角色映射未覆盖全部正式及兼容体验ID")
	}
}

func generateConsumerMap() []byte {
	value := map[string]any{
		"version": 1,
		"consumers": []map[string]any{
			{"contract": "GamePlatform/OpenAPI", "goOutput": "Backend/generated/openapi/gameplatform", "ueModules": []string{"GamePlatformOnline", "GamePlatformSession"}},
			{"contract": "GamePlatform/Proto", "goOutput": "Backend/generated/proto/shared/gameplatform", "cppOutput": "Shared/Generated/Cpp/GamePlatform/Proto", "ueModules": []string{"GamePlatformServer", "GamePlatformSession", "GamePlatformArenaServer"}},
			{"contract": "Games/DivineBeasts/OpenAPI", "goOutput": "Backend/generated/openapi/divinebeasts", "ueModules": []string{"DivineBeastsRuntime"}},
			{"contract": "Games/DivineBeasts/Proto", "goOutput": "Backend/generated/proto/shared/divinebeasts", "cppOutput": "Shared/Generated/Cpp/Games/DivineBeasts/Proto", "ueModules": []string{"DivineBeastsRuntime", "DivineBeastsServer", "DivineBeastsArena"}},
		},
	}
	data, _ := json.MarshalIndent(value, "", "  ")
	return append(data, '\n')
}

func runOfficial(root string) {
	shared := filepath.Join(root, "Shared")
	backend := filepath.Join(root, "Backend")

	// 正式生成前清理仅由生成器拥有的目录，避免删除字段后旧文件残留。
	removeGeneratedDir(filepath.Join(shared, "Generated", "Cpp", "GamePlatform", "Proto"))
	removeGeneratedDir(filepath.Join(shared, "Generated", "Cpp", "Games", "DivineBeasts", "Proto"))
	removeGeneratedDir(filepath.Join(backend, "generated", "proto", "shared"))
	removeGeneratedDir(filepath.Join(backend, "generated", "proto", "internal"))
	// 内部Go绑定须放Backend/internal下；旧generated/proto/internal违反Go导入边界。
	removeGeneratedDir(filepath.Join(backend, "internal", "generated"))
	removeGeneratedDir(filepath.Join(backend, "generated", "openapi"))

	// GamePlatform Proto（公共跨语言协议）：
	// Go端使用module参数让go_package精确落到Backend/generated/proto/...；
	// C++端直接生成到Shared/Generated/Cpp/GamePlatform/Proto，供UE消费者模块只读引用。
	gamePlatformProtoRoot := filepath.Join(shared, "Contracts", "GamePlatform", "Proto")
	gamePlatformProtoFiles, _ := filepath.Glob(filepath.Join(gamePlatformProtoRoot, "*.proto"))
	must(os.MkdirAll(filepath.Join(shared, "Generated", "Cpp", "GamePlatform", "Proto"), 0o755))
	args := []string{
		"-I", gamePlatformProtoRoot,
		"--go_out=" + backend,
		"--go_opt=module=divinebeasts/backend",
		"--go-grpc_out=" + backend,
		"--go-grpc_opt=module=divinebeasts/backend",
		"--cpp_out=" + filepath.Join(shared, "Generated", "Cpp", "GamePlatform", "Proto"),
	}
	args = append(args, gamePlatformProtoFiles...)
	run("protoc", args...)

	// DivineBeasts Proto（项目跨语言协议）：与公共协议分开输出，避免项目定义污染GamePlatform。
	divineBeastsProtoRoot := filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "Proto")
	divineBeastsProtoFiles, _ := filepath.Glob(filepath.Join(divineBeastsProtoRoot, "*.proto"))
	if len(divineBeastsProtoFiles) > 0 {
		must(os.MkdirAll(filepath.Join(shared, "Generated", "Cpp", "Games", "DivineBeasts", "Proto"), 0o755))
		args = []string{
			"-I", divineBeastsProtoRoot,
			"-I", gamePlatformProtoRoot,
			"--go_out=" + backend,
			"--go_opt=module=divinebeasts/backend",
			"--go-grpc_out=" + backend,
			"--go-grpc_opt=module=divinebeasts/backend",
			"--cpp_out=" + filepath.Join(shared, "Generated", "Cpp", "Games", "DivineBeasts", "Proto"),
		}
		args = append(args, divineBeastsProtoFiles...)
		run("protoc", args...)
	}

	// Backend内部Proto只生成Go，不进入Shared；go_package映射Backend/internal/generated/<domain>/v1。
	internalProtoRoot := filepath.Join(backend, "internal", "contracts", "proto")
	internalFiles, _ := filepath.Glob(filepath.Join(internalProtoRoot, "*.proto"))
	args = []string{
		"-I", internalProtoRoot,
		"--go_out=" + backend,
		"--go_opt=module=divinebeasts/backend",
		"--go-grpc_out=" + backend,
		"--go-grpc_opt=module=divinebeasts/backend",
	}
	args = append(args, internalFiles...)
	run("protoc", args...)

	// OpenAPI：每份契约生成独立Go package，避免多个规格中的类型/方法重名。
	for _, group := range []struct{ input, output string }{
		{filepath.Join(shared, "Contracts", "GamePlatform", "OpenAPI"), filepath.Join(backend, "generated", "openapi", "gameplatform")},
		{filepath.Join(shared, "Contracts", "Games", "DivineBeasts", "OpenAPI"), filepath.Join(backend, "generated", "openapi", "divinebeasts")},
	} {
		files, _ := filepath.Glob(filepath.Join(group.input, "*.yaml"))
		for _, file := range files {
			name := strings.TrimSuffix(filepath.Base(file), filepath.Ext(file))
			pkg := strings.NewReplacer("-", "_", ".", "_").Replace(name)
			pkgDir := filepath.Join(group.output, name)
			must(os.MkdirAll(pkgDir, 0o755))
			out := filepath.Join(pkgDir, name+".generated.go")
			run("oapi-codegen", "-generate", "types,client", "-package", pkg, "-o", out, file)
		}
	}
}

func validateOfficialTools(root string) {
	shared := filepath.Join(root, "Shared")
	var locked toolVersions
	readJSON(filepath.Join(shared, "Docs", "codegen-tools.lock.json"), &locked)
	mustToolVersion("protoc", []string{"--version"}, locked.Protoc)
	mustToolVersion("protoc-gen-go", []string{"--version"}, locked.ProtocGenGo)
	mustToolVersion("protoc-gen-go-grpc", []string{"--version"}, locked.ProtocGenGoGrpc)
	mustToolVersion("oapi-codegen", []string{"-version"}, locked.OAPICodegen)
}

func mustToolVersion(name string, args []string, expected string) {
	if _, err := exec.LookPath(name); err != nil {
		panic("缺少正式Codegen工具: " + name)
	}
	if expected == "" {
		panic("codegen-tools.lock.json缺少版本: " + name)
	}
	cmd := exec.Command(name, args...)
	out, err := cmd.CombinedOutput()
	must(err)
	actual := strings.TrimSpace(string(out))
	if !strings.Contains(actual, expected) {
		panic(fmt.Sprintf("正式Codegen工具版本不匹配: %s 期望=%s 实际=%s", name, expected, actual))
	}
}

func removeGeneratedDir(path string) {
	must(os.RemoveAll(path))
	must(os.MkdirAll(path, 0o755))
}

func formatGo(content []byte) []byte {
	formatted, err := format.Source(content)
	if err != nil {
		panic(fmt.Sprintf("生成Go代码格式化失败: %v", err))
	}
	return formatted
}
func run(name string, args ...string) {
	cmd := exec.Command(name, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	must(cmd.Run())
}
func readJSON(path string, value any) {
	data, err := os.ReadFile(path)
	must(err)
	must(json.Unmarshal(data, value))
}
func uniqueSorted(values []string) []string {
	seen := map[string]struct{}{}
	var out []string
	for _, v := range values {
		if _, ok := seen[v]; !ok {
			seen[v] = struct{}{}
			out = append(out, v)
		}
	}
	sort.Strings(out)
	return out
}
func goConstName(value string) string {
	parts := regexp.MustCompile(`[^A-Za-z0-9]+`).Split(value, -1)
	var b strings.Builder
	for _, p := range parts {
		if p == "" {
			continue
		}
		b.WriteString(strings.ToUpper(p[:1]) + p[1:])
	}
	return b.String()
}
func cppConstName(value string) string { return goConstName(value) }
func must(err error) {
	if err != nil {
		panic(err)
	}
}
