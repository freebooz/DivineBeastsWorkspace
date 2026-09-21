// Package ids（标识符包）提供跨业务模块统一使用的逻辑ID校验规则。
package ids

import (
	"errors"
	"regexp"
)

var stableIDPattern = regexp.MustCompile(`^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$`)

// Validate（校验逻辑ID）验证跨服务使用的稳定逻辑ID。
//
// 设计约束：
//   - ID必须非空；
//   - 最长128个字符；
//   - 仅允许字母、数字、点号、下划线、冒号和短横线；
//   - DisplayName（显示名称）等用户可见文本不使用本规则。
func Validate(value string) error {
	if !stableIDPattern.MatchString(value) {
		return errors.New("逻辑ID格式无效")
	}
	return nil
}
