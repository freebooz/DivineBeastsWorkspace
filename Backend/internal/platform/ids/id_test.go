package ids

import "testing"

func TestValidateRejectsEmptyID(t *testing.T) {
	if err := Validate(""); err == nil {
		t.Fatal("期望空ID校验失败")
	}
}

func TestValidateAcceptsStableLogicalID(t *testing.T) {
	if err := Validate("Arena.Mode.Team5v5"); err != nil {
		t.Fatalf("期望合法逻辑ID通过校验: %v", err)
	}
}
