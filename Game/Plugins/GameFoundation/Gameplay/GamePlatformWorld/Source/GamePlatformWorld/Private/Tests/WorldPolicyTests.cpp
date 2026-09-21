// 此文件编译真实生产纯算法，不模拟UE对象、Session连接或资产加载。
#if defined(GAMEPLATFORM_WORLD_NATIVE_TESTS)
#include "Context/WorldPolicy.h"
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace GamePlatformWorldPolicy;
int main()
{
    int Count=0;
    auto Check=[&](bool Value,const char* Name){++Count;if(!Value){std::cerr<<Name<<"\n";std::exit(1);}};
    Check(SupportsWorld(true,false,false),"Game allowed");
    Check(SupportsWorld(false,true,false),"PIE allowed");
    Check(!SupportsWorld(false,false,false),"Editor preview denied");
    Check(!SupportsWorld(true,false,true),"Commandlet denied");
    Check(AcceptsGeneration("A","A",true,false),"current callback");
    Check(!AcceptsGeneration("A","B",true,false),"old callback");
    Check(!AcceptsGeneration("","",true,false),"empty generation");
    Check(!AcceptsGeneration("A","A",false,false),"dead world");
    Check(!AcceptsGeneration("A","A",true,true),"teardown callback");
    Check(AllowLocalDevelopment(true,false,false,false),"explicit offline development");
    Check(!AllowLocalDevelopment(false,false,false,false),"no implicit development");
    Check(!AllowLocalDevelopment(true,true,false,false),"shipping denied");
    Check(!AllowLocalDevelopment(true,false,true,false),"network client denied");
    Check(!AllowLocalDevelopment(true,false,false,true),"listen server denied");
    FReadinessFacts Facts;
    Check(!IsReady(Facts),"default is not ready");
    Facts={true,true,true,true,true,true,true,true};
    Check(IsReady(Facts),"all facts ready");
    for(int i=0;i<8;++i){auto Copy=Facts;Copy.Values()[i]=false;Check(!IsReady(Copy),"each required barrier");}
    FBox Large{{0,0,0},{10,10,10}},Small{{2,2,2},{4,4,4}};
    Check(Contains(Large,{0,0,0}),"inclusive lower boundary");
    Check(Contains(Large,{10,10,10}),"inclusive upper boundary");
    Check(!Contains(Large,{-1,0,0}),"outside region");
    Check(!Contains(Large,{std::numeric_limits<double>::quiet_NaN(),0,0}),"NaN rejected");
    Check(!ValidBox({{2,0,0},{1,1,1}}),"inverted box");
    Check(CompareCandidate(1,Large,1,Small)<0,"smaller specificity wins");
    Check(CompareCandidate(2,Large,1,Small)>0,"priority wins");
    Check(CompareCandidate(1,Large,1,Large)==0,"equal overlap explicitly ambiguous");
    FSessionTarget A{"world.a@1","/Game/Map","server-a","epoch-a","build-1"};
    auto B=A;
    Check(MatchesTarget(A,B),"exact target matches");
    B.Instance="server-b";Check(!MatchesTarget(A,B),"wrong instance");
    B=A;B.World="world.b@1";Check(!MatchesTarget(A,B),"wrong world");
    B=A;B.Map="/Game/Other";Check(!MatchesTarget(A,B),"wrong map");
    B=A;B.Epoch="epoch-b";Check(!MatchesTarget(A,B),"old epoch");
    B=A;B.Build="build-2";Check(!MatchesTarget(A,B),"build mismatch");
    B=A;B.Instance.clear();Check(!MatchesTarget(B,B),"empty instance not authority");
    std::cout<<Count<<" world policy checks passed\n";
}
#endif
