package gateway

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// TestSwaggerIndexDefaultsToGatewayAndGroupsOperations（Swagger 首页默认文档测试）验证多规格下拉列表默认打开公网 Gateway 规格，并以分组展开、操作折叠的列表模式显示接口。
func TestSwaggerIndexDefaultsToGatewayAndGroupsOperations(t *testing.T) {
	indexPage := string(buildSwaggerIndexPage([]string{
		"GamePlatform/OpenAPI/game-server-control.openapi.yaml",
		"GamePlatform/OpenAPI/gateway.openapi.yaml",
	}))

	for _, expected := range []string{
		"'urls.primaryName': \"GamePlatform/OpenAPI/gateway.openapi.yaml\"",
		"docExpansion: 'list'",
		"presets: [SwaggerUIBundle.presets.apis, SwaggerUIStandalonePreset]",
		"layout: 'StandaloneLayout'",
		"const specificationCacheKey = Date.now().toString()",
		"url: '/swagger/specs/' + path + '?_=' + specificationCacheKey",
	} {
		if !strings.Contains(indexPage, expected) {
			t.Errorf("Swagger 首页缺少预期配置 %q", expected)
		}
	}
}

// TestSwaggerDocumentationUsesConfiguredContractsRoot（Swagger 文档根目录测试）验证仅在本地显式配置共享契约目录时，Gateway 才公开 Swagger UI 入口和原始 OpenAPI 文件。
func TestSwaggerDocumentationUsesConfiguredContractsRoot(t *testing.T) {
	contractsRoot := t.TempDir()
	specificationPath := filepath.Join(contractsRoot, "GamePlatform", "OpenAPI", "gateway.openapi.yaml")
	if err := os.MkdirAll(filepath.Dir(specificationPath), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(specificationPath, []byte("openapi: 3.1.0\ninfo:\n  title: Gateway\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("DIVINEBEASTS_SWAGGER_CONTRACTS_ROOT", contractsRoot)

	indexRequest := httptest.NewRequest(http.MethodGet, "/swagger/", nil)
	indexRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(indexRecorder, indexRequest)
	if indexRecorder.Code != http.StatusOK {
		t.Fatalf("Swagger 文档首页状态码=%d body=%s", indexRecorder.Code, indexRecorder.Body.String())
	}
	if cacheControl := indexRecorder.Header().Get("Cache-Control"); cacheControl != "no-store" {
		t.Errorf("Swagger 文档首页 Cache-Control=%q，期望 no-store", cacheControl)
	}
	if !strings.Contains(indexRecorder.Body.String(), "Swagger UI") || !strings.Contains(indexRecorder.Body.String(), "GamePlatform/OpenAPI/gateway.openapi.yaml") {
		t.Fatalf("Swagger 文档首页缺少契约链接: %s", indexRecorder.Body.String())
	}

	specificationRequest := httptest.NewRequest(http.MethodGet, "/swagger/specs/GamePlatform/OpenAPI/gateway.openapi.yaml", nil)
	specificationRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(specificationRecorder, specificationRequest)
	if specificationRecorder.Code != http.StatusOK || !strings.Contains(specificationRecorder.Body.String(), "openapi: 3.1.0") {
		t.Fatalf("Swagger 原始契约访问失败: status=%d body=%s", specificationRecorder.Code, specificationRecorder.Body.String())
	}
	if cacheControl := specificationRecorder.Header().Get("Cache-Control"); cacheControl != "no-store" {
		t.Errorf("Swagger OpenAPI 规格 Cache-Control=%q，期望 no-store", cacheControl)
	}
}

// TestSwaggerDocumentationIsDisabledWithoutContractsRoot（未配置文档根目录测试）验证生产装配未显式挂载共享契约时不会对外暴露本地 Swagger 文档入口。
func TestSwaggerDocumentationIsDisabledWithoutContractsRoot(t *testing.T) {
	t.Setenv("DIVINEBEASTS_SWAGGER_CONTRACTS_ROOT", "")
	request := httptest.NewRequest(http.MethodGet, "/swagger/", nil)
	recorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(recorder, request)
	if recorder.Code != http.StatusNotFound {
		t.Fatalf("未配置契约目录时 Swagger 文档状态码=%d，期望 404", recorder.Code)
	}
}
