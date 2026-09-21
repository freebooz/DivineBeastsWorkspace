#if GAMEPLATFORM_LOADING_NATIVE_TEST
#include "../Operations/LoadingPolicy.h"
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace GamePlatformLoadingPolicy;
static int Checks = 0;
static void Require(bool Value) { ++Checks; if (!Value) { std::cerr << "failed assertion " << Checks << '\n'; std::exit(1); } }
static TaskSpec Task(std::string Id, Requirement Requiredness = Requirement::Required)
{ TaskSpec Spec; Spec.Id = std::move(Id); Spec.Requiredness = Requiredness; return Spec; }
int main()
{
    // 生产算法测试：进度不是完成，取消和迟到完成不能穿透代次。
    Operation Single;
    Require(Single.Start({Task("one")}, 1, 0, 30).empty());
    auto First = Single.Startable(0); Require(First.size() == 1);
    Require(Single.Report(First[0], 2)); Require(Single.Progress() == 1); Require(!Single.Ready());
    Require(Single.Report(First[0], -1)); Require(Single.Progress() == 1);
    Require(Single.Complete(First[0], true, "")); Require(Single.Ready());
    Require(!Single.Complete(First[0], false, "late"));
    Operation Multiple; auto SecondSpec = Task("two"); SecondSpec.Dependencies = {"one"};
    Require(Multiple.Start({Task("one"), SecondSpec}, 1, 0, 30).empty());
    auto Running = Multiple.Startable(0); Require(Running.size() == 1);
    Multiple.Complete(Running[0], true, ""); Require(!Multiple.Ready());
    Running = Multiple.Startable(1); Require(Running.size() == 1 && Running[0].Id == "two");
    Multiple.Complete(Running[0], true, ""); Require(Multiple.Ready());
    Operation Failed; Failed.Start({Task("one")}, 1, 0, 30);
    Failed.Complete(Failed.Startable(0)[0], false, "missing"); Require(Failed.State == OperationState::Failed);
    Operation Optional; Optional.Start({Task("one", Requirement::Optional)}, 1, 0, 30);
    Optional.Complete(Optional.Startable(0)[0], false, "optional missing");
    Require(Optional.Ready()); Require(Optional.Tasks[0].Error == "optional missing");
    auto FallbackSpec = Task("one", Requirement::Degradable); FallbackSpec.HasFallback = true;
    Operation Fallback; Fallback.Start({FallbackSpec}, 1, 0, 30);
    auto Original = Fallback.Startable(0)[0]; Fallback.Complete(Original, false, "primary failed"); Require(!Fallback.Ready());
    auto Replacement = Fallback.Startable(1)[0]; Require(Replacement.Generation != Original.Generation);
    Require(!Fallback.Complete(Original, true, "")); Fallback.Complete(Replacement, true, "");
    Require(Fallback.State == OperationState::DegradedReady);
    Operation FallbackFailed; FallbackFailed.Start({FallbackSpec}, 1, 0, 30);
    FallbackFailed.Complete(FallbackFailed.Startable(0)[0], false, "primary");
    FallbackFailed.Complete(FallbackFailed.Startable(0)[0], false, "fallback"); Require(FallbackFailed.State == OperationState::Failed);
    Operation Invalid; Require(!Invalid.Start({Task("one"),Task("one")}, 1, 0, 30).empty());
    SecondSpec.Dependencies = {"missing"}; Require(!Invalid.Start({SecondSpec},1,0,30).empty());
    auto Cyclic = Task("one"); Cyclic.Dependencies = {"two"}; SecondSpec.Dependencies = {"one"};
    Require(!Invalid.Start({Cyclic,SecondSpec},1,0,30).empty());
    auto BadWeight = Task("one"); BadWeight.Weight = 0; Require(!Invalid.Start({BadWeight},1,0,30).empty());
    BadWeight.Weight = std::numeric_limits<double>::infinity(); Require(!Invalid.Start({BadWeight},1,0,30).empty());
    Require(!Invalid.Start({Task("one",Requirement::Degradable)},1,0,30).empty());
    Operation Cancelled; Cancelled.Start({Task("one")},1,0,30); auto Late = Cancelled.Startable(0)[0];
    Cancelled.Cancel(); Require(Cancelled.State == OperationState::Cancelled); Require(!Cancelled.Complete(Late,true,""));
    Cancelled.Start({Task("one")},2,0,30); Require(!Cancelled.Complete(Late,true,""));
    auto NewToken = Cancelled.Startable(0)[0]; Cancelled.Expire(31); Require(Cancelled.State == OperationState::TimedOut);
    Require(!Cancelled.Complete(NewToken,true,""));
    Operation ScopeA, ScopeB; ScopeA.Start({Task("one")},1,0,30); ScopeB.Start({Task("one")},1,0,30);
    ScopeA.Cancel(); Require(ScopeB.State == OperationState::Running);
    auto ShortTask = Task("one"); ShortTask.TimeoutSeconds = 1;
    Operation Timeout; Timeout.Start({ShortTask},1,0,30); Timeout.Startable(0); Timeout.Expire(1);
    Require(Timeout.State == OperationState::Failed && Timeout.Tasks[0].Error == "TaskTimeout");
    Operation Weighted; auto Heavy = Task("two"); Heavy.Weight = 3;
    Weighted.Start({Task("one"),Heavy},1,0,30); auto Both = Weighted.Startable(0);
    Weighted.Report(Both[0],1); Require(Weighted.Progress() == 0.25);
    Require(!Weighted.Report(Both[1],std::numeric_limits<double>::quiet_NaN()));
    // 失败的可选依赖不能成为另一个必需任务的真实前置条件。
    Operation Blocked; Blocked.Start({Task("one",Requirement::Optional),SecondSpec},1,0,30);
    Blocked.Complete(Blocked.Startable(0)[0],false,"missing"); Blocked.Startable(1);
    Require(Blocked.State == OperationState::Failed);
    std::cout << Checks << " assertions passed\n";
}
#endif
