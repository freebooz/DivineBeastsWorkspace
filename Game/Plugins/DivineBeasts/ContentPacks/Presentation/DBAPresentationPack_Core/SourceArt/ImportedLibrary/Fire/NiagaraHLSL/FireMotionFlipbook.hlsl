// Analytic motion: one bounded burst, no collisions, no persistent spawn rates.
float t=SpellTime;
float id=(float)ParticleIndex;
float h=sin(id*127.1+37.7)*43758.5453;float s=h-floor(h);
float h2=sin(id*311.7+81.9)*21563.3431;float s2=h2-floor(h2);
float h3=sin(id*77.13+27.4)*19413.78;float s3=h3-floor(h3);
float a=id*2.39996323;
float3 dir=float3(cos(a),sin(a),0);
float3 pos=0,sc=1;float2 size=10;float4 q=float4(0,0,0,1);
float3 color=float3(1,.33,.035);float alpha=1;float rot=id*137.5;
float phase=t*(.85+s*.3)+s2;float p=phase-floor(phase);
if(Layer==0){ // projectile heart: independent fire lobes
 pos=float3(15+s*20,cos(a+t*3)*12,sin(a+t*3)*12);size=float2(72,91)*( .8+.3*s2);rot+=t*(id<2?75:-50);alpha=.62;color=1;
}
if(Layer==1){ // longitudinal curved flame envelope
 pos=float3(-55,0,0);size=float2(130,235)*(1+.04*sin(t*13));rot=0;color=1;alpha=.9;
}
if(Layer==2){pos=float3(-25-p*270,cos(a+t)*p*43,sin(a+t)*p*43);size=(1.7+s3*2.7).xx;alpha=saturate(p*18)*pow(1-p,1.8);color=lerp(float3(1,.055,.005),float3(1,.7,.13),s2);}
if(Layer==3){pos=float3(-55-p*175,cos(a)*28,sin(a)*28);size=(25+p*40).xx;alpha=sin(p*3.14159)*.22;color=float3(.4,.15,.07);rot+=t*16;}
if(Layer==10){pos=float3(65+cos(a+t*4)*10,sin(a+t*4)*17,135+cos(a)*11);size=(32+s2*18+min(t,1.6)*12).xx;rot+=t*85;color=1;alpha=.4+saturate(t/1.8)*.35;}
if(Layer==11){float rad=50*(1-p);pos=float3(65+cos(a+t*2)*rad,sin(a+t*2)*rad,135+sin(a*.7)*rad);size=(2+s*2).xx;alpha=saturate(p*10)*saturate((1-p)*12);color=float3(1,.5,.08);}
if(Layer==20){float z=saturate(t/1.15);pos=dir*(22+z*(120+95*s))+float3(0,0,20+z*(65+130*s2));size=(82+s3*55)*(1+z*.6);rot+=t*(s2-.5)*120;alpha=pow(1-z,1.8)*.85;color=1;}
if(Layer==21){float z=saturate(t/(.65+s*1.05));pos=dir*((80+250*s2)*z)+float3(0,0,30+(120+170*s3)*z-180*z*z);size=float2(1.8+s*2,4+s*6);alpha=pow(1-z,1.4);color=lerp(float3(1,.09,.003),float3(1,.9,.25),s3);rot=a*57.2958;}
if(Layer==22){float z=saturate(t/2.0);pos=dir*(30+z*145)+float3(0,0,40+z*160);size=(60+z*90).xx;alpha=sin(z*3.14159)*.33;color=float3(.18,.10,.065);rot+=t*12;}
if(Layer==23){float z=saturate(t/.6);size=(60+z*380).xx;pos.z=-65;alpha=pow(1-z,2);color=float3(1,.38,.035);rot=t*30;}
if(Layer==24){size=(160).xx;alpha=saturate(1-t/.14)*.7;color=float3(1,.73,.25);}
if(Layer==30){size=(610).xx;pos.z=6;rot=t*7;alpha=saturate(t*10)*saturate((6.2-t)/1.3)*(.65+.08*sin(t*9));color=float3(1,.11,.005);}
if(Layer==31){size=(540+min(t,.8)*75).xx;pos.z=9;rot=-t*20;alpha=saturate(1-t/1.7);color=float3(1,.55,.085);}
if(Layer==32){float yaw=t*.25;q=float4(0,0,sin(yaw*.5),cos(yaw*.5));alpha=saturate(t*8)*saturate((4.8-t)/1.6)*.26;color=1;sc=float3(1.14,1.14,.85+.09*sin(t*8));}
if(Layer==33){float life=saturate((5.4-t)/1.8);float rad=35+190*s2;float theta=a+t*.28;pos=float3(cos(theta)*rad,sin(theta)*rad,40+p*160);size=float2(110+s3*65,180+s3*115)*( .6+.5*sin(p*3.14159));rot=sin(t*2+a)*12;alpha=sin(p*3.14159)*life*.85;color=1;}
if(Layer==34){float life=saturate((6.0-t)/1.5);pos=dir*(35+240*s2)+float3(sin(t*1.2+a)*20,cos(t*1.1+a)*20,8+p*(250+s3*140));size=float2(1.8+s*2,3+s*4);alpha=saturate(p*18)*pow(1-p,.7)*life;color=lerp(float3(1,.07,.003),float3(1,.72,.12),s3);}
if(Layer==35){pos=dir*(60+s*170)+float3(0,0,70+p*150);size=(65+p*85).xx;alpha=sin(p*3.14159)*saturate((5.5-t)/2)*.22;color=float3(.2,.1,.06);rot+=t*10;}
if(Layer==36){pos=dir*(s2*65)+float3(0,0,850*(1-saturate(t/.32)));size=float2(90,280);alpha=saturate(1-t/.44);color=1;}
if(Layer==40){pos.z=6;size=(600).xx;rot=t*18;alpha=.30+saturate(t/2)*.4;color=float3(1,.19,.012);}
if(Layer==41){pos=dir*(250+6*sin(t*4+a))+float3(0,0,7);size=(2.4+s3*2).xx;alpha=.6;color=float3(1,.62,.10);}
Particles.Position=pos;Particles.Color=float4(color,alpha);Particles.SpriteSize=size;
Particles.SpriteRotation=rot;Particles.Scale=sc;Particles.MeshOrientation=q;
Particles.SpriteFacing=float3(0,0,1);Particles.SpriteAlignment=Layer==1?float3(-1,0,0):float3(1,0,0);

float frame=t*(12+s*6)+s2*16;
Particles.SubImageIndex=frame-16*floor(frame/16);
