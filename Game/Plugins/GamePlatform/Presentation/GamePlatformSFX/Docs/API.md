# GamePlatformSFX API 说明

版本：0.2.0｜2026-09-29

## 1. Definition

创建 `UGamePlatformSFXDefinition（平台音效定义）` 实例，并为基类 `LogicalId` 使用规范身份，例如：

```text
presentation.sfx.ui_confirm@1
presentation.sfx.combat_hit@1
```

禁止把 `/Game/Audio/...` 资产路径当作 `DefinitionId`。

`Sound` 可引用 SoundWave、SoundCue 或 MetaSound Source；Attenuation与Concurrency均使用UE原生资产。

## 2. 获取服务

```cpp
IGamePlatformSFXService* SFX = IGamePlatformSFXService::Get(WorldContextObject);
```

Dedicated Server、无有效World或不支持的环境返回 `nullptr`。

## 3. 世界位置播放

```cpp
FGamePlatformSFXRequest Request;
Request.RequestId = FGuid::NewGuid();
Request.DefinitionId = TEXT("presentation.sfx.combat_hit@1");
Request.Location = ImpactLocation;

FGamePlatformSFXResult Result = SFX->Play(Request);
```

正常首次调用返回 `Queued`，表示 `GamePlatformData` 已接受异步Definition租约；不是“声音已经实际播放”的同步承诺。

## 4. 附着播放

Definition的 `PlaybackSpace` 配置为 `Attached` 后，C++调用方需要提供弱附着目标：

```cpp
Request.AttachComponent = MeshComponent;
Request.AttachPointName = TEXT("weapon_socket");
```

异步加载完成前Owner若已销毁，请求Fail Closed，不会持有裸指针。

## 5. 停止与取消

```cpp
SFX->Stop(Result.Handle);
SFX->StopByRequestId(Request.RequestId);
```

重复Stop已经进入淡出流程的活动实例按幂等成功处理；旧World/旧Generation句柄拒绝。

## 6. 参数

Definition必须先在 `AllowedFloatParameters` 中声明，例如 `Intensity`。请求才能：

```cpp
Request.FloatParameters.Add(TEXT("Intensity"), 0.8f);
```

运行中也可以：

```cpp
SFX->SetFloatParameter(Handle, TEXT("Intensity"), 0.5f);
```

未声明参数拒绝，不向MetaSound任意注入字符串参数。

## 7. Presentation标准入口

Gameplay正常路径不应直接链接 `GamePlatformSFXClient`。应提交平台中立Presentation Request，并由Presentation Catalog解析：

```text
ProviderChannel = SFX
DefinitionId = presentation.sfx.combat_hit@1
```

SFX Bridge自动完成转换。

预测语义：Predicted/Confirmed使用相同RequestId时只播放一次；Corrected先停止旧实例再重放；Cancelled只停止，不重新播放。

当前中立Presentation Request只携带逻辑SourceId/位置，不携带USceneComponent，因此 `Attached（附着播放）` 目前仅属于SFX低层C++服务能力。正常Gameplay若需要“语义附着到角色骨骼”的标准链，应先在平台Presentation层建立中立Attachment（附着）解析契约，而不是让项目Gameplay直接依赖SFX或把项目Actor类型塞进平台请求。
