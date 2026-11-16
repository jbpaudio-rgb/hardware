// ===================== DSP (AudioWorklet): the effect rack =====================
const WORKLET=`
const CT=32,PI=Math.PI,TP=2*Math.PI;
const lfoHz=k=>.01*Math.pow(3000,k);
const NOTEB=[.125,1/6,.25,1/3,.375,.5,2/3,.75,1,4/3,1.5,2,4];
const cl=(x,a,b)=>x<a?a:x>b?b:x;
const hann=t=>.5-.5*Math.cos(TP*t);
// delay line with linear interpolated reads
class DL{constructor(n){this.b=new Float32Array(n);this.n=n;this.w=0}
 put(x){this.b[this.w]=x;this.w++;if(this.w>=this.n)this.w=0}
 get(d){d=cl(d,1,this.n-2);let p=this.w-d;while(p<0)p+=this.n;if(p>=this.n)p-=this.n;const i=Math.floor(p),f=p-i;const a=this.b[i],b=this.b[i+1>=this.n?0:i+1];return a+(b-a)*f}
 at(i){i%=this.n;if(i<0)i+=this.n;return this.b[i]}}
class Comb{constructor(n){this.b=new Float32Array(n);this.i=0;this.s=0}
 run(x,fb,damp){const y=this.b[this.i];this.s=y*(1-damp)+this.s*damp;this.b[this.i]=x+this.s*fb;if(++this.i>=this.b.length)this.i=0;return y}}
class AP{constructor(n){this.b=new Float32Array(n);this.i=0}
 run(x){const y=this.b[this.i];this.b[this.i]=x+y*.5;if(++this.i>=this.b.length)this.i=0;return y-x}}
// a stereo Freeverb-style tank
class Tank{constructor(fs,nc){const k=fs/44100,CL=[1116,1188,1277,1356,1422,1491,1557,1617].slice(0,nc),AL=[556,441,341,225];
  this.c=[CL.map(n=>new Comb(Math.round(n*k))),CL.map(n=>new Comb(Math.round((n+23)*k)))];this.a=[AL.map(n=>new AP(Math.round(n*k))),AL.map(n=>new AP(Math.round((n+23)*k)))];this.g=.015*8/nc}
 run(l,r,fb,damp,out){const x=(l+r)*this.g;for(let ch=0;ch<2;ch++){let y=0;for(const c of this.c[ch])y+=c.run(x,fb,damp);for(const a of this.a[ch])y=a.run(y);out[ch]=y}}}
// delay-line pitch shifter: two crossfaded heads
class Shift{constructor(n){this.d=new DL(n);this.ph=0}
 // hard splice (edge = short crossfade), like an early harmonizer without de-glitching
 runEdge(x,ratio,win,e){this.d.put(x);this.ph+=(1-ratio)/win;this.ph-=Math.floor(this.ph);const p2=(this.ph+.5)%1;const g=t=>Math.min(1,t/e,(1-t)/e);const a=g(this.ph),b=g(p2);return(this.d.get(1+this.ph*win)*a+this.d.get(1+p2*win)*b)/Math.max(.5,a+b)}
 run(x,ratio,win){this.d.put(x);this.ph+=(1-ratio)/win;this.ph-=Math.floor(this.ph);const p2=(this.ph+.5)%1;return this.d.get(1+this.ph*win)*hann(this.ph)+this.d.get(1+p2*win)*hann(p2)}}
// one-pole low / high split used for shelving tone controls
class OP{constructor(){this.s=0}lp(x,a){this.s+=a*(x-this.s);return this.s}}
// TPT state-variable filter
class SVF{constructor(){this.a=0;this.b=0}run(x,g,k){const h=(x-(k+g)*this.a-this.b)/(1+k*g+g*g);const bp=g*h+this.a;this.a=g*h+bp;const lp=g*bp+this.b;this.b=g*bp+lp;this.lpo=lp;this.bpo=bp;this.hpo=h;return lp}}
const dbg=db=>Math.pow(10,db/20);
// three overdrive models, after well-known boxes: a tube distortion with bias (VULTURE), a parallel triode + pentode saturator with air (HG-2), a drive + compressor + filter + gate unit (BOUM)
function triode(u){return u>=0?Math.tanh(u):Math.tanh(u*.55)/.55*.72}
// bucket-brigade (BBD) line: a delay with its anti-alias and reconstruction low passes and a soft limit
class BBD{constructor(n){this.d=new DL(n);this.a=0;this.b=0;this.c=0}
 run(x,del,ap){this.a+=ap*(x-this.a);this.d.put(Math.tanh(this.a*1.1)/1.1);const y=this.d.get(del);this.b+=ap*(y-this.b);this.c+=ap*(this.b-this.c);return this.c}}
// ---------- reverb ----------
// 8-line feedback delay network: input diffusion, per-line low / high decay, air damping, slow modulation, Hadamard mixing
class FDN{constructor(fs){this.fs=fs;this.d=[];for(let k=0;k<8;k++)this.d.push(new DL(Math.round(fs*.22)));this.lo=new Float32Array(8);this.hs=new Float32Array(8);this.v=new Float32Array(8);this.y=new Float32Array(8);
  this.len=new Float32Array(8);this.gL=new Float32Array(8);this.gH=new Float32Array(8);this.ph=Float32Array.from([0,.13,.29,.41,.55,.67,.79,.91]);
  this.ap=[0,1].map(ch=>(ch?[5.3,4.1,11.3,8.1]:[4.7,3.6,12.7,9.3]).map(ms=>new DL(Math.round(fs*ms/1000))))}
 set(size,rt,lowMul,dampHz,diff,mod){const fs=this.fs,B=[29.7,37.1,41.1,43.7,53.3,59.9,67.7,73.1];
  for(let k=0;k<8;k++){const n=cl(B[k]*size*fs/1000,16,fs*.21);this.len[k]=n;this.gH[k]=Math.pow(10,-3*n/(rt*fs));this.gL[k]=Math.pow(10,-3*n/(rt*lowMul*fs))}
  this.aX=1-Math.exp(-TP*250/fs);this.aD=1-Math.exp(-TP*Math.min(dampHz,fs*.45)/fs);this.diff=diff;this.md=mod*.0012*fs;this.mr=.4/fs}
 run(il,ir,out){const c=this.diff;const xs=[il,ir];
  for(let ch=0;ch<2;ch++){let x=xs[ch];const A=this.ap[ch];for(let k=0;k<4;k++){const dl=A[k];const z=dl.b[dl.w];const v=x+c*z;dl.put(v);x=z-c*v}xs[ch]=x}
  const y=this.y,v=this.v;for(let k=0;k<8;k++){this.ph[k]+=this.mr*(1+k*.13);if(this.ph[k]>=1)this.ph[k]-=1;const s=this.md?this.d[k].get(this.len[k]+this.md*(1+Math.sin(TP*this.ph[k]))):this.d[k].get(this.len[k]);y[k]=s;
   this.lo[k]+=this.aX*(s-this.lo[k]);let w=this.lo[k]*this.gL[k]+(s-this.lo[k])*this.gH[k];this.hs[k]+=this.aD*(w-this.hs[k]);v[k]=this.hs[k]}
  for(let h=1;h<8;h<<=1)for(let i=0;i<8;i+=h<<1)for(let j=i;j<i+h;j++){const a=v[j],b=v[j+h];v[j]=a+b;v[j+h]=a-b}
  const nr=.35355;for(let k=0;k<8;k++)this.d[k].put(v[k]*nr+(k&1?xs[1]:xs[0])*.5);
  out[0]=(y[0]-y[2]+y[4]-y[6]+y[1]*.3)*.45;out[1]=(y[1]-y[3]+y[5]-y[7]+y[0]*.3)*.45}}
// spring tank: each spring is a chain of dispersive allpasses (the chirp and drip) in a feedback loop with its delay
class Spring{constructor(fs,ms,stages){this.ap=new Float32Array(stages);this.d=new DL(Math.round(fs*ms/1000)+4);this.n=Math.round(fs*ms/1000);this.lp=0;this.dc=0;this.fb=0}
 run(x,c,g,a,wob){let y=x+this.fb;const st=this.ap;for(let k=0;k<st.length;k++){const z=c*y+st[k];st[k]=y-c*z;y=z}
  this.d.put(y);let o=this.d.get(this.n*(1+wob));this.lp+=a*(o-this.lp);o=this.lp;this.dc+=.002*(o-this.dc);o-=this.dc;this.fb=Math.tanh(o*g*1.2)/1.2;return o}}
// the models: kind 0 = network (digital, chamber, plate, room), kind 1 = springs; er = early reflections [ms, gain, pan]
const RVMOD=[
 {k:0,size:1.4,damp:9000,low:1.2,diff:.7,mod:1.0,bw:16000,hp:30,er:[[8,.25,-.4],[14,.2,.4],[21,.15,-.2]]},             // 480L: lush random hall
 {k:0,size:1.1,damp:6000,low:1.3,diff:.6,mod:.3,bw:12000,hp:40,bits:12,er:[[11,.2,.3],[19,.15,-.3]]},                // EMT 250: dark, band-limited, a little grain
 {k:0,size:1.0,damp:11000,low:1.0,diff:.78,mod:.5,bw:15000,hp:40,er:[[6,.3,-.3],[11,.25,.4],[17,.2,-.5],[24,.15,.3]]}, // RMX 16: bright and dense
 {k:0,size:1.3,damp:12500,low:1.1,diff:.72,mod:.6,bw:20000,hp:25,er:[[9,.28,-.5],[15,.24,.5],[22,.2,-.3],[31,.16,.4],[40,.12,-.2]]}, // M7: smooth and natural
 {k:1,sp:[33,39,46,52],st:6,c:.5,lp:9000,dr:.4,diff:true},                                                          // BX20: dense, smooth springs
 {k:1,sp:[36,41,47],st:10,c:.6,lp:7000,dr:.5},                                                                       // XL-305
 {k:1,sp:[33,41],st:14,c:.68,lp:5500,dr:1.0},                                                                        // SUPER REVERB: amp tank, drippy
 {k:1,sp:[35,43],st:12,c:.62,lp:4500,dr:.7,wob:.6},                                                                  // EF-P2: darker, a little wobble
 {k:1,sp:[31,38,44],st:14,c:.66,lp:7500,dr:.6},                                                                      // FLINT
 {k:1,sp:[29,37],st:10,c:.6,lp:5000,dr:.8,wob:1.2,lofi:true},                                                        // PLACE: lo-fi, wobbly
 {k:0,size:1.25,damp:7500,low:1.2,diff:.55,mod:.15,bw:9000,hp:120,er:[[11,.6,-.4],[19,.5,.5],[27,.45,-.2],[38,.4,.3],[47,.3,-.5],[61,.25,.6]],rt:1.1}, // CAPITOL chamber
 {k:0,size:1.05,damp:9500,low:1.0,diff:.55,mod:.15,bw:10000,hp:120,er:[[9,.55,.3],[16,.5,-.5],[24,.45,.4],[33,.38,-.2],[44,.3,.6],[57,.25,-.6]],rt:1.0}, // ABBEY ROAD chamber
 {k:0,size:.9,damp:6500,low:1.15,diff:.55,mod:.15,bw:8500,hp:120,er:[[7,.6,-.3],[13,.5,.4],[21,.4,-.5],[30,.35,.5],[41,.3,-.2]],rt:.85},       // SUNSET SOUND chamber
 {k:0,size:.7,damp:8500,low:1.0,diff:.5,mod:.1,bw:9500,hp:150,er:[[5,.65,.3],[9,.55,-.4],[15,.45,.5],[22,.4,-.3],[30,.3,.2]],rt:.7},           // GOLD STAR chamber
 {k:0,size:.7,damp:7000,low:.8,diff:.82,mod:.15,bw:18000,hp:80,plate:true},   // EMT 140
 {k:0,size:.45,damp:11000,low:.7,diff:.84,mod:.1,bw:20000,hp:100,plate:true}, // EMT 240
 {k:0,size:.65,damp:12000,low:.85,diff:.86,mod:.15,bw:20000,hp:70,plate:true},// ECOPLATE III
 {k:0,size:.6,damp:9000,low:.8,diff:.65,mod:.05,bw:16000,hp:90,plate:true},   // RX4000: more metallic
 {k:0,size:.75,damp:6000,low:.95,diff:.8,mod:.12,bw:14000,hp:60,plate:true},  // AUDICON: warm
 {k:0,size:.95,damp:8000,low:1.1,diff:.5,mod:.1,bw:16000,hp:40,er:[[14,.55,-.5],[23,.5,.5],[31,.4,-.2],[42,.35,.4],[55,.3,-.6],[68,.22,.6]],rt:1.0},  // STUDIO 2
 {k:0,size:.5,damp:6500,low:1.0,diff:.45,mod:.05,bw:14000,hp:60,er:[[4,.6,.2],[8,.55,-.4],[12,.45,.5],[17,.4,-.2],[23,.3,.4]],rt:.6},             // SUN STUDIO
 {k:0,size:.7,damp:7000,low:1.2,diff:.5,mod:.08,bw:14000,hp:40,er:[[7,.55,-.3],[12,.5,.4],[19,.4,-.5],[26,.35,.3],[35,.25,-.2]],rt:.8},           // ELECTRIC LADY A
 {k:0,size:.85,damp:8500,low:1.1,diff:.5,mod:.08,bw:16000,hp:40,er:[[10,.55,.4],[17,.5,-.4],[25,.42,.5],[34,.35,-.5],[46,.28,.3],[58,.2,-.3]],rt:.9}]; // CAPITOL A
// Hilbert pair (two allpass chains 90 degrees apart) for frequency shifting
class Hilbert{constructor(){this.a=[.6923878,.9360654322959,.988229522686,.9987488452737];this.b=[.4021921162426,.856171088242,.9722909545651,.9952884791278];this.sa=new Float32Array(16);this.sb=new Float32Array(16);this.d=0}
 chain(x,c,s){for(let k=0;k<4;k++){const y=c[k]*(x+s[k*4+3])-s[k*4+1];s[k*4+1]=s[k*4];s[k*4]=x;s[k*4+3]=s[k*4+2];s[k*4+2]=y;x=y}return x}
 run(x){const re=this.chain(x,this.a,this.sa);const im0=this.chain(x,this.b,this.sb);const im=this.d;this.d=im0;this.re=re;this.im=im}}
class FXRack extends AudioWorkletProcessor{
 constructor(o){super();const a=o.processorOptions;this.I=a.IDX;this.D=a.D;this.DSTP=a.DSTP;this.p=new Float32Array(a.P);
  this.fs=sampleRate;this.Iq=[0,1,2,3,4,5,6,7].map(q=>{const o=Object.assign({},this.I),pf='q'+q;for(const k in this.I)if(k.startsWith(pf+'f'))o[k.slice(2)]=this.I[k];return o});this.initState();
  this.port.onmessage=e=>{const m=e.data;if(m.t==='p')this.p.set(m.p);else if(m.t==='perm'){this.p.set(m.p);const U=m.map.map(k=>this.units[k]),F=m.map.map(k=>this.slotF[k]),S=m.map.map(k=>this.mixS[k]);U.forEach((u,q)=>{if(u)u.I=this.Iq[q]});this.units=U;this.slotF=F;this.mixS=Float32Array.from(S)}else if(m.t==='on')this.noteOn(m.n,m.v||.85);else if(m.t==='off')this.noteOff(m.n);else if(m.t==='alloff')for(const v of this.voices)v.g=false}}
 // all the sound state (delay lines, filters, grains...), rebuilt from scratch if a NaN ever gets in
 initState(){const fs=this.fs;
  this.off=new Float32Array(this.p.length);this.src=new Float32Array(14);this.v=new Float32Array(this.p.length);
  this.lp=[0,.33,.66,.5];this.lr=[0,0,0,0];this.shp=0;this.sht=0;this.shv=0;this.sh2p=0;this.sh2t=0;this.sh2v=0;this.clock=0;this.step=0;this.rs=99991;this.cc=0;this.mc=0;
  this.held=[];this.ms=0;this.env=0;this.mixS=new Float32Array(8);this.mixT=new Float32Array(8);this.units=[null,null,null,null,null,null,null,null];this.slotF=[-1,-1,-1,-1,-1,-1,-1,-1];this.active=[];this.o=[0,0];this.t2=[0,0];
  // built-in sources
  this.dv={ph:0,f:110,e:0,lp1:0,lp2:0,kp:0,ke:0,hn:0,he:0,hp:0};this.voices=[];for(let i=0;i<6;i++)this.voices.push({n:-1,ph:0,f:0,e:0,g:false,lp1:0,lp2:0,age:0});this.vage=0;
 }
 // the state of one effect (delay lines, filters, grains...), made for a slot instance when an effect goes into its slot
 initFx(f){const fs=this.fs;switch(f){
  case 16:this.scE=0;this.scR=false;this.scSt=-1;this.scK=0;break;   // SIDECHAIN
  case 0:this.od=[0,1].map(()=>({lo:new OP(),hi:new OP(),dx:0,dy:0,air:new OP(),mid:new SVF(),f:new SVF(),dx2:0,dy2:0}));this.cEnv=0;this.gEnv=0;this.gG=1;this.phA=[new Float32Array(12),new Float32Array(12)];this.phB=[new Float32Array(6),new Float32Array(6)];this.phYB=[0,0];this.phCB=[0,0];this.phL1=0;this.phL2=0;this.phE=0;this.phY=[0,0];this.phL=0;this.phC=[0,0];break;
  case 1:this.od=[0,1].map(()=>({lo:new OP(),hi:new OP(),dx:0,dy:0,air:new OP(),mid:new SVF(),f:new SVF(),dx2:0,dy2:0}));this.cEnv=0;this.gEnv=0;this.gG=1;this.phA=[new Float32Array(12),new Float32Array(12)];this.phB=[new Float32Array(6),new Float32Array(6)];this.phYB=[0,0];this.phCB=[0,0];this.phL1=0;this.phL2=0;this.phE=0;this.phY=[0,0];this.phL=0;this.phC=[0,0];break;
  case 2:this.flD=[new DL(Math.round(fs*.03)),new DL(Math.round(fs*.03))];this.flL=0;this.flY=[0,0];this.flC=[fs*.002,fs*.002];this.flA=0;this.flAt=1;this.flE=0;break;
  case 3:this.chB=[0,1,2].map(()=>new BBD(Math.round(fs*.04)));this.chL=0;this.chR=0;this.chRT=0;this.chRV=0;break;
  case 4:this.mpS=[new Shift(Math.round(fs*.1)),new Shift(Math.round(fs*.1))];this.mpD=[new DL(Math.round(fs*.25)),new DL(Math.round(fs*.25))];this.mpY=[0,0];this.mpRt=0;this.mpRv=0;this.mpRc=0;this.mpT=[0,0];break;
  case 5:this.dlD=[new DL(Math.round(fs*3.2)),new DL(Math.round(fs*3.2))];this.dlT=[fs*.3,fs*.3];this.dlF=[0,0];this.dlH=[0,0];this.dlB=[0,0];this.dlU=fs*.1;this.dlP=0;this.dlW=0;this.dlWf=0;this.dlRc=0;this.dlRs=0;this.dlX=[new Shift(Math.round(fs*.1)),new Shift(Math.round(fs*.1))];this.dlRv=new Tank(fs,4);this.dlRo=[0,0];break;
  case 6:this.rvF=new FDN(fs);this.rvSp=null;this.rvSm=-1;this.rvPd=[new DL(Math.round(fs*.42)),new DL(Math.round(fs*.42))];this.rvIn=[0,0,0,0];this.rvO=[0,0];this.rvW=0;this.rvD=[0,0];break;
  case 7:this.grB=new DL(Math.round(fs*3.4));this.grN=16;this.grP=new Float32Array(16);this.grR=new Float32Array(16);this.grL=new Float32Array(16);this.grT=new Float32Array(16);this.grG=new Float32Array(16);this.grA=new Uint8Array(16);this.grC=0;this.grS=0;this.grY=0;this.grF=[new SVF(),new SVF()];this.grRn=0;break;
  case 8:this.fkD=[new DL(Math.round(fs*.07)),new DL(Math.round(fs*.07))];this.fkS=[0,0];this.fkF=[new SVF(),new SVF()];this.fkR=[new SVF(),new SVF()];this.fkY=[0,0];this.fkT=[0,0];break;
  case 9:this.bcH=[0,0];this.bcC=0;this.rmP=0;this.bcF=[new SVF(),new SVF()];this.bcE=0;this.bcG=1;this.bcSeq=new Float32Array(16).fill(1);this.bcB=[new DL(Math.round(fs*.6)),new DL(Math.round(fs*.6))];this.bcSt=false;this.bcSs=0;this.bcSi=0;this.bcSl=1;this.bcDr=0;this.bcDg=1;this.bcN=0;break;
  case 10:this.bcH=[0,0];this.bcC=0;this.rmP=0;this.bcF=[new SVF(),new SVF()];this.bcE=0;this.bcG=1;this.bcSeq=new Float32Array(16).fill(1);this.bcB=[new DL(Math.round(fs*.6)),new DL(Math.round(fs*.6))];this.bcSt=false;this.bcSs=0;this.bcSi=0;this.bcSl=1;this.bcDr=0;this.bcDg=1;this.bcN=0;break;
  case 11:this.stB=[new DL(Math.round(fs*4.2)),new DL(Math.round(fs*4.2))];this.stOn=false;this.stStart=0;this.stLen=1;this.stI=0;this.stLast=-1;this.stX=0;this.stRep=0;this.stPos=0;this.stG=-1;this.stGt=0;this.stPrevOn=false;this.stHold=0;break;
  case 12:this.reB=[new DL(Math.round(fs*4.5)),new DL(Math.round(fs*4.5))];this.reC=0;this.reS=0;this.reH=[{c:0,s:0,p:0,d:1,r:-1},{c:0,s:0,p:0,d:1,r:-1,half:true}];this.reL=0;this.reF=[0,0];this.reHold=-1;break;
  case 13:this.fzH=[new DL(Math.round(fs*1.2)),new DL(Math.round(fs*1.2))];this.fzLy=[0,1,2].map(()=>({b:[new Float32Array(Math.round(fs*.8)),new Float32Array(Math.round(fs*.8))],n:0,env:0,st:0,h:[{c:0,d:1,s:0,p:0},{c:0,d:1,s:0,p:0,half:1}]}));this.fzOn=false;this.fzE=0;this.fzLast=0;this.fzT=0;this.fzR=1;this.fzF=[new SVF(),new SVF()];this.fzNext=0;break;
  case 14:this.tpR=[new DL(Math.round(fs*.012)),new DL(Math.round(fs*.012))];this.tpD=[new DL(Math.round(fs*.56)),new DL(Math.round(fs*.56))];this.tpC=[new DL(Math.round(fs*.03)),new DL(Math.round(fs*.03))];this.tpW=0;this.tpF=0;this.tpF2=0;this.tpN=0;this.tpWb=0;this.tpWt=0;this.tpLg=-1;this.tpY=[0,0];this.tpYl=[0,0];this.tpT=[0,0];this.tpL1=[0,0];this.tpL2=[0,0];this.tpHP=[0,0];this.tpB=[new SVF(),new SVF()];this.tpH=[new SVF(),new SVF()];break;
  case 15:this.shF=new FDN(fs);this.shW=new FDN(fs);this.shSp=[new Spring(fs,33,12),new Spring(fs,41,12)];this.shPd=[new DL(Math.round(fs*.52)),new DL(Math.round(fs*.52))];this.shS=[0,1,2].map(()=>[new Shift(Math.round(fs*.12)),new Shift(Math.round(fs*.12))]);this.shY=[0,0];this.shO=[0,0];this.shWo=[0,0];this.shL=[0,0];this.shH=[0,0];this.shLF=[new SVF(),new SVF()];this.shOF=[new SVF(),new SVF()];this.shRp=0;break;
 }}
 // one instance per slot: it shares the rack's values and clock and keeps its own effect state and its own control ids
 unit(q,f){const u=Object.create(this);u.I=this.Iq[q];u.rs=1234567+q*7919;u.cur=f;u.initFx(f);return u}
 // quantize with triangular dither, so quiet sounds keep their tail instead of being gated to silence
 qz(x,q){return Math.round(x*q+this.rnd()-this.rnd())/q}
 rnd(){this.rs=(this.rs*1664525+1013904223)>>>0;return this.rs/4294967296}
 g(id){return this.p[this.I[id]]}
 val(id){const i=this.I[id],d=this.D[i];return cl(this.p[i]+this.off[i],d.mn,d.mx)}
 noteOn(n,vel){let v=this.voices.find(x=>x.n===n)||this.voices.find(x=>x.e<1e-4&&!x.g)||this.voices.reduce((a,b)=>a.age<b.age?a:b);v.n=n;v.f=440*Math.pow(2,(n-69)/12);v.g=true;v.vel=vel;v.age=++this.vage}
 noteOff(n){for(const v of this.voices)if(v.n===n)v.g=false}
 ctrl(){const dt=CT/this.fs,I=this.I,p=this.p;
  const c0=Math.floor(this.clock);this.clock+=this.g('bpm')/60*4*dt;const ns=Math.floor(this.clock)&15;this.step=ns;this.edge=Math.floor(this.clock)!==c0;
  const s=this.src;
  for(let i=0;i<4;i++){this.lp[i]+=lfoHz(this.val('l'+i+'rate'))*dt;if(this.lp[i]>=1){this.lp[i]-=1;this.lr[i]=this.rnd()*2-1}const ph=this.lp[i];
   s[i<3?1+i:12]=[Math.sin(TP*ph),1-4*Math.abs(ph-.5),2*ph-1,ph<.5?1:-1,this.lr[i]][Math.round(this.g('l'+i+'shape'))]}
  this.shp+=lfoHz(this.val('shrate'))*dt;if(this.shp>=1){this.shp-=1;this.sht=this.rnd()*2-1}
  const sm=this.g('shsm');this.shv+=(sm<=0?1:1-Math.exp(-dt/(.002+sm*.5)))*(this.sht-this.shv);s[4]=this.shv;
  this.sh2p+=lfoHz(this.val('sh2rate'))*dt;if(this.sh2p>=1){this.sh2p-=1;this.sh2t=this.rnd()*2-1}const sm2=this.g('sh2sm');this.sh2v+=(sm2<=0?1:1-Math.exp(-dt/(.002+sm2*.5)))*(this.sh2t-this.sh2v);s[13]=this.sh2v;
  for(let k=0;k<6;k++)s[5+k]=p[I['s'+k+'_'+this.step]];s[11]=Math.min(1,this.env*4);s[0]=0;
  this.off.fill(0);const D=this.D,DP=this.DSTP;
  const addMod=(si,di,amt)=>{if(si<=0||di<=0||!amt)return;const pi=DP[di];this.off[pi]+=s[si]*amt*(D[pi].mx-D[pi].mn)*.5};
  for(let m=0;m<8;m++)addMod(Math.round(this.g('m'+m+'src')),Math.round(this.g('m'+m+'dst')),this.g('m'+m+'amt'));
  for(let k=0;k<6;k++)addMod(5+k,Math.round(this.g('s'+k+'dst')),this.g('s'+k+'amt'));
  for(let i=0;i<p.length;i++){const d=D[i];this.v[i]=cl(p[i]+this.off[i],d.mn,d.mx)}
  const V=k=>this.v[I[k]];
  // the chain: a slot instance is (re)made when its effect changes
  this.active=[];for(let q=0;q<8;q++){const f=Math.round(p[I['slot'+q]])-1;if(f!==this.slotF[q]){this.slotF[q]=f;this.mixS[q]=0;this.units[q]=f>=0?this.unit(q,f):null}
   if(f>=0){this.active.push(q);this.mixT[q]=this.v[this.Iq[q]['f'+f+'mix']]}else this.mixT[q]=0}
  this.src0=Math.round(p[I.insrc]);this.gain=Math.pow(10,(V('ingain')-.5)*36/20);this.dw=V('drywet');this.mt=V('master');
  const bpm=this.g('bpm'),beat=60/bpm*this.fs;
  for(const q of this.active)this.units[q].fxCtrl(this.slotF[q],dt,bpm,beat)}
 // per effect values for one slot instance (this = the instance; I and V read that slot's controls)
 fxCtrl(f,dt,bpm,beat){const I=this.I,p=this.p,V=k=>this.v[I[k]];switch(f){
  case 16:{let m=0;for(let k=0;k<16;k++)if(p[I['f16st'+k]]>.5)m|=1<<k;const fs=this.fs;   // SIDECHAIN: here only the LFO and the sequence (no other parts)
   this.SC={src:Math.round(p[I.f16src]),rate:.25*Math.pow(2,Math.round(p[I.f16rate])),bpm,mask:m,up:1/Math.max(1,V('f16att')*300*fs/1000),down:Math.exp(-4.6/Math.max(1,10*Math.pow(200,V('f16dec'))*fs/1000)),amt:V('f16amt')};this.scBeat=this.clock/4;this.scK=0;break}
  case 0:{const md=Math.round(p[I.f0model]),fs=this.fs,a=hz=>1-Math.exp(-TP*hz/fs);
   if(md===0){const dr=V('f0vdrive');this.OD={md,g:1+dr*dr*(p[I.f0vx10]>.5?160:24),b:V('f0vbias')*.7,t:Math.round(p[I.f0vtype]),aL:a(140),aH:a(2800),lo:dbg(V('f0vlow')*9)-1,hi:dbg(V('f0vhigh')*9)-1,out:dbg((V('f0vout')-.5)*24)*.55/(1+dr*.6)};
     const t=this.OD.t,b=this.OD.b;this.OD.y0=t===0?triode(b):t===1?2/PI*Math.atan(b*1.8):Math.max(-1,Math.min(1,b*1.5))}
   else if(md===1){const sat=V('f0hsat'),den=V('f0hdens');this.OD={md,gs:1+sat*sat*14,gd:1+den*5,den,alt:p[I.f0halt]>.5,aA:a(9000),air:dbg(V('f0hair')*10)-1,bl:V('f0hblend'),out:dbg((V('f0hout')-.5)*24)*.5/(1+sat*.5+den*.3),
     mg:Math.tan(PI*1200/fs)}}
   else{const dr=V('f0bdrive'),fv=V('f0bfilt');const fc=fv<0?18000*Math.pow(2,fv*7):20*Math.pow(2,fv*8);this.OD={md,g:1+dr*dr*30,m:Math.round(p[I.f0bmode]),comp:V('f0bcomp'),thr:dbg(-V('f0bcomp')*30),
     fg:Math.tan(PI*Math.min(fc,fs*.45)/fs),hp:fv>.03,lp:fv<-.03,gate:V('f0bgate')*.12,out:dbg((V('f0bout')-.5)*24)*.6/(1+dr*.5)}}}break;
  case 1:{const pm=Math.round(p[I.f1model]);let hz,fmin,oct,n,fb,pos=[0,0],drive=0;
   if(pm===0){hz=.08*Math.pow(300,V('f1mrate'));n=Math.round(p[I.f1mstages])?12:6;fb=V('f1mres')*.9;fmin=110;oct=V('f1msweep')*6.2;drive=V('f1mdrive')}
   else if(pm===1){hz=.06*Math.pow(140,V('f1xspeed'));const it=Math.round(p[I.f1xint]);n=[4,6,8,10][it];fb=[.12,.3,.45,.62][it];fmin=230;oct=4.4}
   else if(pm===2){hz=.02*Math.pow(500,V('f1irate'));n=8;fb=V('f1ifb')*.9;fmin=55;oct=7.2}
   else{// BI-PHASE: two 6-stage phasers, two sweep oscillators (sine or rounded square), in series or in parallel (A left, B right)
    const st=V('f1stereo'),sh=[Math.round(p[I.f1bshape1]),Math.round(p[I.f1bshape2])];this.phL1+=.03*Math.pow(400,V('f1brate1'))*dt;this.phL2+=.03*Math.pow(400,V('f1brate2'))*dt;this.phL1-=Math.floor(this.phL1);this.phL2-=Math.floor(this.phL2);
    const osc=(k,ph)=>{const s=Math.sin(TP*ph);return .5+.5*(sh[k]?Math.tanh(s*4)/Math.tanh(4):s)};const srcB=Math.round(p[I.f1bsrcb]);const coef=f=>{const t=Math.tan(PI*cl(f,20,this.fs*.45)/this.fs);return(t-1)/(t+1)};
    for(let ch=0;ch<2;ch++){const o=ch*st*.5;const la=osc(0,this.phL1+o),lb=srcB?osc(1,this.phL2+o):osc(0,this.phL1+o);
     this.phC[ch]=coef(90*Math.pow(2,6*(.5+(la-.5)*V('f1bdepa'))));this.phCB[ch]=coef(90*Math.pow(2,6*(.5+(lb-.5)*V('f1bdepb'))))}
    this.PH={bi:true,fa:V('f1bfba')*.88,fbb:V('f1bfbb')*.88,par:Math.round(p[I.f1broute])===1,w:V('f1width')*2};hz=0}
   if(pm!==3){
   this.phL+=hz*dt;this.phL-=Math.floor(this.phL);const sm=Math.round(p[I.f1isrc]);
   const stv=V('f1stereo');for(let ch=0;ch<2;ch++){const ph=this.phL+ch*stv*.5;let l;
    if(pm===1){const t=1-2*Math.abs((ph%1)-.5);l=Math.pow(t,1.6)}else l=.5+.5*Math.sin(TP*ph);       // PHASE 100: a skewed triangle sweep
    if(pm===2){const dep=V('f1idepth');l=sm===0?.5+(l-.5)*dep+(V('f1iman')-.5)*(1-dep):sm===1?cl(V('f1iman')*.3+this.env*V('f1isens')*6*dep,0,1):V('f1iman');if(sm!==0&&ch)l=l+(1-2*l)*stv}
    pos[ch]=l;const f=cl(fmin*Math.pow(2,oct*l),20,this.fs*.45);const t=Math.tan(PI*f/this.fs);this.phC[ch]=(t-1)/(t+1)}
   this.PH={fb,n,drive,w:V('f1width')*2}}}break;
  case 2:{const fm=Math.round(p[I.f2model]),ms=this.fs/1000,st=V('f2stereo'),env=Math.min(1,this.env*3);const Q={m:fm,w:V('f2width')*2,pol:1,tz:false,dr:0,bl:1};let hz=0,lo,hi,pos=[0,0];
   const sinp=k=>.5+.5*Math.sin(TP*(this.flL+k*st*.5)),trip=k=>{let q=this.flL+k*st*.5;q-=Math.floor(q);return 1-2*Math.abs(q-.5)};
   if(fm===0){hz=.03*Math.pow(300,V('f2urate'));const c=.5+V('f2utime')*11.5,a=V('f2uamt');lo=c*(1-a*.9);hi=c*(1+a*1.5);Q.fb=V('f2ufb')*.93;Q.dr=V('f2udrive');const sq=Math.round(p[I.f2ushape]);
     for(let k=0;k<2;k++){const s=sinp(k);pos[k]=sq?.5+.5*Math.tanh((s-.5)*12)/Math.tanh(6):s}}
   else if(fm===1){hz=.02*Math.pow(500,V('f2irate'));lo=.05;hi=8;Q.fb=V('f2ifb')*.9;Q.tz=p[I.f2itzf]>.5;const sm=Math.round(p[I.f2isrc]),dep=V('f2idepth'),man=V('f2iman');
     for(let k=0;k<2;k++){let l=sm===0?.5+(sinp(k)-.5)*dep+(man-.5)*(1-dep):sm===1?cl(man*.3+env*V('f2isens')*2*dep,0,1):man;if(sm!==0&&k)l=l+(1-2*l)*st;pos[k]=l}}
   else if(fm===2){hz=.03*Math.pow(300,V('f2hrate'));lo=.2;hi=6.5;Q.fb=V('f2hres')*.9*(Math.round(p[I.f2hpol])?-1:1);Q.pol=Math.round(p[I.f2hpol])?-1:1;Q.bl=V('f2hblend');const dep=V('f2hdepth'),man=V('f2hman');
     for(let k=0;k<2;k++)pos[k]=cl(man+(trip(k)-.5)*dep,0,1)}
   else if(fm===3){hz=.03*Math.pow(300,V('f2srate'));lo=.3;hi=9;Q.fb=V('f2sregen')*.92;const md=Math.round(p[I.f2smode]),dep=V('f2sdepth'),man=V('f2sman'),e=env*V('f2ssens')*2;
     for(let k=0;k<2;k++){const lf=(sinp(k)-.5)*dep;pos[k]=cl(man+(md===0?lf:md===1?e*dep:lf*.6+e*dep*.7),0,1)}}
   else{hz=.03*Math.pow(300,V('f2aspeed'));lo=.25;hi=.25+8.75*V('f2arange');Q.fb=V('f2aenh')*.97;const md=Math.round(p[I.f2amode]),man=V('f2aman');
     if(md===2){const th=.02+V('f2athr')*.5;if(env>th&&this.flE<=th&&this.flAt>=1)this.flAt=0;this.flE=env;if(this.flAt<1)this.flAt+=hz*dt;this.flA=this.flAt<1?Math.sin(PI*this.flAt):0}
     for(let k=0;k<2;k++)pos[k]=md===0?trip(k):md===1?man:cl(man+(1-man)*this.flA,0,1)}
   this.flL+=hz*dt;this.flL-=Math.floor(this.flL);
   Q.d=pos.map(x=>cl((lo*Math.pow(hi/lo,x))*ms,1,this.fs*.028));Q.c=4*ms;this.FL=Q}break;
  case 3:{const cm=Math.round(p[I.f3model]),fs=this.fs,ms=fs/1000,ap=hz=>1-Math.exp(-TP*hz/fs);let hz;const C_={m:cm,w:V('f3width')*2};
   if(cm===0){hz=.1*Math.pow(90,V('f3crate'));C_.base=1.6*ms;C_.dep=V('f3cdepth')*4.2*ms;C_.ap=ap(7000)}
   else if(cm===1){const md=Math.round(p[I.f3dmode]);hz=[.25,.25,.5,.5][md];C_.base=3.2*ms;C_.dep=[.35,.7,.8,1.4][md]*ms;C_.ap=ap(9000)}
   else if(cm===2){hz=.05*Math.pow(120,V('f3trate'));C_.base=5*ms;C_.dep=V('f3tdepth')*6*ms;C_.ap=ap(3000*Math.pow(4,V('f3ttone')))}
   else{hz=.05*Math.pow(200,V('f3jrate'));C_.base=(1.5+V('f3jlag')*13.5)*ms;C_.dep=V('f3jdepth')*5*ms;C_.ap=ap(10000);const dv=V('f3jdpv');C_.dg=dv<.5?1:(1-dv)*2;C_.wg=dv<.5?dv*2:1;C_.ph=V('f3jphase')*.5;C_.sh=Math.round(p[I.f3jshape])}
   const prev=this.chL;this.chL+=hz*dt;if(this.chL>=1){this.chL-=1;this.chRT=this.rnd()}if(cm===3&&C_.sh===2){this.chRV+=(this.chRT-this.chRV)*Math.min(1,hz*dt*6)}
   this.CH=C_}break;
  case 4:{const hm=Math.round(p[I.f4model]),ms=this.fs/1000;const pre=hm?'f4n':'f4t';const st=Math.round((V(pre+'pitch')-.5)*24),dc=V(pre+'det')*50;
   const Q={m:hm,r:[Math.pow(2,(st*100+dc)/1200),Math.pow(2,(st*100-dc)/1200)],del:Math.max(1,V(pre+'del')*(hm?200:112.5)*ms),fb:V(pre+'fb')*.85,w:V('f4width')*2,win:Math.round((hm?.06:.04)*this.fs),rand:hm?V('f4nrand')*15*ms:0};
   if(Q.rand){if(++this.mpRc>=Math.round(this.fs*.18/CT)){this.mpRc=0;this.mpRt=this.rnd()}this.mpRv+=(this.mpRt-this.mpRv)*.05}this.MP=Q}break;
  case 5:{const dm=Math.round(p[I.f5model]),fs=this.fs,sync=p[I.f5sync]>.5,ap=hz=>1-Math.exp(-TP*hz/fs);
   const T=v=>cl((sync?NOTEB[Math.round(v*12)]*60/bpm:.02*Math.pow(75,v))*fs,8,fs*3);const Q={m:dm};
   if(dm===0){Q.t=[T(V('f5pta')),T(V('f5ptb'))];Q.fb=V('f5pfb')*.95;Q.mr=.05*Math.pow(120,V('f5pmr'));Q.md=V('f5pmd')*.004*fs;Q.lp=ap(1500*Math.pow(8,V('f5ptone')))}
   else if(dm===1){const t=T(V('f5ltime'));Q.type=Math.round(p[I.f5ltype]);Q.t=Q.type===1?[t,t*.75]:[t,t];Q.fb=V('f5lrep')*.97;Q.mr=.6;Q.md=V('f5lmod')*.0025*fs;const fv=V('f5lfilt');Q.lp=fv<0?ap(16000*Math.pow(2,fv*6)):ap(16000);Q.hp=fv>0?ap(20*Math.pow(2,fv*8)):0;Q.grit=V('f5lgrit')}
   else if(dm===2){Q.t=[T(V('f5xtl')),T(V('f5xtr'))];Q.fb=V('f5xfb')*.92;const st=Math.round((V('f5xpitch')-.5)*24),dc=V('f5xdet')*50;Q.r=[Math.pow(2,(st+dc/100)/12),Math.pow(2,(st-dc/100)/12)];Q.lp=ap(15600)}
   else if(dm===3){const t=T(V('f5stime'));Q.t=[t,t];Q.fb=V('f5sfb')*.95;Q.mr=.05*Math.pow(120,V('f5srate'));Q.md=V('f5smod')*.004*fs;Q.lp=ap(16000*Math.pow(2,-V('f5sdamp')*4));Q.dr=V('f5sdrive')}
   else if(dm===4){Q.u=fs*(.12-.07*V('f5espeed'));const H=[[1],[2],[3],[4],[1,2],[2,3],[3,4],[1,3],[2,4],[1,2,3,4]][Math.round(p[I.f5eheads])];Q.h=H;Q.mode=Math.round(p[I.f5emode]);Q.fb=V('f5eswell')*.9;
     const tn=V('f5etone');Q.lp=ap(5200*Math.pow(2,tn));Q.wow=.15}
   else if(dm===5){Q.u=fs*(.06+V('f5htime')*.69);Q.h=[1];Q.mode=1;Q.fb=V('f5hsus')*1.05;Q.lp=ap(6500);Q.pre=V('f5hpre');Q.wow=V('f5hwow')}
   else if(dm===7){Q.u=fs*(.11-V('f5cspeed')*.06);const H=[];for(let k=1;k<=5;k++)if(p[I['f5ch'+k]]>.5)H.push(k);Q.h=H.length?H:[1];Q.mode=2;Q.fb=V('f5cswell')*.8;Q.lp=ap(5000*Math.pow(2,V('f5ctone')));Q.pre=V('f5cdrive');Q.wow=V('f5cwow')}
   else{Q.u=fs*(.18-V('f5rrate')*.12);Q.h=[[1],[2],[3],[2,3],[1,3],[1,2],[1,2,3]][Math.round(p[I.f5rmode])];Q.mode=2;Q.fb=V('f5rint')*.82;Q.lp=ap(4200);Q.bass=V('f5rbass');Q.treb=V('f5rtreb');Q.rev=V('f5rrev');Q.wow=V('f5rwow');Q.aB=ap(250);Q.aT=ap(2500)}
   this.DL_=Q}break;
  case 6:{const rm=Math.round(p[I.f6model]),M=RVMOD[rm],fs=this.fs,ap=hz=>1-Math.exp(-TP*Math.min(hz,fs*.45)/fs);const Q={m:rm,M,w:V('f6width')*2};
   if(M.k===1){if(this.rvSm!==rm){this.rvSm=rm;this.rvSp=M.sp.map(ms=>new Spring(fs,ms,M.st))}
     Q.dr=1+V('f6sdwell')*6*M.dr;Q.g=.62+V('f6sdecay')*.35;Q.a=ap(M.lp*Math.pow(2,V('f6stone')*1.3));Q.c=M.c;Q.wob=(M.wob||0)*.004}
   else{let rt,pre,tone=0,mic=.5,size=M.size,damp=M.damp,low=M.low,mod=M.mod;
     if(rm<=3){rt=.3*Math.pow(40,V('f6ddecay'));pre=V('f6dpre');size*=.5+V('f6dsize');damp*=Math.pow(2,-V('f6ddamp')*3);low=.5*Math.pow(4,V('f6dlow'));mod*=V('f6dmod')*2}
     else if(rm<=13){rt=.8*Math.pow(6,V('f6cdecay'))*M.rt;pre=V('f6cpre');tone=V('f6ctone');mic=V('f6cmic')}
     else if(rm<=18){rt=.5*Math.pow(12,V('f6pdecay'));pre=V('f6ppre');tone=V('f6ptone')}
     else{rt=.2*Math.pow(12,V('f6odecay'))*M.rt;pre=V('f6opre');tone=V('f6otone');mic=V('f6odist')}
     damp*=Math.pow(2,tone*1.5);this.rvF.set(size,rt*1.35,low,damp,M.diff,mod);   // x1.35: the air damping shortens the felt decay
     Q.pre=pre*.25*fs;Q.erG=M.er?1.1-mic*.8:0;Q.lateG=M.er?.55+mic*.7:1;Q.bw=ap(M.bw*Math.pow(2,tone));Q.hp=ap(M.hp)}
   this.RV=Q}break;
  case 7:{const gm=Math.round(p[I.f7model]),fs=this.fs;const Q={m:gm,len:Math.round(fs*.015*Math.pow(20,V('f7size'))),st:Math.round((V('f7pitch')-.5)*24),spray:V('f7spray')*.5*fs,frz:p[I.f7freeze]>.5};
   Q.int=gm===3?[.125,.25,.5,1,2][Math.round(V('f7rate')*4)]*60/bpm*fs:fs/(1+V('f7dens')*39);Q.sync=gm===3;
   Q.pos=gm===3?.05*fs:V('f7pos')*1.6*fs;Q.win=gm===1||gm===2?V('f7tex'):gm===3?V('f7contour'):.5;Q.rev=gm===1?V('f7rev'):0;Q.fb=gm===1?V('f7fb')*.8:0;
   Q.scan=gm===2?V('f7scan'):0;Q.spread=gm===2?V('f7spread'):.7;Q.warp=gm===3?V('f7warp'):0;Q.pat=Math.round(p[I.f7pat]);Q.dr=gm===0?V('f7drive'):0;
   const fv=gm===0||gm===2?V('f7filt'):0;const fc=fv<0?18000*Math.pow(2,fv*7):20*Math.pow(2,fv*8);Q.fg=Math.tan(PI*Math.min(fc,fs*.45)/fs);Q.lp=fv<-.03;Q.hp=fv>.03;
   this.grS+=Q.scan*dt*.4;this.grS-=Math.floor(this.grS);this.GR=Q}break;
  case 8:{const fm=Math.round(p[I.f8model]),fs=this.fs;const Q={m:fm};
   if(fm===0){const f=30*Math.pow(70,V('f8rtune')),sp=V('f8rspread')*50/1200;Q.d=[fs/(f*Math.pow(2,sp)),fs/(f*Math.pow(2,-sp))];Q.f=[f*Math.pow(2,sp),f*Math.pow(2,-sp)].map(x=>Math.tan(PI*Math.min(x,fs*.45)/fs));
     Q.fb=V('f8rfb')*1.02;Q.dr=V('f8rdrive');Q.res=Math.round(p[I.f8rmode])===1;const fv=V('f8rtone');const fc=fv<0?16000*Math.pow(2,fv*6):20*Math.pow(2,fv*9);Q.tg=Math.tan(PI*Math.min(fc,fs*.45)/fs);Q.lp=fv<-.03;Q.hp=fv>.03}
   else{Q.d=fs*(1+V('f8tloop')*59)/1000;Q.fb=p[I.f8thold]>.5?1.6:V('f8tfb')*1.35;Q.bg=Math.tan(PI*(250*Math.pow(24,V('f8ttone')))/fs);Q.vol=V('f8tvol')*.6;Q.kill=p[I.f8tkill]>.5}
   this.FK=Q}break;
  case 9:{const bm=Math.round(p[I.f9model]),fs=this.fs;const Q={m:bm,q:Math.pow(2,16-V('f9crush')*14-1),div:1+Math.pow(V('f9rate'),2)*49};
   if(bm===0){const fv=V('f9bfilt');const fc=fv<0?18000*Math.pow(2,fv*7):20*Math.pow(2,fv*8);Q.fg=Math.tan(PI*Math.min(fc,fs*.45)/fs);Q.lp=fv<-.03;Q.hp=fv>.03;Q.gate=V('f9bgate')*.15}
   else if(bm===1){Q.fg=Math.tan(PI*Math.min(200*Math.pow(90,V('f9ofilt')),fs*.45)/fs);Q.lp=true;Q.k=.7;
     if(this.edge){if(this.step===0)for(let k=0;k<16;k++)this.bcSeq[k]=Math.pow(2,(this.rnd()*2-1)*3);const sd=V('f9oseq');Q.seqf=1;
       if(this.rnd()<V('f9ostut')){this.bcSt=true;this.bcSl=Math.max(32,Math.round(60/bpm*fs/8));this.bcSs=this.bcB[0].w-this.bcSl;this.bcSi=0}else this.bcSt=false}
     Q.div*=Math.pow(this.bcSeq[this.step],V('f9oseq'))}
   else if(bm===2){Q.vol=V('f9jvol')*2;Q.fg=Math.tan(PI*5000/fs);Q.lp=p[I.f9jfilt]>.5}
   else{Q.jit=V('f9djit');Q.noise=V('f9dnoise')*.02;Q.drop=V('f9ddrop');Q.drive=V('f9ddrive');if(Q.drop>0&&this.rnd()<Q.drop*.05)this.bcDr=Math.round(fs*(.005+this.rnd()*.04))}
   this.BC=Q}break;
  case 10:{const rm=Math.round(p[I.f10model]);const lohi=rm===0||rm===2;const lo=lohi&&Math.round(p[I.f10range])===0;const v=V('f10freq');let f=lo?.5*Math.pow(160,v):lohi?30*Math.pow(133,v):15*Math.pow(200,v);
   const Q={m:rm,sq:(rm===0||rm===3)&&Math.round(p[I.f10wave])===1,vol:1,drive:0,mode:0,sp:0};
   if(rm===0){this.rmL+=.03*Math.pow(300,V('f10rlrate'))*dt;Q.lfo=V('f10rldepth')}
   else if(rm===1){f*=Math.pow(2,V('f10xfine')/12);Q.mode=Math.round(p[I.f10xmode]);Q.sp=V('f10xspread')}
   else if(rm===2){this.rmL+=.03*Math.pow(300,V('f10mlrate'))*dt;Q.lfo=V('f10mlamt');Q.lsq=Math.round(p[I.f10lshape])===1;Q.drive=V('f10mdrive')}
   else Q.vol=V('f10jvol')*2;
   this.rmL=(this.rmL||0)-Math.floor(this.rmL||0);
   if(Q.lfo){const s=Math.sin(TP*this.rmL);const l=Q.lsq?Math.tanh(s*6):s;f*=Math.pow(2,l*Q.lfo*2)}
   Q.f=f;this.RM=Q}break;
  case 11:{const sm=Math.round(p[I.f11model]),DIV=[2,1,.5,.25,.125,.0625],on=p[I.f11on]>.5,live=true,maxL=this.stB[0].n-16;const Q={m:sm,gate:V('f11gate')};
   const L=v=>Math.min(maxL,Math.max(64,Math.round(DIV[Math.round(v*5)]*beat)));
   if(sm===0){let len=L(V('f11rrate'));if(p[I.f11rtrip]>.5)len=Math.round(len*2/3);Q.len=len;Q.pitch=(V('f11rpitch')-.5)*4;Q.fade=V('f11rfade');
     if(this.edge&&live){const rand=Math.round(p[I.f11rmode])===1;let want=on;if(rand){if(this.stHold>0)this.stHold--;else{want=on||this.rnd()<V('f11rchance');this.stHold=want?3:0}if(this.stHold>0)want=true}
       if(want&&!this.stOn){this.stOn=true;this.stStart=this.stB[0].w-len;this.stI=0;this.stPos=0;this.stRep=0;this.stLen=len}else if(!want)this.stOn=false}}
   else{const durB=[.5,1,2,4,8,16][Math.round(V('f11sdur')*5)];const mode=Math.round(p[I.f11smode]);Q.l0=L(V('f11sstart'));Q.l1=L(V('f11send'));Q.cv=V('f11scurve');Q.dur=durB*beat;Q.pitch=0;Q.fade=0;
     if(this.edge&&live){const start=mode===2?(this.step===0&&on||this.step===0&&!on&&false):(on&&!this.stPrevOn)||(mode===1&&on&&this.stG<0);
       if(mode===2&&this.step===0&&(on||this.rnd()<.5))this.stG=0;else if(start&&mode!==2)this.stG=0;
       if(this.stG===0&&!this.stOn){this.stOn=true;this.stStart=this.stB[0].w-Q.l0;this.stI=0;this.stPos=0;this.stRep=0;this.stLen=Q.l0;this.stGt=0}
       if(!on&&mode!==2){this.stG=-1;this.stOn=false}this.stPrevOn=on}}
   this.ST=Q}break;
  case 12:{const rm=Math.round(p[I.f12model]),fs=this.fs;const Q={m:rm,T:Math.round(fs*.08*Math.pow(25,V('f12time')))};
   if(rm===0){let s=(V('f12tspeed')-.5)*4;if(Math.abs(s)<.13)s=s<0?-.13:.13;Q.rate=s;Q.rand=V('f12trand');Q.hold=p[I.f12thold]>.5;Q.fb=0}
   else if(rm===1){Q.fb=V('f12fb')*.85;Q.md=V('f12smod')*.004*fs;Q.mr=.6;Q.a=1-Math.exp(-TP*1200*Math.pow(12,V('f12stone'))/fs)}
   else{Q.fb=V('f12fb')*.85;Q.md=V('f12dtweak')*.006*fs;Q.mr=.1*Math.pow(60,V('f12dtweez'));Q.a=1}
   this.RE=Q}break;
  case 13:{const zm=Math.round(p[I.f13model]),fs=this.fs,ft=v=>.005*Math.pow(600,v),k=t=>1-Math.exp(-dt/t);const Q={m:zm,on:p[I.f13on]>.5};
   if(zm===0){Q.n=Math.round(fs*.003*Math.pow(40,V('f13wsize')));Q.G=Q.n;Q.rate=Math.pow(2,Math.round((V('f13wpitch')-.5)*24)/12);Q.att=k(.01);Q.rel=k(.05);Q.dec=1;Q.lvl=1;Q.single=true;
     const fv=V('f13wtone');const fc=fv<0?16000*Math.pow(2,fv*6):20*Math.pow(2,fv*9);Q.tg=Math.tan(PI*Math.min(fc,fs*.45)/fs);Q.lp=fv<-.03;Q.hp=fv>.03;this.fzR+=(Q.rate-this.fzR)*(1-Math.exp(-dt/(.001+V('f13wglide')*1.5)))}
   else if(zm===1){Q.n=Math.round(fs*.35);Q.G=Math.round(fs*.12);Q.att=k(ft(V('f13pin')));Q.rel=k(ft(V('f13pout')));Q.dec=1;Q.lvl=V('f13plevel')*1.4;Q.single=true;Q.auto=Math.round(p[I.f13pmode])===1;this.fzR=1}
   else{Q.n=Math.round(fs*.35);Q.G=Math.round(fs*.12);Q.att=k(ft(V('f13xrise')));Q.rel=k(ft(V('f13xtail')));const sv=V('f13xsus');Q.dec=sv>.97?1:Math.exp(-dt/(1+29*sv*sv));Q.lvl=V('f13xlevel')*1.4;Q.single=Math.round(p[I.f13xmode])===0;this.fzR=1}
   // triggers: the FREEZE switch, or (DEEP FREEZE in AUTO) each new note
   const env=Math.min(1,this.env*3);let cap=false,rel=false;
   if(Q.auto){this.fzT+=dt;if(env>.05&&env>this.fzE*1.8+.01&&this.fzT>.15){cap=true;this.fzT=0}this.fzE+=(env-this.fzE)*k(.3);if(Q.on&&!this.fzOn)cap=true}   // a new note: the level jumps well above its recent average
   else{if(Q.on&&!this.fzOn)cap=true;if(!Q.on&&this.fzOn)rel=true}
   this.fzOn=Q.on;
   if(cap){if(Q.single)for(const L of this.fzLy)if(L.st===1)L.st=2;const L=this.fzLy[this.fzNext];this.fzNext=(this.fzNext+1)%3;   // the oldest layer is reused
     const n=Math.min(Q.n,L.b[0].length),H=this.fzH;for(let i=0;i<n;i++){L.b[0][i]=H[0].at(H[0].w-n+i);L.b[1][i]=H[1].at(H[1].w-n+i)}L.n=n;L.st=1;L.env=0;for(const h of L.h){h.c=0;h.go=0}}
   if(rel)for(const L of this.fzLy)if(L.st===1)L.st=2;
   this.FZ=Q}break;
  case 14:{const tm=Math.round(p[I.f14model]),fs=this.fs,Q={m:tm},op=f=>1-Math.exp(-TP*f/fs),db=v=>Math.pow(10,(v-.5)*24/20);
   if(tm===0){Q.son=p[I.f14dson]>.5;Q.dbl=p[I.f14ddbl]>.5;Q.g=.5*Math.pow(10,V('f14dsat')*1.2);Q.out=db(V('f14dvol'))/(Q.son?Math.pow(Q.g,.6):1);Q.t=V('f14dtone');Q.ta=op(700);
     const lv=V('f14dlag');const ms=lv<.3?-4+lv/.3*14:10*Math.pow(50,(lv-.3)/.7);Q.ref=fs*.0045;Q.lag=Q.ref+ms*fs/1000;   // the reference deck runs 4.5 ms late, so the lag deck can pass ahead of it (through zero)
     Q.wob=V('f14dwob')*fs*.0016;const b=V('f14dblend');Q.gr=Math.min(1,2-2*b);Q.gl=Math.min(1,2*b);Q.ty=Math.round(p[I.f14dtype]);Q.s=Q.ty===1?-1:1;Q.fb=Q.ty===2?.5:0;Q.wide=p[I.f14dwide]>.5;Q.nm=1/Math.max(1,(Q.gr+Q.gl)*.75);Q.la=op(9000)}
   else if(tm===1){const ch=Math.round(p[I.f14ctype]);Q.g=.6*Math.pow(10,V('f14cdrive')*1.1);Q.h=ch?1:.8;Q.out=1/Math.pow(Q.g,.55);Q.wow=V('f14cwow');Q.fl=V('f14cflut');Q.hiss=V('f14chiss')*(ch?.012:.018);
     Q.t=V('f14ctone');Q.ta=op(900);Q.wear=V('f14cwear');Q.la=op((ch?13500:9000)*(1-.65*Q.wear));Q.ha=op(45)}
   else{const sp=Math.round(p[I.f14aspeed]),tp=Math.round(p[I.f14atape]),b=V('f14abias');Q.gi=.25*Math.pow(16,V('f14ain'));Q.out=db(V('f14aout'))/Math.pow(Q.gi,.55);
     Q.h=[.75,1,1.3,1.6][tp]*[.85,1,1.1][sp]*(1+.35*b);const hf=Math.min([13000,18000,22000][sp]*(1-.35*b),fs*.45);Q.lg=Math.tan(PI*hf/fs);
     Q.bg=Math.tan(PI*[45,70,110][sp]/fs);Q.bk=.8;Q.bm=[.45,.32,.22][sp]*.8;Q.hiss=V('f14ahiss')*.006*[1.4,1,.7][sp];Q.as=.05+.1*Math.max(0,-b);Q.ha=op(18)}
   this.TP=Q}break;
  case 15:{const sm=Math.round(p[I.f15model]),fs=this.fs,op=f=>1-Math.exp(-TP*f/fs),Q={m:sm,wl:0},F=this.shF;
   if(sm===0){const rt=Math.pow(40,V('f15bdecay')),damp=6000*Math.pow(3,V('f15btone'));Q.pre=V('f15bpre')*.5*fs;F.set(1.25,rt,1.1,damp,.7,V('f15bmod')*1.5);
     Q.v=[[2],[1.5],[2,1.5],[.5],[2,.5]][Math.round(p[I.f15bpitch])];Q.amt=V('f15bamt');Q.la=op(damp);Q.out=.9}
   else if(sm===1){const rt=Math.pow(40,V('f15alen'));Q.pre=.02*fs;F.set(1.5,rt,1.2,7000,.75,0);F.mr=(.08+3*V('f15arate'))/fs;F.md=V('f15adepth')*.004*fs;
     const md=Math.round(p[I.f15amode]);Q.v=[[2],[.5],[2,.5],[1.5,2],[4],[2,1.5,.5]][md];Q.amt=V('f15ashim');Q.la=op(9000);Q.out=.95;
     this.shRp+=((p[I.f15aramp]>.5?1:0)-this.shRp)*(1-Math.exp(-dt/1.2));if(this.shRp>1e-3)Q.v=Q.v.map(r=>Math.pow(r,1-this.shRp));   // RAMP: the octaves slide back to unison
     const fv=V('f15afilt');Q.fl=fv<-.03?1:fv>.03?2:0;Q.fg=Math.tan(PI*Math.min(fv<0?16000*Math.pow(2,fv*5):20*Math.pow(2,fv*6),fs*.45)/fs)}
   else{const rt=Math.pow(40,V('f15kdecay')),damp=5000*Math.pow(3,V('f15ktone'));Q.pre=V('f15kpre')*.5*fs;F.set(1.3,rt,1.1,damp,.72,.6);
     Q.v=[2];Q.amt=V('f15kmod');Q.la=op(damp);Q.out=.6;const dw=V('f15kdwell');Q.dw=.6+4*dw;Q.dwn=Math.tanh(Q.dw);Q.wl=V('f15kworld');Q.wt=Math.round(p[I.f15kworldt]);Q.rt=Math.round(p[I.f15kroute]);
     const wr=rt*.6;if(Q.wt===0)this.shW.set(1.6,wr,1.3,damp*1.2,.7,.5);else if(Q.wt===1)this.shW.set(.7,wr,1.0,damp*2,.8,.2);
     else{Q.sg=[33,41].map(ms=>Math.pow(10,-3*ms/1000/Math.max(.3,wr)));Q.sa=op(Math.min(damp,5000))}}
   Q.gn=1/Math.sqrt(Q.v.length);Q.ha=op(180);Q.win=Math.round(fs*.1);this.SH=Q}break;
 }}
 // ----- built-in sources -----
 demo(){const d=this.dv,fs=this.fs;if(this.edge){const st=this.step;const NOTES=[45,57,52,57,48,60,55,52,45,57,52,64,43,55,50,59];d.f=440*Math.pow(2,(NOTES[st]-69)/12);d.e=1;
   if(st%4===0){d.kp=0;d.ke=1}if(st%4===2)d.he=1}
  d.ph+=d.f/fs;d.ph-=Math.floor(d.ph);d.e*=.99975;const saw=2*d.ph-1;const fc=1-Math.exp(-TP*(200+3000*d.e*d.e)/fs);d.lp1+=fc*(saw-d.lp1);d.lp2+=fc*(d.lp1-d.lp2);
  d.ke*=.9993;d.kp+=(45+180*d.ke*d.ke)/fs;const kick=Math.sin(TP*d.kp)*d.ke;
  d.he*=.996;const n=this.rnd()*2-1;const hp=n-d.hp;d.hp=n;const hat=hp*d.he*.25;
  const y=d.lp2*d.e*.45+kick*.6;return [y+hat*.7,y+hat*1.1]}
 kbd(){let y=0;const fs=this.fs;for(const v of this.voices){if(v.e<1e-5&&!v.g)continue;v.e+=v.g?(v.vel-v.e)*.004:-v.e*.0006;v.ph+=v.f/fs;v.ph-=Math.floor(v.ph);
   const saw=2*v.ph-1,sq=v.ph<.5?.6:-.6;const fc=1-Math.exp(-TP*(300+4000*v.e)/fs);v.lp1+=fc*(saw*.7+sq*.3-v.lp1);v.lp2+=fc*(v.lp1-v.lp2);y+=v.lp2*v.e*.3}return [y,y]}
 // ----- the 16 effects: each takes (l, r) and writes this.o -----
 fx(f,l,r){const o=this.o;switch(f){
  case 16:{const Q=this.SC;let e=this.scE;
   if(Q.src>=4){const pos=(this.scBeat+(this.scK++)*Q.bpm/60/this.fs)*Q.rate;if(Q.src===4)e=.5+.5*Math.cos(TP*(pos-Math.floor(pos)));else{const st=Math.floor(pos)&15;if(st!==this.scSt){this.scSt=st;if((Q.mask>>st)&1)this.scR=true}if(this.scR){e+=Q.up;if(e>=1){e=1;this.scR=false}}else e*=Q.down}}else e*=Q.down;
   this.scE=e;const g=1-Q.amt*e;o[0]=l*g;o[1]=r*g;break}
  case 0:{const Q=this.OD;
   if(Q.md===0){for(let ch=0;ch<2;ch++){const S=this.od[ch];const u=(ch?r:l)*Q.g+Q.b;let y;
     if(Q.t===0)y=triode(u);else if(Q.t===1)y=2/PI*Math.atan(u*1.8);
     else{y=Math.max(-1,Math.min(1,u*1.5));const gt=Math.abs(Q.b)*.6;if(gt>0&&Math.abs(u-Q.b)<gt)y=Q.b*1.5+(y-Q.b*1.5)*Math.abs(u-Q.b)/gt}     // pentode 2: bias starves the stage, it sputters
     y-=Q.y0;const dc=y-S.dx+.995*S.dy;S.dx=y;S.dy=dc;y=dc;
     const lo=S.lo.lp(y,Q.aL),hi=y-S.hi.lp(y,Q.aH);y+=lo*Q.lo+hi*Q.hi;o[ch]=y*Q.out}}
   else if(Q.md===1){for(let ch=0;ch<2;ch++){const S=this.od[ch];const x=ch?r:l;let w=x;
     if(Q.alt){S.mid.run(w,Q.mg,.7);w+=S.mid.bpo*1.5}                                   // alt: the saturation leans on the mids
     w=Math.tanh(w*Q.gs);                                                                // pentode: saturation
     const t=w*Q.gd;w=t/(1+Math.abs(t))+.18*Q.den*t*t/(1+t*t);                           // triode: density, even harmonics
     const dc=w-S.dx+.995*S.dy;S.dx=w;S.dy=dc;w=dc;
     w+=(w-S.air.lp(w,Q.aA))*Q.air;                                                      // air: shelf on the top
     o[ch]=(x+(w*1.3-x)*Q.bl)*Q.out}}
   else{const lvl=Math.max(Math.abs(l),Math.abs(r));this.gEnv+=(lvl>this.gEnv?.02:.0004)*(lvl-this.gEnv);const gt=this.gEnv>Q.gate||Q.gate<=0?1:0;this.gG+=(gt-this.gG)*(gt?.01:.0006);
     const y=[0,0];for(let ch=0;ch<2;ch++){const u=(ch?r:l)*Q.g;let s;
      if(Q.m===0)s=Math.tanh(u);else if(Q.m===1)s=u>0?1-Math.exp(-u):-(1-Math.exp(u*1.6))/1.6;else{s=Math.tanh(u*6);s=Math.sign(s)*Math.pow(Math.abs(s),.5)}
      y[ch]=s}
     const e=Math.max(Math.abs(y[0]),Math.abs(y[1]));this.cEnv+=(e>this.cEnv?.004:.00012)*(e-this.cEnv);let cg=1;if(this.cEnv>Q.thr)cg=Math.pow(Q.thr/this.cEnv,.75);cg*=1+Q.comp*2;
     for(let ch=0;ch<2;ch++){const S=this.od[ch];let s=y[ch]*cg;S.f.run(s,Q.fg,.9);if(Q.lp)s=S.f.lpo;else if(Q.hp)s=S.f.hpo;
      const dc=s-S.dx+.995*S.dy;S.dx=s;S.dy=dc;o[ch]=dc*Q.out*this.gG}}
   break}
  case 1:{const Q=this.PH;
   if(Q.bi){const ap=(x,st,c)=>{for(let k=0;k<6;k++){const z=c*x+st[k];st[k]=x-c*z;x=z}return x};
    for(let ch=0;ch<2;ch++){const x=ch?r:l;let ya=x+Math.tanh(this.phY[ch]*Q.fa);ya=ap(ya,this.phA[ch],this.phC[ch]);this.phY[ch]=ya;const a=.5*(x+ya);
     const xb=Q.par?x:a;let yb=xb+Math.tanh(this.phYB[ch]*Q.fbb);yb=ap(yb,this.phB[ch],this.phCB[ch]);this.phYB[ch]=yb;const b=.5*(xb+yb);
     o[ch]=Q.par?(ch?b:a):b}                              // parallel: phaser A on the left, phaser B on the right
    const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*Q.w;o[0]=mid+sd;o[1]=mid-sd;break}
   for(let ch=0;ch<2;ch++){let x=ch?r:l;if(Q.drive>0)x=Math.tanh(x*(1+Q.drive*6))/(1+Q.drive*2);let y=x+Math.tanh(this.phY[ch]*Q.fb);const st=this.phA[ch],c=this.phC[ch];for(let k=0;k<Q.n;k++){const z=c*y+st[k];st[k]=y-c*z;y=z}this.phY[ch]=y;o[ch]=.5*(x+y)}const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*Q.w;o[0]=mid+sd;o[1]=mid-sd;break}
  case 2:{const Q=this.FL;for(let ch=0;ch<2;ch++){let x=ch?r:l;if(Q.dr)x=Math.tanh(x*(1+Q.dr*5))/(1+Q.dr*1.5);const dl=this.flD[ch];this.flC[ch]+=(Q.d[ch]-this.flC[ch])*.02;
    let y,dry=x;if(Q.tz){y=dl.get(Q.c+(this.flC[ch]-4*this.fs/1000)*1.0);dry=dl.get(Q.c);dl.put(x+Math.tanh(y*Q.fb));o[ch]=.5*(dry-y)}         // through zero: the dry path is delayed too, the notches pass through zero
    else{y=dl.get(this.flC[ch]);dl.put(x+Math.tanh(y*Q.fb));o[ch]=(dry*(1-Q.bl*.5)+Q.pol*y*Q.bl*.5)}}
   const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*Q.w;o[0]=mid+sd;o[1]=mid-sd;break}
  case 3:{const C_=this.CH,B=this.chB,tri=ph=>{ph-=Math.floor(ph);return 1-2*Math.abs(ph-.5)},m=.5*(l+r);
   const ph=this.chL;
   if(C_.m===0){const y=B[0].run(m,C_.base+C_.dep*tri(ph),C_.ap);o[0]=o[1]=.5*(m+y)*1.3}                                  // CE-2: mono, triangle sweep, wet and dry equal
   else if(C_.m===1){const a=B[0].run(m,C_.base+C_.dep*tri(ph),C_.ap),b=B[1].run(m,C_.base+C_.dep*(1-tri(ph)),C_.ap);       // DIMENSION D: two lines swept in opposite directions, cross-mixed
     o[0]=(m+.7*a-.3*b)*.75;o[1]=(m+.7*b-.3*a)*.75}
   else if(C_.m===2){const y=[0,1,2].map(k=>B[k].run(m,C_.base+C_.dep*(.5+.5*Math.sin(TP*(ph+k/3))),C_.ap));                   // TRI-STEREO: left, centre, right
     o[0]=.45*m+.65*y[0]+.35*y[1];o[1]=.45*m+.65*y[2]+.35*y[1]}
   else{const lf=k=>{const q=ph+k*C_.ph;return C_.sh===0?.5+.5*Math.sin(TP*q):C_.sh===1?tri(q):(k?1-this.chRV:this.chRV)};      // JULIANNA: stereo in, dry / chorus / vibrato blend
     for(let ch=0;ch<2;ch++){const x=ch?r:l;const y=B[ch].run(x,C_.base+C_.dep*lf(ch),C_.ap);o[ch]=(x*C_.dg+y*C_.wg)/(C_.dg+C_.wg)}}
   const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*C_.w;o[0]=mid+sd;o[1]=mid-sd;break}
  case 4:{const Q=this.MP,m=.5*(l+r);
   for(let ch=0;ch<2;ch++){const t=Q.del+(Q.rand?Q.rand*(ch?1-this.mpRv:this.mpRv):0);this.mpT[ch]+=(t-this.mpT[ch])*.002;const dl=this.mpD[ch];dl.put(m+this.mpY[ch]*Q.fb);const x=dl.get(this.mpT[ch]);
    let y=Q.m?this.mpS[ch].run(x,Q.r[ch],Q.win):this.mpS[ch].runEdge(x,Q.r[ch],Q.win,.06);if(!Q.m)y=this.qz(y,4096);this.mpY[ch]=y;o[ch]=y}
   const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*Q.w;o[0]=mid+sd;o[1]=mid-sd;break}
  case 5:{const Q=this.DL_,B=this.dlD,F=this.dlF,fs=this.fs;
   if(Q.m<=3){for(let c=0;c<2;c++)this.dlT[c]+=(Q.t[c]-this.dlT[c])*.0004;let md0=0,md1=0;if(Q.md){this.dlP+=Q.mr/fs;this.dlP-=Math.floor(this.dlP);md0=Math.sin(TP*this.dlP)*Q.md;md1=-md0}
    if(Q.m===0){const m=.5*(l+r);const a=B[0].get(this.dlT[0]+md0),b=B[0].get(this.dlT[1]+md1);F[0]+=Q.lp*(a-F[0]);
      B[0].put(this.qz(m+F[0]*Q.fb,4096));o[0]=a;o[1]=b}                                                         // two taps of one 12-bit memory, A left, B right
    else if(Q.m===1){if(Q.type===3){const T_=Math.max(64,Math.round(this.dlT[0]));if(++this.dlRc>=T_){this.dlRc=0;this.dlRs=B[0].w}const c=this.dlRc,c2=(c+(T_>>1))%T_;   // whole samples: an odd window must not read between samples
       for(let ch=0;ch<2;ch++){const b=B[ch];const y=b.at(this.dlRs-1-2*c)*hann(c/T_)+b.at(this.dlRs-1-2*c2+T_)*hann(c2/T_);F[ch]+=Q.lp*(y-F[ch]);b.put((ch?r:l)+F[ch]*Q.fb*.8);o[ch]=y}}
      else{const y=[B[0].get(this.dlT[0]+md0),B[1].get(this.dlT[1]+md1)];
       for(let ch=0;ch<2;ch++){let s=y[ch];F[ch]+=Q.lp*(s-F[ch]);s=F[ch];if(Q.hp){this.dlH[ch]+=Q.hp*(s-this.dlH[ch]);s-=this.dlH[ch]}
        if(Q.grit>0){s=Math.tanh(s*(1+Q.grit*4))/(1+Q.grit);const q=Math.pow(2,12-Q.grit*7);s=Math.round(s*q)/q}y[ch]=s}
       const pp=Q.type===2;B[0].put(l+(pp?y[1]:y[0])*Q.fb);B[1].put((pp?0:r)+(pp?y[0]:y[1])*Q.fb);o[0]=B[0].get(this.dlT[0]+md0);o[1]=B[1].get(this.dlT[1]+md1);if(Q.type===1){o[0]=.6*o[0]+.4*B[1].get(this.dlT[1]);o[1]=.6*o[1]+.4*B[0].get(this.dlT[1])}}}
    else if(Q.m===2){for(let ch=0;ch<2;ch++){const y=B[ch].get(this.dlT[ch]);const s=this.dlX[ch].run(y,Q.r[ch],Math.round(fs*.06));F[ch]+=Q.lp*(s-F[ch]);B[ch].put((ch?r:l)+F[ch]*Q.fb);o[ch]=s}}   // pitch shifter on every repeat
    else{for(let ch=0;ch<2;ch++){const x=Math.tanh((ch?r:l)*(1+Q.dr*6))/(1+Q.dr*2);const y=B[ch].get(this.dlT[ch]+(ch?md1:md0));F[ch]+=Q.lp*(y-F[ch]);B[ch].put(x+F[ch]*Q.fb);o[ch]=y}}
    break}
   // tape: one loop, several playback heads, wow and flutter, saturation on record, darker and thinner repeats
   this.dlU+=(Q.u-this.dlU)*.00025;this.dlW+=.55/fs;this.dlWf+=6.3/fs;this.dlW-=Math.floor(this.dlW);this.dlWf-=Math.floor(this.dlWf);
   const wob=1+Q.wow*(.004*Math.sin(TP*this.dlW)+.0012*Math.sin(TP*this.dlWf));const m=.5*(l+r);let inx=m;if(Q.pre){inx=Math.tanh(m*(1+Q.pre*3))/(1+Q.pre*.8);inx+=Q.pre*.3*(inx-this.dlB[1]);this.dlB[1]=inx}
   const n=Q.h.length;let sl=0,sr=0,last=0,sum=0;
   for(let k=0;k<n;k++){const y=B[0].get(Q.h[k]*this.dlU*wob);const pa=n>1?k/(n-1):.5;sl+=y*(1-pa*.7);sr+=y*(.3+pa*.7);sum+=y;last=y}
   const fbs=Q.mode===0?0:Q.mode===1?last:sum/Math.sqrt(n);F[0]+=Q.lp*(fbs-F[0]);this.dlH[0]+=.004*(F[0]-this.dlH[0]);
   B[0].put(Math.tanh((inx+(F[0]-this.dlH[0])*Q.fb)*1.2)/1.2);
   let ol=sl/Math.sqrt(n),or=sr/Math.sqrt(n);
   if(Q.m===6){this.dlB[0]+=Q.aB*(ol-this.dlB[0]);const lo=this.dlB[0];this.dlH[1]+=Q.aT*(ol-this.dlH[1]);const hi=ol-this.dlH[1];const g=x=>x+lo*Q.bass*.6+hi*Q.treb*.6;ol=g(ol);or=g(or);
     if(Q.rev>0){this.dlRv.run(l,r,.8,.35,this.dlRo);ol+=this.dlRo[0]*Q.rev*2.5;or+=this.dlRo[1]*Q.rev*2.5}}
   o[0]=ol+(this.rnd()-.5)*.0015;o[1]=or+(this.rnd()-.5)*.0015;break}
  case 6:{const Q=this.RV,M=Q.M;
   if(M.k===1){const S=this.rvSp;const m=Math.tanh(.5*(l+r)*Q.dr)/Math.sqrt(Q.dr);this.rvW+=1/this.fs*.7;const wob=Q.wob*Math.sin(TP*this.rvW);let L_=0,R_=0;
     for(let k=0;k<S.length;k++){const y=S[k].run(m*.6,Q.c*(1+k*.02),Q.g,Q.a,wob*(k?1:-1));if(k&1)R_+=y;else L_+=y}
     if(S.length===1)R_=L_;const n=Math.sqrt(S.length/2);L_/=n;R_/=n;if(M.diff){this.rvD[0]+=.5*(L_-this.rvD[0]);this.rvD[1]+=.5*(R_-this.rvD[1]);L_=.75*L_+.25*this.rvD[1];R_=.75*R_+.25*this.rvD[0]}
     if(M.lofi){const q=64;L_=Math.round(L_*q)/q;R_=Math.round(R_*q)/q}o[0]=L_*.8;o[1]=R_*.8}
   else{const P_=this.rvPd,I_=this.rvIn;for(let ch=0;ch<2;ch++){let x=ch?r:l;I_[ch]+=Q.bw*(x-I_[ch]);x=I_[ch];I_[2+ch]+=Q.hp*(x-I_[2+ch]);x-=I_[2+ch];P_[ch].put(x)}
     const il=P_[0].get(Q.pre+1),ir=P_[1].get(Q.pre+1);this.rvF.run(il,ir,this.rvO);let ol=this.rvO[0]*Q.lateG,or=this.rvO[1]*Q.lateG;
     if(Q.erG&&M.er){for(const e of M.er){const t=Q.pre+e[0]*this.fs/1000;const s=(P_[0].get(t)+P_[1].get(t))*.5*e[1]*Q.erG;ol+=s*(1-e[2])*.7;or+=s*(1+e[2])*.7}}
     if(M.bits){const q=Math.pow(2,M.bits+1);ol=this.qz(ol,q);or=this.qz(or,q)}o[0]=ol;o[1]=or}
   const mid=.5*(o[0]+o[1]),sd=.5*(o[0]-o[1])*Q.w;o[0]=mid+sd;o[1]=mid-sd;break}
  case 7:{const G=this.GR,m=.5*(l+r),fs=this.fs;if(!G.frz)this.grB.put(m+this.grY*G.fb);
   const win=t=>{const h=hann(t);if(G.win<.5){const tr=Math.min(1,t/.15,(1-t)/.15);const k=G.win*2;return tr*(1-k)+h*k}const k=(G.win-.5)*2;const pc=Math.min(1,t/.02)*Math.exp(-5*t);return h*(1-k)+pc*k};
   this.grC-=1;if(this.grC<=0){this.grC+=G.sync?G.int:G.int*(.6+.8*this.rnd());
     for(let k=0;k<this.grN;k++)if(!this.grA[k]){let st=G.st;if(G.warp)st+=Math.round((this.rnd()*2-1)*G.warp*12);let rate=Math.pow(2,st/12);
       let rv=G.rev&&this.rnd()<G.rev;if(G.sync)rv=G.pat===1||(G.pat===2&&this.rnd()<.5);if(rv)rate=-rate;
       const back=G.pos+G.len*Math.max(1,Math.abs(rate))+this.rnd()*G.spray+(G.scan?this.grS*fs*1.6:0)+4;
       const room=this.grB.n-G.len*Math.abs(rate)-16;const bk=Math.min(back,room);
       this.grA[k]=1;this.grL[k]=G.len;this.grT[k]=0;this.grR[k]=rate;this.grP[k]=this.grB.w-bk+(rate<0?G.len*Math.abs(rate):0);this.grG[k]=.5+(this.rnd()-.5)*G.spread;break}}
   let yl=0,yr=0,n=0;for(let k=0;k<this.grN;k++){if(!this.grA[k])continue;const t=this.grT[k]/this.grL[k];if(t>=1){this.grA[k]=0;continue}
     const pos=this.grP[k]+this.grT[k]*this.grR[k];const i=Math.floor(pos),fr=pos-i;const s=(this.grB.at(i)*(1-fr)+this.grB.at(i+1)*fr)*win(t);const pa=this.grG[k];yl+=s*(1-pa);yr+=s*pa;this.grT[k]++;n++}
   const gn=(G.sync?2.6:1.6)/Math.max(1,Math.sqrt(n));yl*=gn;yr*=gn;this.grY=.5*(yl+yr);
   if(G.dr){yl=Math.tanh(yl*(1+G.dr*6))/(1+G.dr*1.5);yr=Math.tanh(yr*(1+G.dr*6))/(1+G.dr*1.5)}
   if(G.lp||G.hp){const F=this.grF;F[0].run(yl,G.fg,.9);F[1].run(yr,G.fg,.9);yl=G.lp?F[0].lpo:F[0].hpo;yr=G.lp?F[1].lpo:F[1].hpo}
   o[0]=yl;o[1]=yr;break}
  case 8:{const K=this.FK;
   if(K.m===0){for(let ch=0;ch<2;ch++){const x=ch?r:l;let y;
     if(K.res){const F=this.fkR[ch];F.run(x*.5+this.fkY[ch]*K.fb*.9,K.f[ch],.04+.5*(1-Math.min(1,K.fb)));y=F.bpo}      // resonator: a band pass that rings at TUNE
     else{const dl=this.fkD[ch];y=dl.get(K.d[ch]);}                                                                     // comb: a short delay tuned to TUNE
     let s=y;if(K.lp||K.hp){const F=this.fkF[ch];F.run(s,K.tg,.9);s=K.lp?F.lpo:F.hpo}s=Math.tanh(s*(1+K.dr*6))/(1+K.dr*1.5);
     if(!K.res)this.fkD[ch].put(x*.6+s*K.fb);this.fkY[ch]=s;o[ch]=s*.5}}
   else{for(let ch=0;ch<2;ch++){const x=K.kill?(this.rnd()-.5)*1e-4:(ch?r:l);const dl=this.fkD[ch];const v=dl.get(K.d*(ch?1.013:1));
     const F=this.fkF[ch];F.run(v,K.bg,.7);const loop=Math.tanh((F.bpo*1.6+v*.25)*K.fb);dl.put(x+loop);o[ch]=loop*K.vol}}   // the loop: delay, tone band, gain, saturation
   break}
  case 9:{const Q=this.BC;let x0=l,x1=r;
   if(Q.m===3&&Q.drive){const g=1+Q.drive*6;x0=Math.tanh(x0*g)/(1+Q.drive*3);x1=Math.tanh(x1*g)/(1+Q.drive*3)}
   this.bcC+=1;const per=Q.m===3&&Q.jit?Q.div*(1+(this.rnd()*2-1)*Q.jit*.8):Q.div;
   if(this.bcC>=per){this.bcC-=per;if(this.bcC>=per)this.bcC=0;this.bcH[0]=(Math.floor(x0*Q.q)+.5)/Q.q;this.bcH[1]=(Math.floor(x1*Q.q)+.5)/Q.q}   // mid-rise steps: even at 2 bits a quiet sound still toggles instead of vanishing
   let y0=this.bcH[0],y1=this.bcH[1];{const z=this.bcZ||(this.bcZ=[0,0,0,0]);const a0=y0-z[0]+.999*z[1];z[0]=y0;z[1]=a0;const a1=y1-z[2]+.999*z[3];z[2]=y1;z[3]=a1;y0=a0;y1=a1}   // remove the offset of the steps
   if(Q.m===0&&Q.gate>0){const e=Math.max(Math.abs(l),Math.abs(r));this.bcE+=(e>this.bcE?.02:.0005)*(e-this.bcE);const t=this.bcE>Q.gate?1:0;this.bcG+=(t-this.bcG)*(t?.02:.002);y0*=this.bcG;y1*=this.bcG}
   if(Q.m===1){this.bcB[0].put(y0);this.bcB[1].put(y1);if(this.bcSt){const n=this.bcSl,i=this.bcSi;const fd=Math.min(1,i/48,(n-i)/48);y0=this.bcB[0].at(this.bcSs+i)*fd;y1=this.bcB[1].at(this.bcSs+i)*fd;if(++this.bcSi>=n)this.bcSi=0}}
   if((Q.m===0&&(Q.lp||Q.hp))||Q.m===1||(Q.m===2&&Q.lp)){const F=this.bcF;F[0].run(y0,Q.fg,Q.k||.9);F[1].run(y1,Q.fg,Q.k||.9);y0=Q.hp?F[0].hpo:F[0].lpo;y1=Q.hp?F[1].hpo:F[1].lpo}
   if(Q.m===2){y0*=Q.vol;y1*=Q.vol}
   if(Q.m===3){if(this.bcDr>0){this.bcDr--;this.bcDg+=(0-this.bcDg)*.05}else this.bcDg+=(1-this.bcDg)*.01;const nz=(this.rnd()-.5)*Q.noise;y0=(y0+nz)*this.bcDg;y1=(y1+nz)*this.bcDg}
   o[0]=y0;o[1]=y1;break}
  case 10:{const Q=this.RM;this.rmP+=Q.f/this.fs;this.rmP-=Math.floor(this.rmP);
   if(Q.m===1&&Q.mode>0){const H=this.rmH||(this.rmH=[new Hilbert(),new Hilbert()]);const dir=Q.mode===1?-1:1;                               // frequency shifter: every partial moves by the same number of Hz
     for(let ch=0;ch<2;ch++){const ph=this.rmP*(ch?1+Q.sp*.5:1);const c=Math.cos(TP*ph),s=Math.sin(TP*ph);H[ch].run(ch?r:l);o[ch]=(H[ch].re*c-dir*H[ch].im*s)}
     break}
   const s=Math.sin(TP*this.rmP);const c=Q.sq?Math.tanh(s*10):s;
   let x0=l,x1=r;if(Q.drive){const g=1+Q.drive*5;x0=Math.tanh(x0*g)/(1+Q.drive*1.5);x1=Math.tanh(x1*g)/(1+Q.drive*1.5)}
   if(Q.m===1&&Q.sp>0){const c2=Math.sin(TP*this.rmP*(1+Q.sp*.5));o[0]=x0*c;o[1]=x1*c2}else{o[0]=x0*c*Q.vol;o[1]=x1*c*Q.vol}break}
  case 11:{const Q=this.ST;this.stB[0].put(l);this.stB[1].put(r);this.stX+=((this.stOn?1:0)-this.stX)*.004;if(this.stX<1e-4){o[0]=l;o[1]=r;break}
   if(Q.m===1&&this.stOn){this.stGt++;const g=Math.min(1,this.stGt/Q.dur);if(g>=1){const mode=Math.round(this.p[this.I.f11smode]);if(mode===1&&this.p[this.I.f11on]>.5){this.stGt=0;this.stStart=this.stB[0].w-Q.l0;this.stLen=Q.l0;this.stLenT=Q.l0;this.stI=0;this.stPos=0;this.stRep=0}else{this.stOn=false;this.stG=-1}}
     const k=Q.cv<.5?Math.pow(g,.4+Q.cv*1.2):Math.pow(g,1+(Q.cv-.5)*4);this.stLenT=Math.max(64,Math.round(Q.l0+(Q.l1-Q.l0)*k))}
   const n=this.stLen,i=this.stI;const play=n*Q.gate;const gain=Math.pow(1-Q.fade*.5,this.stRep);
   let yl=0,yr=0;if(i<play){const pos=this.stPos;const fade=Math.min(1,i/64,(play-i)/64);const j=Math.floor(pos),fr=pos-j;const b0=this.stB[0],b1=this.stB[1],s0=this.stStart;
     yl=(b0.at(s0+j)*(1-fr)+b0.at(s0+j+1)*fr)*fade*gain;yr=(b1.at(s0+j)*(1-fr)+b1.at(s0+j+1)*fr)*fade*gain}
   this.stPos+=Math.pow(2,Q.pitch*this.stRep/12);if(++this.stI>=n){this.stI=0;this.stPos=0;this.stRep++;if(Q.m===1&&this.stLenT)this.stLen=Math.min(this.stLenT,this.stB[0].n-16)}
   o[0]=l+(yl-l)*this.stX;o[1]=r+(yr-r)*this.stX;break}
  case 12:{const Q=this.RE,B=this.reB,fs=this.fs;
   if(Q.m===0){if(!Q.hold){B[0].put(l);B[1].put(r);this.reHold=-1}else if(this.reHold<0)this.reHold=B[0].w;const anchor=this.reHold<0?B[0].w:this.reHold;
     let yl=0,yr=0;for(const h of this.reH){if(h.c<=0||h.c>=h.d){if(h.half&&!h.go){h.go=1;h.c=-Math.round(Q.T/Math.abs(Q.rate)/2)}                       // the second head starts half a pass later
       if(h.c>=h.d||h.c===0){let rt=Q.rate;if(Q.rand&&this.rnd()<Q.rand){const S=[-2,-1,-.5,.5,1,2];rt=S[Math.floor(this.rnd()*6)]}h.r=rt;h.d=Math.max(64,Math.round(Q.T/Math.abs(rt)));h.p=rt<0?anchor-1:anchor-Q.T;h.c=0}}
      if(h.c>=0){const g=hann(h.c/h.d);const j=Math.floor(h.p),fr=h.p-j;yl+=(B[0].at(j)*(1-fr)+B[0].at(j+1)*fr)*g;yr+=(B[1].at(j)*(1-fr)+B[1].at(j+1)*fr)*g;h.p+=h.r}h.c++}
     o[0]=yl;o[1]=yr;break}
   // reverse delay: two heads, each plays the last TIME window backward, offset by half a window so their fades overlap
   const T=Q.T;let md=0;if(Q.md){this.reL+=Q.mr/fs;this.reL-=Math.floor(this.reL);md=Math.sin(TP*this.reL)*Q.md}
   let yl=0,yr=0;for(let k=0;k<2;k++){const h=this.reH[k];if(h.r!==-2){h.r=-2;h.c=k?Math.round(T/2):0;h.s=B[0].w}if(h.c>=T){h.c=0;h.s=B[0].w}
     const g=hann(h.c/T);const pos=h.s-1-h.c+md*g;const j=Math.floor(pos),fr=pos-j;yl+=(B[0].at(j)*(1-fr)+B[0].at(j+1)*fr)*g;yr+=(B[1].at(j)*(1-fr)+B[1].at(j+1)*fr)*g;h.c++}
   this.reF[0]+=Q.a*(yl-this.reF[0]);this.reF[1]+=Q.a*(yr-this.reF[1]);B[0].put(l+this.reF[0]*Q.fb);B[1].put(r+this.reF[1]*Q.fb);o[0]=yl;o[1]=yr;break}
  case 13:{const Q=this.FZ;this.fzH[0].put(l);this.fzH[1].put(r);let yl=0,yr=0;
   for(const L of this.fzLy){if(L.st===0||L.n<8)continue;if(L.st===1){L.env+=(1-L.env)*Q.att;L.env*=Q.dec}else{L.env-=L.env*Q.rel;if(L.env<1e-4){L.st=0;continue}}
    const n=L.n,G=Math.min(Q.G,n);for(const h of L.h){if(h.half&&!h.go){h.go=1;h.c=-Math.round(G/2)}if(h.c>=0&&(h.c===0||h.c>=G)){h.c=0;h.s=Q.m===0?0:this.rnd()*(n-G);h.p=0}
     if(h.c>=0){const g=hann(h.c/G);let pos=h.s+h.p;pos%=n;const j=Math.floor(pos),fr=pos-j,j2=j+1>=n?0:j+1;yl+=(L.b[0][j]*(1-fr)+L.b[0][j2]*fr)*g*L.env;yr+=(L.b[1][j]*(1-fr)+L.b[1][j2]*fr)*g*L.env;h.p+=this.fzR}h.c++}}
   yl*=Q.lvl;yr*=Q.lvl;if(Q.m===0&&(Q.lp||Q.hp)){const F=this.fzF;F[0].run(yl,Q.tg,.9);F[1].run(yr,Q.tg,.9);yl=Q.lp?F[0].lpo:F[0].hpo;yr=Q.lp?F[1].lpo:F[1].hpo}
   o[0]=l+yl;o[1]=r+yr;break}
  case 14:{const Q=this.TP,fs=this.fs,ts=(u,a)=>Math.tanh(u+a)-Math.tanh(a);   // tape saturation: soft, slightly lopsided
   if(Q.m===0){if(this.tpLg<0)this.tpLg=Q.lag;this.tpLg+=(Q.lag-this.tpLg)*.0004;   // the lag deck speeds up or slows down to reach a new LAG TIME, like a real tape
     this.tpW+=.7/fs;this.tpW-=Math.floor(this.tpW);this.tpWt+=1/fs;if(this.tpWt>.25){this.tpWt=0;this.tpN=this.rnd()*2-1}this.tpWb+=(this.tpN-this.tpWb)*.00012;
     const dl=this.tpLg+Q.wob*(.6*Math.sin(TP*this.tpW)+.6*this.tpWb);const X=[l,r],R=[0,0],G=[0,0];
     for(let ch=0;ch<2;ch++){let y=X[ch];if(Q.son)y=ts(y*Q.g,.05);y*=Q.out;this.tpT[ch]+=Q.ta*(y-this.tpT[ch]);y=this.tpT[ch]*Math.pow(1.6,-Q.t*.5)+(y-this.tpT[ch])*Math.pow(4,Q.t);
       if(!Q.dbl){o[ch]=y;continue}this.tpR[ch].put(y);this.tpD[ch].put(y+this.tpY[ch]*Q.fb);R[ch]=this.tpR[ch].get(Q.ref);let g=this.tpD[ch].get(dl);this.tpYl[ch]+=Q.la*(g-this.tpYl[ch]);g=this.tpYl[ch];this.tpY[ch]=g;G[ch]=g}
     if(Q.dbl){if(Q.wide){o[0]=(Q.gr*R[0]+.3*Q.s*Q.gl*G[0])*Q.nm;o[1]=(Q.s*Q.gl*G[1]+.3*Q.gr*R[1])*Q.nm}else{o[0]=(Q.gr*R[0]+Q.s*Q.gl*G[0])*Q.nm;o[1]=(Q.gr*R[1]+Q.s*Q.gl*G[1])*Q.nm}}}
   else if(Q.m===1){this.tpW+=.55/fs;this.tpF+=7.5/fs;this.tpF2+=11.3/fs;this.tpW-=Math.floor(this.tpW);this.tpF-=Math.floor(this.tpF);this.tpF2-=Math.floor(this.tpF2);
     this.tpWt+=1/fs;if(this.tpWt>.4){this.tpWt=0;this.tpN=this.rnd()*2-1}this.tpWb+=(this.tpN-this.tpWb)*.00008;
     const d=fs*(.012+Q.wow*.004*(.7*Math.sin(TP*this.tpW)+.5*this.tpWb)+Q.fl*.00035*(Math.sin(TP*this.tpF)+.5*Math.sin(TP*this.tpF2)));
     const drop=cl(1-Q.wear*1.6*Math.max(0,Math.abs(this.tpWb)-.35),.25,1);const X=[l,r];   // WEAR: the level dips where the tape is worn
     for(let ch=0;ch<2;ch++){let y=Q.h*ts(X[ch]*Q.g/Q.h,.08)*Q.out;this.tpC[ch].put(y);y=this.tpC[ch].get(d)*drop+(this.rnd()-.5)*Q.hiss;
       this.tpL1[ch]+=Q.la*(y-this.tpL1[ch]);this.tpL2[ch]+=Q.la*(this.tpL1[ch]-this.tpL2[ch]);y=this.tpL2[ch];this.tpHP[ch]+=Q.ha*(y-this.tpHP[ch]);y-=this.tpHP[ch];
       this.tpT[ch]+=Q.ta*(y-this.tpT[ch]);o[ch]=this.tpT[ch]*Math.pow(1.6,-Q.t*.5)+(y-this.tpT[ch])*Math.pow(4,Q.t)}}
   else{const X=[l,r];for(let ch=0;ch<2;ch++){let y=X[ch]*Q.gi;y=Q.h*ts(y/Q.h,Q.as);const B=this.tpB[ch];B.run(y,Q.bg,Q.bk);y+=B.bpo*Q.bm;   // head bump: a low bump that moves with the tape speed
       y+=(this.rnd()-.5)*Q.hiss;const H=this.tpH[ch];y=H.run(y,Q.lg,1.3);this.tpHP[ch]+=Q.ha*(y-this.tpHP[ch]);o[ch]=(y-this.tpHP[ch])*Q.out}}
   break}
  case 15:{const Q=this.SH,P_=this.shPd,O=this.shO;P_[0].put(l);P_[1].put(r);let il=P_[0].get(Q.pre+1),ir=P_[1].get(Q.pre+1);
   if(Q.m===2){il=Math.tanh(il*Q.dw)/Q.dwn;ir=Math.tanh(ir*Q.dw)/Q.dwn}   // DWELL drives the input
   const world=(a,b)=>{const W=this.shWo;if(Q.wt===2){const S=this.shSp;W[0]=S[0].run(a*.5,.6,Q.sg[0],Q.sa,0)*1.4;W[1]=S[1].run(b*.5,.6,Q.sg[1],Q.sa,0)*1.4}else this.shW.run(a,b,W);return W};
   if(Q.m===2&&Q.rt===2&&Q.wl>0){const W=world(il,ir);il+=(W[0]*1.6-il)*Q.wl;ir+=(W[1]*1.6-ir)*Q.wl}   // WORLD > DARK
   this.shF.run(il+this.shY[0],ir+this.shY[1],O);
   // the shimmer: the reverb output, pitch shifted, filtered and fed back into the reverb
   for(let ch=0;ch<2;ch++){let s=0;for(let k=0;k<Q.v.length;k++)s+=this.shS[k][ch].run(O[ch],Q.v[k],Q.win);s*=Q.gn;
     this.shL[ch]+=Q.la*(s-this.shL[ch]);s=this.shL[ch];this.shH[ch]+=Q.ha*(s-this.shH[ch]);s-=this.shH[ch];
     if(Q.fl){const F=this.shLF[ch];F.run(s,Q.fg,1.2);s=Q.fl===1?F.lpo:F.hpo}this.shY[ch]=Math.tanh(s*Q.amt*1.1)}
   let ol=O[0]*Q.out,or=O[1]*Q.out;
   if(Q.m===1&&Q.fl){const A=this.shOF;A[0].run(ol,Q.fg,1.2);A[1].run(or,Q.fg,1.2);ol=Q.fl===1?A[0].lpo:A[0].hpo;or=Q.fl===1?A[1].lpo:A[1].hpo}
   if(Q.m===2&&Q.rt!==2&&Q.wl>0){const W=Q.rt===0?world(O[0],O[1]):world(il,ir);   // DARK > WORLD or PARALLEL
     if(Q.rt===0){ol+=(W[0]*Q.out*1.3-ol)*Q.wl;or+=(W[1]*Q.out*1.3-or)*Q.wl}else{ol+=W[0]*Q.wl*.8;or+=W[1]*Q.wl*.8}}
   o[0]=Math.tanh(ol*.7)/.7;o[1]=Math.tanh(or*.7)/.7;break}   // a soft ceiling: long tails with a lot of shimmer keep building
  }}
 process(ins,outs){const o=outs[0],L=o[0],R=o[1]||o[0],n=L.length;const inp=ins&&ins[0]&&ins[0].length?ins[0]:null;
  for(let i=0;i<n;i++){if(this.cc===0)this.ctrl();this.cc=(this.cc+1)%CT;
   let l,r;if(this.src0===0){const d=this.demo(),k=this.kbd();l=d[0]+k[0];r=d[1]+k[1]}else if(this.src0===1){const d=this.kbd();l=d[0];r=d[1]}else{l=inp?inp[0][i]:0;r=inp?(inp[1]||inp[0])[i]:l}
   l*=this.gain;r*=this.gain;const a=Math.abs(l)+Math.abs(r);this.env+=(a>this.env?.01:.0004)*(a-this.env);
   const dl=l,dr=r;
   for(let k=0;k<this.active.length;k++){const q=this.active[k],mt=this.mixT[q];this.mixS[q]+=(mt-this.mixS[q])*.002;const m=this.mixS[q];if(m<1e-4&&mt<=0)continue;
    this.units[q].fx(this.slotF[q],l,r);l+=(this.o[0]-l)*m;r+=(this.o[1]-r)*m}
   l=dl+(l-dl)*this.dw;r=dr+(r-dr)*this.dw;
   this.ms+=.004*(this.mt-this.ms);if(!(l===l&&r===r&&Math.abs(l)<1e6&&Math.abs(r)<1e6)){l=r=0;this.bad=true}L[i]=Math.tanh(l*this.ms*1.3);R[i]=Math.tanh(r*this.ms*1.3)}
  if(this.bad){this.bad=false;this.initState()}
  if(++this.mc>=10){this.mc=0;this.port.postMessage({step:this.step,env:Math.min(1,this.env*3),mix:Array.from(this.mixS)})}
  return true}}
registerProcessor('vact',FXRack);`;

