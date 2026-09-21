package servicehost

import (
	"net/http/httptest"
	"testing"
)

func TestHealthEndpoints(t *testing.T) {
	h := NewHandler("MatchService", "0.1.0")
	for _, path := range []string{"/health/live", "/health/ready", "/version"} {
		r := httptest.NewRequest("GET", path, nil)
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		if w.Code != 200 {
			t.Fatalf("%s返回状态码=%d", path, w.Code)
		}
	}
}
