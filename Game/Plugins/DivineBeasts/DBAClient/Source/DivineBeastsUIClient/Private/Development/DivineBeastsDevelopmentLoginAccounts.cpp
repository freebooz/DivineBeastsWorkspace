// 第三层客户端开发适配：JSON仅在本次调用栈解析，密码不进入Widget快照、资产或日志。
#include "Development/DivineBeastsDevelopmentLoginAccounts.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DivineBeasts::UI::DevelopmentLogin
{
    namespace
    {
        // 先完整验证再发布名称，避免部分配置失效时留下可点击的半套账号。
        bool ParseAccounts(const FString& Json, TArray<TSharedPtr<FJsonValue>>& OutAccounts,
            TArray<FString>& OutNames)
        {
            OutAccounts.Reset(); OutNames.Reset();
            if (Json.IsEmpty() || Json.Len() > 65536 ||
                !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutAccounts) ||
                OutAccounts.Num() != 10)
            { OutAccounts.Reset(); return false; }
            TSet<FString> UniqueNames;
            for (const auto& Entry : OutAccounts)
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                FString Name, Password;
                if (!Entry.IsValid() || !Entry->TryGetObject(Object) || !Object || !Object->IsValid() ||
                    !(*Object)->TryGetStringField(TEXT("accountName"), Name) ||
                    !(*Object)->TryGetStringField(TEXT("password"), Password) ||
                    Name.IsEmpty() || Name != Name.TrimStartAndEnd() || Name.Len() > 128 ||
                    Password.IsEmpty() || UniqueNames.Contains(Name.ToLower()))
                { OutAccounts.Reset(); OutNames.Reset(); return false; }
                UniqueNames.Add(Name.ToLower()); OutNames.Add(MoveTemp(Name));
                Password.Reset();
            }
            return true;
        }
    }

    bool ReadAccountNames(const FString& Json, TArray<FString>& OutAccountNames)
    {
        TArray<TSharedPtr<FJsonValue>> Accounts;
        return ParseAccounts(Json, Accounts, OutAccountNames);
    }

    bool ReadCredential(const FString& Json, int32 Index, const FString& ExpectedAccountName,
        FString& OutAccountName, FString& OutPassword)
    {
        OutAccountName.Reset(); OutPassword.Reset();
        TArray<TSharedPtr<FJsonValue>> Accounts; TArray<FString> Names;
        if (!ParseAccounts(Json, Accounts, Names) || !Names.IsValidIndex(Index) ||
            Names[Index] != ExpectedAccountName)
        { return false; }
        OutAccountName = Names[Index];
        return Accounts[Index]->AsObject()->TryGetStringField(TEXT("password"), OutPassword);
    }
}
