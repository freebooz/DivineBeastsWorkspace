package gateway

import (
	"encoding/json"
	"fmt"
	"html"
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

const swaggerContractsRootEnvironment = "DIVINEBEASTS_SWAGGER_CONTRACTS_ROOT"

// registerSwaggerDocumentation（注册本地 Swagger 文档）仅在装配层显式挂载 Shared Contracts 时公开文档，避免生产服务因默认配置泄露内部控制面接口说明。
func (a *api) registerSwaggerDocumentation(contractsRoot string) {
	contractsRoot = strings.TrimSpace(contractsRoot)
	if contractsRoot == "" {
		return
	}
	handler, err := newSwaggerDocumentationHandler(contractsRoot)
	if err != nil {
		panic(fmt.Errorf("Swagger 文档配置无效: %w", err))
	}
	a.mux.Handle("GET /swagger/", handler)
}

type swaggerDocumentationHandler struct {
	indexPage []byte
	files     http.Handler
}

func newSwaggerDocumentationHandler(contractsRoot string) (*swaggerDocumentationHandler, error) {
	rootInfo, err := os.Stat(contractsRoot)
	if err != nil {
		return nil, fmt.Errorf("读取共享契约目录失败: %w", err)
	}
	if !rootInfo.IsDir() {
		return nil, fmt.Errorf("共享契约路径不是目录: %s", contractsRoot)
	}

	specificationPaths, err := findOpenAPISpecificationPaths(contractsRoot)
	if err != nil {
		return nil, err
	}
	if len(specificationPaths) == 0 {
		return nil, fmt.Errorf("共享契约目录不包含 OpenAPI 规格: %s", contractsRoot)
	}

	return &swaggerDocumentationHandler{
		indexPage: buildSwaggerIndexPage(specificationPaths),
		files:     http.StripPrefix("/swagger/specs/", http.FileServer(http.Dir(contractsRoot))),
	}, nil
}

func (h *swaggerDocumentationHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path == "/swagger/" {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		_, _ = w.Write(h.indexPage)
		return
	}
	if strings.HasPrefix(r.URL.Path, "/swagger/specs/") {
		h.files.ServeHTTP(w, r)
		return
	}
	http.NotFound(w, r)
}

func findOpenAPISpecificationPaths(contractsRoot string) ([]string, error) {
	var specificationPaths []string
	err := filepath.WalkDir(contractsRoot, func(filePath string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if entry.IsDir() {
			return nil
		}
		fileName := strings.ToLower(entry.Name())
		if !strings.HasSuffix(fileName, ".openapi.yaml") && !strings.HasSuffix(fileName, ".openapi.yml") {
			return nil
		}
		relativePath, err := filepath.Rel(contractsRoot, filePath)
		if err != nil {
			return fmt.Errorf("计算 OpenAPI 相对路径失败: %w", err)
		}
		specificationPaths = append(specificationPaths, filepath.ToSlash(relativePath))
		return nil
	})
	if err != nil {
		return nil, fmt.Errorf("扫描 OpenAPI 规格失败: %w", err)
	}
	sort.Strings(specificationPaths)
	return specificationPaths, nil
}

func buildSwaggerIndexPage(specificationPaths []string) []byte {
	specificationsJSON, err := json.Marshal(specificationPaths)
	if err != nil {
		panic(fmt.Errorf("序列化 Swagger 规格列表失败: %w", err))
	}

	var specificationLinks strings.Builder
	for _, specificationPath := range specificationPaths {
		escapedPath := html.EscapeString(specificationPath)
		specificationLinks.WriteString("<li><a href=\"/swagger/specs/")
		specificationLinks.WriteString(escapedPath)
		specificationLinks.WriteString("\">")
		specificationLinks.WriteString(escapedPath)
		specificationLinks.WriteString("</a></li>")
	}

	return []byte(fmt.Sprintf(`<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DivineBeasts Swagger UI</title>
  <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5.17.14/swagger-ui.css">
</head>
<body>
  <main>
    <h1>DivineBeasts Swagger UI</h1>
    <p>本地 Gateway 从挂载的 Shared/Contracts 读取 OpenAPI 真源。内部控制面接口仅用于可信后端与专用服务器，不应暴露到公网。</p>
    <div id="swagger-ui"></div>
    <section id="swagger-fallback">
      <h2>原始 OpenAPI 规格</h2>
      <p>若浏览器无法加载 Swagger UI 的公共静态资源，可直接下载以下 YAML 文件并导入本地 Swagger Editor 或其他兼容工具。</p>
      <ul>%s</ul>
    </section>
  </main>
  <script src="https://unpkg.com/swagger-ui-dist@5.17.14/swagger-ui-bundle.js"></script>
  <script>
    const specificationPaths = %s;
    if (window.SwaggerUIBundle) {
      window.SwaggerUIBundle({
        dom_id: '#swagger-ui',
        deepLinking: true,
        displayRequestDuration: true,
        urls: specificationPaths.map((path) => ({ name: path, url: '/swagger/specs/' + path })),
      });
    }
  </script>
</body>
</html>`, specificationLinks.String(), specificationsJSON))
}
