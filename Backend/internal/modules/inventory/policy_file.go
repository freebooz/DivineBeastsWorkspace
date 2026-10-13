package inventory

// 后端正式物品目录读取与严格校验：服务器规则来自受控部署配置，
// 不从UE客户端Definition（显示定义）或HTTP请求体取得堆叠上限、消费许可。

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
)

// PolicyDocument（物品规则文件）使用版本号防止旧结构被误当作正式发奖依据。
type PolicyDocument struct {
	SchemaVersion int          `json:"schemaVersion"`
	GameID        string       `json:"gameId"`
	Notes         string       `json:"notes"`
	Items         []ItemPolicy `json:"items"`
}

// LoadItemPolicyCatalog（读取只读服务器物品规则目录）拒绝未知字段、坏版本、重复定义和超长文件。
// 调用方必须从只读部署路径打开文件；本函数不接受游戏客户端上传。
// expectedGameID（预期游戏ID）可选；生产必须提供，防止载入另一个游戏的物品规则。
func LoadItemPolicyCatalog(reader io.Reader, expectedGameID ...string) (*StaticItemPolicyCatalog, error) {
	if reader == nil {
		return nil, errors.New("后端物品规则文件读取器不能为空")
	}
	payload, err := io.ReadAll(io.LimitReader(reader, 512*1024+1))
	if err != nil || len(payload) > 512*1024 {
		return nil, errors.New("后端物品规则文件无法读取或超过512KiB")
	}
	decoder := json.NewDecoder(bytes.NewReader(payload))
	decoder.DisallowUnknownFields()
	var document PolicyDocument
	if err := decoder.Decode(&document); err != nil {
		return nil, err
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return nil, errors.New("物品规则文件存在尾随数据")
	}
	if document.SchemaVersion != 1 || !validID(document.GameID) {
		return nil, errors.New("后端物品规则目录版本或GameID不合法")
	}
	if len(expectedGameID) > 1 ||
		(len(expectedGameID) == 1 &&
			(!validID(expectedGameID[0]) || expectedGameID[0] != document.GameID)) {
		return nil, errors.New("后端物品目录GameID与当前运行的游戏不一致")
	}
	return NewStaticItemPolicyCatalog(document.Items)
}
