float t=SpellTime,id=(float)ParticleIndex;
float h=sin(id*127.1+13.7)*43758.54;float s=h-floor(h);
float h2=sin(id*331.7+79.1)*21563.34;float s2=h2-floor(h2);
float a=id*2.3999632;
float3 pos=0,scale=1;float2 size=10;float alpha=1,rot=t*45;
float4 q=float4(0,0,0,1);float3 col=float3(.4,.1,1);
if(Layer==0){size=float2(58,38);col=float3(.48,.40,1);alpha=.72;rot=t*180;}
if(Layer==1){float roll=t*4;q=float4(sin(roll*.5),0,0,cos(roll*.5));col=1;}
if(Layer==2){float f=t*2+s;float p=f-floor(f);pos=float3(-p*95,cos(a+t*6)*12,sin(a+t*6)*12);size=(1.5+s2*2.4).xx;alpha=(1-p)*.7;col=lerp(float3(.43,.08,1),float3(.45,.8,1),s2);}
if(Layer==10){float z=saturate(t/.65);pos=float3(cos(a),sin(a),sin(a*1.7))*(20+z*(80+75*s));size=float2(1.7+s*2,3+s2*5);alpha=pow(1-z,1.6);col=lerp(float3(.46,.06,1),float3(.55,.83,1),s2);}
if(Layer==11){float z=saturate(t/.5);size=(25+z*180).xx;alpha=pow(1-z,2);col=float3(.5,.23,1);rot=t*130;}
if(Layer==12){size=(90).xx;alpha=saturate(1-t/.12);col=float3(.65,.76,1);}
if(Layer==20){pos=float3(60,0,137);size=(83+sin(t*8)*4).xx;col=float3(.55,.18,1);rot=t*110;alpha=.65;}
if(Layer==21){float theta=a+t*3;pos=float3(60+cos(theta)*35,sin(theta)*38,137+sin(theta*1.3)*27);size=(2+s2*3).xx;col=float3(.37,.65,1);alpha=.8;}
Particles.Position=pos;Particles.SpriteSize=size;Particles.Color=float4(col,alpha*HeadAlpha);Particles.SpriteRotation=rot;Particles.Scale=scale*HeadAlpha;Particles.MeshOrientation=q;
