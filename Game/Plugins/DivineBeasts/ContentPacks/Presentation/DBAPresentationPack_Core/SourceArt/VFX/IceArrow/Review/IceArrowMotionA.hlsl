// SSLM 项目表现层候选运动真值快照。仅通过 Monolith 导入 Niagara Update；不处理伤害或碰撞。
#if GPU_SIMULATION
// 全部位置均为局部厘米，复用真实Actor/根组件的运动；不会替代权威弹道。
float a=max(0.0,Age); int i=Index % 128; float q=i*2.399963+a*1.7;
float fade=saturate(a/0.065)*saturate((Life-a)/0.18);
float3 position=float3(0,0,0); float3 scale=float3(1,1,1);
float4 rotation=float4(0,0,0,1); float2 size=float2(38,38);
float3 tint=float3(0.50,0.85,1.15); float opacity=fade;
// 施法以脚底锚点为原点：地面内外环反向旋转，冰核在角色前方凝聚。
if(Mode==0 || Mode==1){
 float turn=a*(Mode==0?0.33:-0.52); position=float3(0,0,2+Mode*0.8);
 scale=float3(2.65,2.65,1)*min(1.0,0.7+a*3.0);rotation=float4(0,0,sin(turn*.5),cos(turn*.5));
 opacity=fade*(Mode==0?0.80:0.95);
}
else if(Mode==2){position=float3(38,0,104+sin(a*4)*3);scale=float3(.22,.22,.45)*(0.3+saturate(a/.55)*.7);rotation=float4(0,sin(.785398),0,cos(.785398));}
else if(Mode==3){float r=52-saturate(a/.75)*25;position=float3(25+cos(q)*r,sin(q)*r,86+sin(q*.8)*23);scale=float3(.25,.25,.12)*fade;rotation=normalize(float4(sin(q*.5)*.25,cos(q*.5)*.25,0,1));}
else if(Mode==4){float r=25+(i%24)*2.4-saturate(a)*20;position=float3(18+cos(q)*r,sin(q)*r,15+((i*17)%95)+a*20);size=float2(3.5+(i%4),3.5+(i%4));opacity*=.60;}
else if(Mode==5){position=float3(42,0,103);size=float2(56,56)*(0.2+saturate(a/.55));opacity*=.40;}
// 飞行：自定义晶体箭身、三个弯曲流带、寒雾与碎晶组成可读体积，不用基础圆锥。
else if(Mode==10){scale=float3(.95,1.1,1.1);position=float3(0,0,0);opacity=fade;}
else if(Mode==15){position=float3(-3,0,0);scale=float3(.50,.25,.25);opacity=.8*fade;tint=float3(1.0,1.1,1.3);}
else if(Mode==16){float ang=(i % 5)*1.256637;position=float3(-32+(i % 2)*8,cos(ang)*19,sin(ang)*15);scale=float3(.4,.5,.45);rotation=float4(0,-.7071068,0,.7071068);opacity=fade;}
else if(Mode==11){float t=(i%3)*2.094395+a*.75;rotation=float4(sin(t*.5),0,0,cos(t*.5));position=float3(-25,0,0);scale=float3(1,1,1);opacity=.45*fade;}
else if(Mode==12){float f=(i%18)/17.0;position=float3(-20-f*210,cos(q)*12*(.25+f),sin(q)*12*(.25+f));size=float2(40+f*26,36+f*30);opacity*=.11*(1-f);}
else if(Mode==13){float f=(i%14)/13.0;position=float3(-15-f*175,cos(q)*18,sin(q)*18);scale=float3(.18,.18,.10);rotation=normalize(float4(.2,sin(q),cos(q),1));opacity*=.65;}
else if(Mode==14){size=float2(95,95)*(1+a*3);opacity=saturate(1-a/.14);}
// 命中：极短白芯→外推碎晶→短寿命冰冠→寒雾。局部范围只表达单次命中的冲量。
else if(Mode==20){size=float2(165,165)*(0.45+a*2);opacity=saturate(1-a/.14);tint=float3(1.3,1.5,1.7);}
else if(Mode==21){float r=15+a*190;position=float3(cos(q)*r,sin(q)*r,15+a*(110+(i%5)*12)-145*a*a);scale=float3(.08,.08,.21)*saturate((Life-a)/.30);rotation=normalize(float4(sin(q),cos(q),sin(q*.7),1));}
else if(Mode==22){position=float3(0,0,-18);float grow=saturate(a/.13);scale=float3(.8,.8,.72)*grow;opacity=saturate((Life-a)/.38);}
else if(Mode==23){float r=12+a*78;position=float3(cos(q)*r,sin(q)*r,8+a*20);size=float2(88,88)*(0.45+a*.75);opacity=fade*.19;}
// A 方案：竖直施法法阵、凝聚箭身、法线方向冲击环，仍为纯客户端外观。
else if(Mode==30){float s=(.7+.3*saturate(a/.35))*1.65;position=float3(-24,0,104);scale=float3(s,s,1);rotation=float4(0,.7071068,0,.7071068);opacity=fade*.6;}
else if(Mode==31){position=float3(38,0,104);scale=float3(.65,.65,.65)*(.15+.85*saturate(a/.65));opacity=fade;}
else if(Mode==32){position=float3(38,0,104);scale=float3(.33,.14,.14)*(.15+.85*saturate(a/.65));opacity=fade*.8;tint=float3(1,1.1,1.3);}
else if(Mode==40){float s=.25+2.0*saturate(a/.28);position=float3(-2,0,0);scale=float3(s,s,1);rotation=float4(0,.7071068,0,.7071068);opacity=pow(saturate(1-a/Life),1.4);}
else if(Mode==41){float angle=i*2.399963;float r=12+a*(160+(i % 4)*25);position=float3(-a*(80+(i % 3)*35),cos(angle)*r,sin(angle)*r-70*a*a);scale=float3(.45,.45,.28)*(.75+(i % 4)*.15)*saturate((Life-a)/.25);rotation=normalize(float4(sin(q),cos(q),sin(q*.7),1));opacity=fade;}
else if(Mode==42){float r=10+a*80;position=float3(-8-a*25,cos(q)*r,sin(q)*r);size=float2(70,70)*(0.5+a);opacity=fade*.25;}
else if(Mode==43){size=float2(85,85)*(0.4+a*3);opacity=saturate(1-a/.12);tint=float3(1.2,1.4,1.6);}
// 地面残霜由独立向下静态地形查询放置，不把漂浮命中点伪装为地面。
else if(Mode==24){position=float3(0,0,2);scale=float3(2.8,2.8,1)*(0.18+saturate(a/.28)*.82);opacity=fade*.60;}
else if(Mode==25){position=float3(0,0,3);float turn=-a*.28;rotation=float4(0,0,sin(turn*.5),cos(turn*.5));scale=float3(2.3,2.3,1)*(0.3+saturate(a/.18)*.7);opacity=fade*.55;}
Particles.Position=position;Particles.Scale=scale;Particles.MeshOrientation=rotation;
Particles.SpriteSize=size;Particles.SpriteRotation=(i*137.5)+Age*27;
Particles.Color=float4(tint,opacity);
#endif
