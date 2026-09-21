# UE 5.8.0 请求级重定向安全补丁

## 范围与现状

这是用户明确授权的最小引擎源码补丁：修改HTTP模块的 `Public/Interfaces/IHttpRequest.h`、`Private/Curl/CurlHttp.h/.cpp`，新增 `Private/Tests/HttpRedirectPolicyTests.cpp`。不改TLS、不修改全局默认、不改其他HTTP后端、不调整插件描述、不创建UE宿主、不执行引擎构建。

引擎 `Build.version` 已读为5.8.0；现场非Git仓库。`UE5.8-HttpRedirectPolicy.hashes.json` 记录的是**本轮实际读到的现场基线**及补丁后SHA256，不声称现场文件等于官方未修改源码。原文件已完整保留在工作空间 `Saved/Validation/EngineHttpRedirect/Before`。

## 锁定公开API与安全语义

```cpp
#define UE_HTTP_HAS_REQUEST_REDIRECT_POLICY 1
enum class EHttpRequestRedirectPolicy : uint8 { Follow, Reject };
virtual bool SetRedirectPolicy(EHttpRequestRedirectPolicy Policy) { return false; }
```

宏只表示源码接口存在，不保证当前运行后端支持；未知后端默认false。Curl覆盖仅在游戏线程、非Processing且枚举合法时接受。默认Follow保留原行为，Reject真正设置 `CURLOPT_FOLLOWLOCATION=0`。

已有句柄立即应用并检查curl返回码；延迟创建时保存选择，在Init和每次Setup时应用。设置失败返回false并阻止发送，显式再次成功设置才解除；Init/Setup失败沿现有PreProcess失败路径结束，不加入HTTP发送队列。策略不在Cleanup/retry中重置。Reject收到3xx时保留status/header回调，不把它当作内部跟随响应过滤。

调用端必须同时检查源码能力和运行后端的返回值；false绝不能继续发送认证请求，也不能通过SetOption回读假装支持：

```cpp
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
{
    // 报告当前后端不具备安全传输能力，不调用ProcessRequest。
    return;
}
#else
// 未打补丁的引擎明确拒绝安全发送，不能降级为自动跟随。
return;
#endif
```

**新增虚函数改变vtable/ABI，必须重新编译HTTP及其调用者、相关插件和目标。不能仅替换头文件或将旧DLL当作已修复。** 本轮没有构建或二进制生效证明。

## 精确应用与重复检测

PowerShell 7脚本参数使用调用者指定的引擎根，不硬编码个人盘符或用户名。脚本要求Git命令可用，但不要求引擎属于Git仓库。

```powershell
./Build/Engine/Patches/Apply-UE58HttpRedirectPolicy.ps1 -EngineRoot '<UE源码根>' -CheckOnly
./Build/Engine/Patches/Apply-UE58HttpRedirectPolicy.ps1 -EngineRoot '<UE源码根>'
```

检查版本、补丁自身摘要及全部四个文件状态。全部匹配修改前摘要才可应用；全部匹配修改后摘要返回AlreadyApplied；未知修改、部分应用、目标重解析点或版本不符均拒绝。实际写入前备份到工作空间Saved，应用后再次核对摘要。不自动回滚未知状态，不删除文件；回退前须由人工确认独占和当前哈希，使用保存的原文件恢复，并单独评估新增测试文件。

## 已留测试材料及未执行边界

引擎私有源码中的两个自动化身份：

- `System.HTTP.RedirectPolicy.Contract`：基类不支持、合法/非法枚举、游戏/非游戏线程、飞行中拒绝、Cleanup后再次Setup。
- `System.HTTP.RedirectPolicy.LoopbackNetwork`：真实请求向同源及跨源301/302/303/307/308端点发送，仅使用固定测试标记。要求原始3xx响应和status/Location回调保留，两个目标端点收到的请求计数合计为0。缺夹具参数明确失败，不假装通过。

`Tests/RedirectFixture.py` 为双端口回环HTTP夹具；不保存、不输出请求头/体，不用真实凭据。它只作为材料保留，本轮未启动。后续获准并重新编译引擎后，操作者可显式启动：

```text
python Build/Engine/Patches/Tests/RedirectFixture.py --output Saved/Validation/EngineHttpRedirect/<唯一运行目录>/fixture.json
```

读取输出JSON的origin，给正式工程自动化进程传 `-HttpRedirectPolicyTestOrigin=http://127.0.0.1:<端口>`，运行上面两项。应分别在 `http.CurlDeferEasyHandleLifetime` 的0/1启动配置执行；当前材料未证明legacy和deferred路径的真实网络结果，也未包含curl失败注入的引擎执行证据。

当前主工程仍受用户保留的三份空描述阻断。**UE编译、上述自动化、真实HTTP负例、native策略测试均未执行；没有red/green或网络成功计数。** 已做源码调用链核对和哈希记录；应用脚本在Saved副本上的验证另记实际日志，不能代替UE测试。

应用脚本已在 `Saved/Validation/EngineHttpRedirect/ApplyCheck-*` 的原文件副本验证：CheckOnly为Ready；实际应用后全部SHA256匹配；重复应用为AlreadyApplied；真实引擎只读CheckOnly为AlreadyApplied。实际输出保存在 `Saved/Validation/EngineHttpRedirect/PatchApplication.log`。首次副本尝试发现Git向上识别工作空间仓库而静默跳过路径，后置哈希门禁真实拒绝；脚本现用临时Git目录发现边界修正，重测通过，首次副本和备份保留未删除。这不是UE源码编译通过证明。

用户已切换世界系统任务，本补丁在当前安全断点冻结，不继续扩展。
