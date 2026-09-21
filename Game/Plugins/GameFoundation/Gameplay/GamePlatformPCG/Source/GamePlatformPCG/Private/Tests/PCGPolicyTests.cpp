#if defined(GAMEPLATFORM_PCG_NATIVE_TEST) && GAMEPLATFORM_PCG_NATIVE_TEST
#include "../Validation/PCGPolicy.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace GamePlatformPCGPolicy;
static int Checks = 0;
static void Require(bool Condition) { ++Checks; if (!Condition) { std::cerr << "failed " << Checks << '\n'; std::exit(1); } }
int main()
{
    NumericProfile Profile;
    Require(Validate(Profile).empty());
    Profile.HalfExtentCm = {0,100,100}; Require(Validate(Profile) == "InvalidBounds");
    Profile.HalfExtentCm = {100,100,100}; Profile.SpacingCm = -1; Require(Validate(Profile) == "InvalidSpacing");
    Profile.SpacingCm = 100; Profile.Scale = std::numeric_limits<double>::quiet_NaN(); Require(Validate(Profile) == "InvalidScale");
    Profile.Scale = 1; Profile.Density = -1; Require(Validate(Profile) == "InvalidDensity");
    Profile.Density = 1; Profile.HalfExtentCm = {100000,100,100}; Require(Validate(Profile) == "BoundsBudgetExceeded");
    Profile.HalfExtentCm = {100,100,100}; Profile.MaxOutputs = 0; Require(Validate(Profile) == "InvalidOutputBudget");
    Profile.MaxOutputs = 100; Profile.MinOutputs = 101; Require(Validate(Profile) == "InvalidOutputBudget");
    Profile.MinOutputs = 0; Profile.TimeoutSeconds = std::numeric_limits<double>::infinity(); Require(Validate(Profile) == "InvalidTimeout");
    Require(StableSeed("world@1","region@1","profile@1",7,11) == StableSeed("world@1","region@1","profile@1",7,11));
    Require(StableSeed("world@1","region@1","profile@1",7,11) != StableSeed("world@1","region@1","profile@1",8,11));
    Lifecycle Request;
    Require(Request.Begin(17)); Require(!Request.Begin(18)); Require(Request.Loaded(17));
    Require(!Request.Generated(16,true)); Require(Request.Generated(17,true));
    Require(Request.Outcome == Outcome::Succeeded && Request.Phase == Phase::Retained);
    Require(!Request.Generated(17,false)); Require(Request.BeginCleanup());
    Require(Request.Outcome == Outcome::Succeeded && Request.Phase == Phase::Cleaning);
    Require(!Request.Cleaned(16,true,true)); Require(!Request.Cleaned(17,false,true));
    Require(!Request.Cleaned(17,true,false)); Require(Request.Cleaned(17,true,true));
    Require(Request.Phase == Phase::Cleaned); Require(!Request.BeginCleanup());
    Lifecycle Cancelled; Cancelled.Begin(3); Cancelled.Stop(Outcome::Cancelled);
    Require(Cancelled.Phase == Phase::Cleaning && Cancelled.Outcome == Outcome::Cancelled);
    Require(!Cancelled.Generated(3,true)); Require(Cancelled.Cleaned(3,true,true));
    Lifecycle Timeout; Timeout.Begin(4); Timeout.Loaded(4); Timeout.Stop(Outcome::TimedOut);
    Require(!Timeout.Generated(4,true)); Require(Timeout.Outcome == Outcome::TimedOut);
    Lifecycle Failure; Failure.Begin(5); Failure.Loaded(5); Require(Failure.Generated(5,false));
    Require(Failure.Phase == Phase::Cleaning && Failure.Outcome == Outcome::Failed);
    Lifecycle Other; Other.Begin(1); Require(Other.Phase == Phase::Loading);
    Require(Cancelled.Phase == Phase::Cleaned);
    std::cout << Checks << " assertions passed\n";
}
#endif
