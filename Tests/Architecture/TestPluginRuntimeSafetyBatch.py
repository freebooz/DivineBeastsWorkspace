#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 插件运行期专项：会话传输同步失败/取消、Ticker生命周期、鸟群限频源码合同。

本脚本是无需锁定编辑器的静态回归。不能代替 UE5.8 实际 Automation、
客户端/服务器联机试验、Unreal Insights 性能或真人手动签收。
"""
from __future__ import annotations

import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SESSION=ROOT/"Game/Plugins/GamePlatform/OnlineServices/GamePlatformSession/Source/GamePlatformSession/Private"
VFX=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private"


def source(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


class PluginRuntimeSafetyBatchTests(unittest.TestCase):
    def test_session_synchronous_transport_failure_is_not_success(self):
        impl=source(SESSION/"GamePlatformSessionClientSubsystem.cpp")
        start=impl.index("StartedTransport->BeginTransfer(Request, MoveTemp(Callbacks));")
        body=impl[start:impl.index("bool UGamePlatformSessionClientSubsystem::CancelTransfer",start)]
        self.assertIn("Runtime->ActiveOperationId != ExpectedOperation",body)
        self.assertIn("StartedTransport != Transport",body)
        self.assertIn("const TSharedPtr<IGamePlatformSessionTransport> StartedTransport = Transport;",impl)
        self.assertIn("Snapshot.TransferOperationId == ExpectedOperation",body)
        self.assertIn("Snapshot.State == EGamePlatformSessionTransferState::Ready",body)
        self.assertIn("GamePlatformSessionErrors::TransportFailed",body)
        self.assertLess(body.index("return false;"),body.rindex("OutResult = FGamePlatformResult::Success();"))

    def test_session_state_broadcast_can_synchronously_cancel_before_begin(self):
        impl=source(SESSION/"GamePlatformSessionClientSubsystem.cpp")
        start=impl.index("    RefreshSnapshot();",impl.index("bool UGamePlatformSessionClientSubsystem::BeginOperation("))
        before=impl[start:impl.index("    const FGuid ExpectedOperation = Request.TransferOperationId;",start)]
        self.assertIn("Runtime->ActiveOperationId != Request.TransferOperationId",before)
        self.assertIn("Runtime->State->Snapshot().bOperationActive",before)
        self.assertIn("!Transport.IsValid()",before)
        self.assertIn("return false;",before)

    def test_real_transport_stops_travel_and_proof_after_sync_cancel(self):
        transport=source(SESSION/"Transport/GamePlatformUESessionTransport.cpp")
        start=transport.index("const FGuid BegunOperationId = Request.TransferOperationId;")
        end=transport.index("HandshakeTicker = FTSTicker::GetCoreTicker().AddTicker(",start)
        body=transport[start:end]
        self.assertEqual(3,body.count("ActiveOperationId != BegunOperationId"))
        self.assertLess(body.index("Callbacks.OnBindingPrepared"),body.index("Callbacks.OnTravelCommitted"))
        self.assertLess(body.index("Callbacks.OnTravelCommitted"),body.index("Controller->ClientTravel("))
        self.assertIn("TSharedFromThis<FGamePlatformUESessionTransport>",transport)
        self.assertIn("CallbackLifetime = AsShared()",transport)
        handshake=transport[transport.index("bool TickHandshake(float)"):
            transport.index("void HandleAdmissionAccepted(")]
        self.assertIn("ActiveOperationId != ProbeOperationId",handshake)
        self.assertIn("Component->SubmitAdmissionProof(MoveTemp(Proof))",handshake)

    def test_transport_fact_callback_survives_synchronous_complete_and_cancel(self):
        transport=source(SESSION/"Transport/GamePlatformUESessionTransport.cpp")
        # 四事实最后一项可能同步触发Session.CompleteTransfer -> ClearOperation，
        # 故不能直接在Callbacks.OnFact函数对象内释放Callbacks自身。
        self.assertIn("auto Prepared = MoveTemp(Callbacks.OnBindingPrepared)",transport)
        self.assertIn("auto Committed = MoveTemp(Callbacks.OnTravelCommitted)",transport)
        self.assertEqual(2,transport.count("auto FactCallback = MoveTemp(Callbacks.OnFact)"))
        self.assertEqual(2,transport.count("Callbacks.OnFact = MoveTemp(FactCallback)"))
        self.assertIn("CallbackLifetime = AsShared()",transport)
        self.assertIn("ConfirmedOperationId = ActiveOperationId",transport)
        self.assertNotIn("Callbacks.OnFact(",transport)
        self.assertIn("ActiveOperationId == ConfirmedOperationId",transport)

    def test_real_session_native_automation_exercises_sync_cancel_and_error(self):
        tests=source(SESSION/"Tests/GamePlatformSessionSubsystemTests.cpp")
        self.assertIn("bFailSynchronously",tests)
        self.assertIn("SessionImmediateTransportFailure",tests)
        self.assertIn("CancelBeforeTransport",tests)
        self.assertIn("Session->OnSessionChanged().AddLambda(",tests)
        self.assertIn("Test->TestFalse(TEXT(\"同步失败必须让BeginOperation返回false\")",tests)
        self.assertIn("Test->TestEqual(TEXT(\"同步取消委托只执行一次\")",tests)
        self.assertIn("旧同步回调不能恢复Ready",tests)
        self.assertIn("GamePlatform.Session.Subsystem.TransportRecovery",tests)

    def test_bird_flock_uses_profile_tick_interval_and_no_per_bird_actor(self):
        bird=source(VFX/"Environment/GamePlatformBirdFlockActor.cpp")
        profile=source(VFX/"Environment/GamePlatformBirdFlockProfile.cpp")
        tests=source(VFX/"Tests/GamePlatformBirdFlockTests.cpp")
        self.assertIn("SetActorTickInterval(1.0f / BirdProfile->UpdateRateHz)",bird)
        self.assertIn("UpdateAccumulatorSeconds = FMath::Fmod(",bird)
        self.assertIn("BirdInstances->UpdateInstanceTransform(",bird)
        self.assertNotIn("SpawnActor<",bird)
        self.assertIn("UpdateRateHz < 5.0f || UpdateRateHz > 60.0f",profile)
        self.assertIn("GetActorTickInterval()",tests)
        self.assertIn("动态调整物种10Hz更新频率",tests)
        self.assertIn("关闭鸟群立即释放实例",tests)

    def test_session_verification_uses_engine_cmake_tools(self):
        entry=source(ROOT/"Build/Validation/VerifySession.ps1")
        self.assertIn("Get-Command 'cmake.exe'",entry)
        self.assertIn("Get-Command 'ctest.exe'",entry)
        self.assertIn("Engine/Extras/ThirdPartyNotUE/CMake/bin",entry)
        self.assertIn("$exitCode = 2",entry)

    def test_telemetry_retry_ticker_handles_are_bounded(self):
        base=ROOT/"Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry"
        code=source(base/"Source/GamePlatformTelemetry/Private/Sinks/GamePlatformTelemetryNetworkSink.cpp")
        tests=source(base/"Source/GamePlatformTelemetry/Private/Tests/GamePlatformTelemetryCoreTests.cpp")
        self.assertIn("RetryTickerHandles.Remove(*HandleIdentity)",code)
        self.assertIn("*HandleIdentity = Handle;",code)
        self.assertIn('TEXT("retry_schedule_failed")',code)
        self.assertIn("Network.RetryHandleLifecycle",tests)
        self.assertIn("Sink->RetryTickerHandles.Num() <= Retry.MaxPendingBatches",tests)

    def test_telemetry_shutdown_is_idempotent_and_fails_closed(self):
        base=ROOT/"Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry"
        code=source(base/"Source/GamePlatformTelemetry/Private/Sinks/GamePlatformTelemetryNetworkSink.cpp")
        tests=source(base/"Source/GamePlatformTelemetry/Private/Tests/GamePlatformTelemetryCoreTests.cpp")
        region=code[code.index("void FGamePlatformTelemetryNetworkSink::Shutdown("):
                    code.index("void FGamePlatformTelemetryNetworkSink::FinalizeShutdownAfterBudget")]
        self.assertIn("if (bShuttingDown)",region)
        self.assertLess(region.index("bShuttingDown = true"),region.index("CancelRetryTickers();"))
        self.assertIn("if (!ShutdownTickerHandle.IsValid())",region)
        self.assertIn("Network.ShutdownIdempotent",tests)

    def test_save_storage_refuses_oversized_write_before_mutating_disk(self):
        # 本地存档在策略层之外仍要具备独立的字节限额与部分备份清理。
        # 只检查源码约束；真实UE自动化的随机文件读写需要独占UE环境再执行。
        base = ROOT / "Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private"
        storage = source(base / "Storage/GamePlatformSaveStorage.cpp")
        native = source(base / "Tests/GamePlatformSaveStorageTests.cpp")
        write = storage[storage.index("FGamePlatformResult FGamePlatformSaveStorage::WriteAtomically("):
                        storage.index("FGamePlatformResult FGamePlatformSaveStorage::DeleteRecordFiles(")]
        self.assertIn("Encoded.Num() > FGamePlatformSavePolicy::GetMaxEncodedBytes()", write)
        self.assertIn('TEXT("SaveFileTooLarge")', write)
        self.assertLess(write.index("Encoded.Num() >"), write.index("PlatformFile.CreateDirectoryTree"))
        copy_failed = write[write.index("if (!PlatformFile.CopyFile("):
                            write.index("if (!ReplaceByMove(BackupPath")]
        self.assertIn("PlatformFile.DeleteFile(*BackupTempPath)", copy_failed)
        self.assertIn("PlatformFile.DeleteFile(*TempPath)", copy_failed)
        self.assertIn("GamePlatform.Save.Storage.RejectsOversizedWrite", native)
        self.assertIn("Files.FileSize(*Primary)", native)
        self.assertIn("原有主档字节必须原样保留", native)

    def test_save_process_slot_is_exclusive_across_game_instances_until_worker_ends(self):
        # 本测试验证实现接线和原生用例覆盖；不是模拟磁盘上的并发IO通过。
        base = ROOT / "Game/Plugins/GamePlatform/Application/GamePlatformSave/Source/GamePlatformSaveClient/Private"
        lease = source(base / "Storage/GamePlatformSaveProcessSlotLease.h")
        subsystem = source(base / "Subsystems/GamePlatformSaveSubsystem.cpp")
        private_header = source(base / "Subsystems/GamePlatformSaveSubsystem.h")
        native = source(base / "Tests/GamePlatformSaveStorageTests.cpp")
        self.assertIn("FGamePlatformSaveProcessSlotLease::TryAcquire(StableStorageId)", subsystem)
        self.assertIn("Pending.ProcessSlotLease = MoveTemp(ProcessSlotLease)", subsystem)
        self.assertIn("TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe>", private_header)
        # 三条异步路径全部应把同一租约复制给工作线程，防止GI取消先释放真实IO锁。
        self.assertEqual(3, subsystem.count("PendingRequests.FindChecked(Handle.Id).ProcessSlotLease"))
        self.assertIn("[WeakThis, Handle, ProcessSlotLease, RecordCopy = Record]", subsystem)
        self.assertIn("[WeakThis, Handle, Key, StableStorageId, ProcessSlotLease]", subsystem)
        self.assertIn("[WeakThis, Handle, StableStorageId, ProcessSlotLease]", subsystem)
        self.assertEqual(3, subsystem.count("(void)ProcessSlotLease"))
        self.assertIn("FScopeLock Lock(&Mutex())", lease)
        self.assertIn("ActiveStorageIds().Contains(StableStorageId)", lease)
        self.assertIn("ActiveStorageIds().Remove(StableStorageId)", lease)
        self.assertIn("GamePlatform.Save.Storage.ProcessSlotLeaseLifecycle", native)
        self.assertIn("旧后台任务未结束前，取消回调不能提前释放进程锁", native)
        self.assertIn("旧后台IO结束后，新GameInstance可重新取得同槽位", native)

    def test_production_tick_sources_have_unregister_path_in_same_file(self):
        missing=[]
        count=0
        for cpp in (ROOT/"Game/Plugins").rglob("*.cpp"):
            if any(part in ("Intermediate","Binaries","Saved","Tests") for part in cpp.parts):
                continue
            text=cpp.read_text(encoding="utf-8-sig",errors="replace")
            if "AddTicker(" not in text:
                continue
            count += 1
            if "RemoveTicker(" not in text:
                missing.append(cpp.relative_to(ROOT).as_posix())
        self.assertGreaterEqual(count,15)
        self.assertEqual([],missing,"Ticker移除应具备可审查的同源清理路径")


if __name__=="__main__":
    unittest.main(verbosity=2)
