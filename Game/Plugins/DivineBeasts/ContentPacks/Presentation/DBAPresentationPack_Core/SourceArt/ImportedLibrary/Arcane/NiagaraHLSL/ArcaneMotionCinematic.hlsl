float t=SpellTime,id=(float)ParticleIndex;
float h=sin(id*127.1+13.7)*43758.54;float s=h-floor(h);
float h2=sin(id*331.7+79.1)*21563.34;float s2=h2-floor(h2);
float a=id*2.3999632;
float3 pos=0,scale=1;float2 size=10;float alpha=1,rot=0;
float4 q=float4(0,0,0,1);float3 col=1;float3 alignment=float3(1,0,0);
if(Layer==0){size=float2(56,56);col=float3(.25,.15,1);alpha=.32;rot=t*180;}
if(Layer==1){pos=float3(-57,0,0);size=float2(190,235)*(1+.035*sin(t*18));alpha=.9;}
if(Layer==2){float f=t*2+s;float p=f-floor(f);pos=float3(-p*145,cos(a+t*7)*19,sin(a+t*7)*19);size=(2+s2*3).xx;alpha=(1-p)*.7;col=lerp(float3(.43,.08,1),float3(.45,.8,1),s2);}
if(Layer==10){float z=saturate(t/.75);pos=float3(cos(a),sin(a),sin(a*1.7))*(15+z*(120+90*s));size=float2(2+s*3,6+s2*9);alpha=pow(1-z,1.6);col=lerp(float3(.46,.06,1),float3(.55,.83,1),s2);rot=a*57.2958;}
if(Layer==11){float z=saturate(t/.65);size=(50+z*265).xx;alpha=pow(1-z,1.8)*.75;col=float3(.5,.23,1);rot=t*95;}
if(Layer==12){size=(135).xx;alpha=saturate(1-t/.16)*.7;col=float3(.4,.38,1);}
if(Layer==13){float z=saturate(t/.8);pos=float3(cos(a)*45*z,sin(a)*45*z,18+z*(45+s*65));size=float2(170,190)*(0.45+z*.85);alpha=pow(1-z,1.3)*.72;rot=a*57.2958+t*(70+s*60);}
if(Layer==20){pos=float3(60,0,137);size=(83+sin(t*8)*4).xx;col=float3(.55,.18,1);rot=t*110;alpha=.6;}
if(Layer==21){float theta=a+t*3;pos=float3(60+cos(theta)*35,sin(theta)*38,137+sin(theta*1.3)*27);size=(2+s2*3).xx;col=float3(.37,.65,1);alpha=.8;}
if(Layer==22){pos=float3(60,0,145);size=float2(135,125);alpha=.62;rot=t*85;}
Particles.Position=pos;Particles.SpriteSize=size;Particles.Color=float4(col,alpha*HeadAlpha);Particles.SpriteRotation=rot;Particles.SpriteAlignment=alignment;Particles.Scale=scale*HeadAlpha;Particles.MeshOrientation=q;
