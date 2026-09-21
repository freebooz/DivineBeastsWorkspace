//go:build productiondeps

package main

import (
    "context"
    "encoding/json"
    "errors"
    "flag"
    "fmt"
    "os"
    "path/filepath"
    "strings"
    "time"

    "divinebeasts/backend/internal/modules/identity"
    "divinebeasts/backend/internal/modules/playerdata"
    "divinebeasts/backend/internal/platform/database/postgres"
    "github.com/jackc/pgx/v5/pgxpool"
)

// 只接非秘密RunId/路径；DSN由受控进程环境注入，任何失败只输出阶段，不输出原始数据库错误。
func main() {
    runID:=flag.String("run-id","","本轮D格式GUID")
    credentialFile:=flag.String("credentials-file","","受限短期凭据文件绝对路径")
    flag.Parse()
    if err:=prepare(*runID,*credentialFile);err!=nil {
        fmt.Fprintln(os.Stderr,"Online测试数据准备失败；原始诊断已抑制，请检查所属RunId/迁移/服务依赖。")
        os.Exit(1)
    }
    fmt.Printf("RunId=%s Accounts=2 Status=Passed\n",*runID)
}

func prepare(runID,path string) error {
    if !filepath.IsAbs(path) { return errUnsafeCredentials }
    file,err:=os.Open(path);if err!=nil{return errUnsafeCredentials}
    credentials,err:=readCredentials(file,runID);file.Close();if err!=nil{return err}
    dsn:=os.Getenv("POSTGRES_DSN")
    config,err:=pgxpool.ParseConfig(dsn)
    expectedDatabase:="online_"+strings.ReplaceAll(runID,"-","")
    if err!=nil || config.ConnConfig.Database!=expectedDatabase { return errors.New("不是本轮新隔离库") }
    ctx,cancel:=context.WithTimeout(context.Background(),60*time.Second);defer cancel()
    pool,err:=postgres.Open(ctx,postgres.Config{DSN:dsn,MaxConns:2,PingTimeout:5*time.Second});if err!=nil{return errors.New("数据库不可用")};defer pool.Close()
    var schema,owner string
    if err=pool.Inner().QueryRow(ctx,"SELECT current_schema(),run_id FROM public.online_integration_owner").Scan(&schema,&owner);err!=nil || schema!="public" || owner!=runID { return errors.New("数据库归属标记不匹配") }
    identityService,err:=identity.NewPersistentService(postgres.NewOnlineIdentityRepository(pool),identity.SystemClock{},15*time.Minute,30*24*time.Hour)
    if err!=nil{return errors.New("身份服务构造失败")}
    profiles:=playerdata.NewService(postgres.NewOnlinePlayerRepository(pool))
    for _,account:=range []*testAccount{&credentials.Accounts.A,&credentials.Accounts.B} {
        created,err:=identityService.EnsureAccount(ctx,credentials.GameID,account.AccountName,account.Password)
        if err!=nil{return errors.New("账号准备失败")}
        if account.PlayerID!="" && account.PlayerID!=created.PlayerID {return errors.New("账号身份不匹配")}
        // 不直接写player_profiles；明确通过该数据所有者的幂等初始化用例。
        if err=profiles.EnsureProfile(ctx,created.PlayerID,credentials.GameID);err!=nil{return errors.New("资料准备失败")}
        account.PlayerID=created.PlayerID
    }
    encoded,err:=json.MarshalIndent(credentials,"","  ");if err!=nil{return errUnsafeCredentials}
    temporary,err:=os.CreateTemp(filepath.Dir(path),".online-credentials-*");if err!=nil{return errors.New("受限文件不可写")}
    tempName:=temporary.Name();defer os.Remove(tempName)
    if _,err=temporary.Write(encoded);err!=nil{temporary.Close();return errors.New("受限文件写入失败")}
    if err=temporary.Close();err!=nil{return errors.New("受限文件关闭失败")}
    if err=os.Rename(tempName,path);err!=nil{return errors.New("受限文件替换失败")}
    return nil
}
