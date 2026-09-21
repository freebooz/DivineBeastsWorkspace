// Package main是受控的一次性测试数据工具，不是第六个业务应用，不提供网络端口。
package main

import (
    "encoding/json"
    "errors"
    "io"
    "regexp"
    "strings"
)

// 形状与UE驱动共享；密码只从受限短期文件读取，绝不序列化到普通证据或stdout。
type testAccount struct {
    AccountName string `json:"accountName"`
    Password string `json:"password"`
    PlayerID string `json:"playerId"`
}
type testCredentials struct {
    RunID string `json:"runId"`
    GameID string `json:"gameId"`
    Accounts struct { A testAccount `json:"A"`; B testAccount `json:"B"` } `json:"accounts"`
}
var runPattern = regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`)
var errUnsafeCredentials = errors.New("受控凭据文件或RunId不合法")

func readCredentials(reader io.Reader, runID string) (testCredentials, error) {
    var credentials testCredentials
    if !runPattern.MatchString(runID) || runID == "00000000-0000-0000-0000-000000000000" { return credentials, errUnsafeCredentials }
    data, err := io.ReadAll(io.LimitReader(reader, 65537))
    if err != nil || len(data)>65536 { return credentials, errUnsafeCredentials }
    decoder := json.NewDecoder(strings.NewReader(string(data)))
    decoder.DisallowUnknownFields()
    if decoder.Decode(&credentials) != nil { return credentials, errUnsafeCredentials }
    var extra any
    if decoder.Decode(&extra) != io.EOF { return credentials, errUnsafeCredentials }
    if credentials.RunID != runID || credentials.GameID != "divine-beasts" { return credentials, errUnsafeCredentials }
    prefix := "online_"+strings.ReplaceAll(runID,"-","")
    if credentials.Accounts.A.AccountName != prefix+"_a" || credentials.Accounts.B.AccountName != prefix+"_b" ||
        credentials.Accounts.A.Password == credentials.Accounts.B.Password { return credentials, errUnsafeCredentials }
    for _, account := range []testAccount{credentials.Accounts.A,credentials.Accounts.B} {
        if len(account.Password)<32 || len(account.Password)>72 || strings.ContainsAny(account.Password,"\r\n\x00") { return credentials,errUnsafeCredentials }
    }
    return credentials,nil
}
