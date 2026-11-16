// ===================== Parameter table: Octavio FX, the 16 effects of the FM-1 prototype in a 8-slot chain =====================
const D=[];const IDX={};
// each of the 8 slots has its own full set of effect controls, ids prefixed q0 .. q7 (so an effect can be in the chain more than once)
let __pf='',__pn='';const NS=8;   // boxes in the chain
function F(id,nm,mn,mx,df,fmt){id=__pf+id;nm=__pn+nm;IDX[id]=D.length;D.push({id,nm,t:'f',mn,mx,df,fmt:fmt||'pct'})}
function C(id,nm,ch,df){id=__pf+id;nm=__pn+nm;IDX[id]=D.length;D.push({id,nm,t:'c',mn:0,mx:ch.length-1,df,ch})}
function B(id,nm,df){id=__pf+id;nm=__pn+nm;IDX[id]=D.length;D.push({id,nm,t:'b',mn:0,mx:1,df:df?1:0})}
// the bank: same 16 effects and knobs as the firmware (OVERDRIVE .. SHIMMER), each effect's last knob is its MIX
const FXN=['OVERDRIVE','PHASER','FLANGER','CHORUS','MICRO PITCH','DELAY','REVERB','GRANULAR','FEEDBACK','BITCRUSH','RING MOD','STUTTER','REVERSE','FREEZE','TAPE','SHIMMER','SIDECHAIN'];
const NFX=FXN.length;   // SIDECHAIN (16): ducks on another part (the hardware synth), an LFO or a 16-step sequence
// OVERDRIVE has three models; each knob carries the model it belongs to (0 VULTURE, 1 HG-2, 2 BOUM)
const ODM=['VULTURE','HG-2','BOUM'],PHM=['MF-103','PHASE 100','INSTANT','BI-PHASE'],CHM=['CE-2','DIMENSION D','TRI-STEREO','JULIANNA'],DLM=['SUPER PRIME TIME','TIMELINE','DMX 15-80 S','SDD-3000','ECHOREC 2','EP-3','RE-201','COPICAT'],
 RVM=['480L','EMT 250','RMX 16','M7','BX20','XL-305','SUPER REVERB','EF-P2','FLINT','PLACE','CAPITOL','ABBEY ROAD','SUNSET SOUND','GOLD STAR','EMT 140','EMT 240','ECOPLATE III','RX4000','AUDICON','STUDIO 2','SUN STUDIO','ELECTRIC LADY A','CAPITOL A'];
const FLM=['CLUSTER FLUX','INSTANT FLANGER','FLANGER HOAX','STARLIGHT DLX','A/DA FLANGER'];
const MPM=['H910','H949'],GRM=['EVIL PET','MULTIGRAIN','GR-1','S-4 MOSAIC'],FBM=['RIFT','TSA 2'],BCM=['BITMAP','OTTOBIT JR','3 SERIES','DEGRADER'],RMM=["RANDY'S REVENGE",'RADIUS','MF-102','3 SERIES'],STM=['RYSUP','STUTTER EDIT 2'],RVSM=['TENSOR','SETBACK','DL4 REVERSE'],FZM=['I WISH','DEEP FREEZE','PLUS'],TPM=['DECO V2','CASSETTE 2','A800'],SHM=['BIGSKY MX','ASTRAL DESTINY','DARK WORLD'];
const RG={DIGITAL:[0,1,2,3],SPRING:[4,5,6,7,8,9],CHAMBER:[10,11,12,13],PLATE:[14,15,16,17,18],ROOM:[19,20,21,22]};
// delay models come in two groups, shown as two sub-tabs
const MGROUPS={5:[['DIGITAL',[0,1,2,3]],['TAPE',[4,5,6,7]]],6:Object.entries(RG)};
const MODELS={0:ODM,1:PHM,2:FLM,3:CHM,4:MPM,5:DLM,6:RVM,7:GRM,8:FBM,9:BCM,10:RMM,11:STM,12:RVSM,13:FZM,14:TPM,15:SHM};
// a knob belongs to one model (a number), to several (a list), or to all (nothing)
const ownsKnob=(md,m)=>md==null||(Array.isArray(md)?md.includes(m):md===m);
const knobOwner=(f,md)=>!MODELS[f]||md==null?FXN[f]:Array.isArray(md)?(MGROUPS[f]?MGROUPS[f].find(g=>g[1].includes(md[0]))[0]:FXN[f]):MODELS[f][md];   // effects with several models: their knobs carry the model index
const FXK=[[['vdrive','DRIVE','pct',0],['vbias','BIAS','bip',0],['vlow','LOW','bip',0],['vhigh','HIGH','bip',0],['vout','OUTPUT','outdb',0],
  ['hsat','SATURATION','pct',1],['hdens','DENSITY','pct',1],['hair','AIR','pct',1],['hblend','BLEND','pct',1],['hout','OUTPUT','outdb',1],
  ['bdrive','DRIVE','pct',2],['bcomp','COMPRESS','pct',2],['bfilt','FILTER','filt',2],['bgate','GATE','pct',2],['bout','OUTPUT','outdb',2]],[['mrate','RATE','mrate',0],['msweep','SWEEP','pct',0],['mres','RESONANCE','pct',0],['mdrive','DRIVE','pct',0],
  ['xspeed','SPEED','xspeed',1],
  ['irate','RATE','irate',2],['idepth','DEPTH','pct',2],['ifb','FEEDBACK','bip',2],['iman','MANUAL','pct',2],['isens','SENSITIVITY','pct',2],
  ['brate1','OSC 1 RATE','brate',3],['brate2','OSC 2 RATE','brate',3],['bdepa','DEPTH A','pct',3],['bfba','FEEDBACK A','pct',3],['bdepb','DEPTH B','pct',3],['bfbb','FEEDBACK B','pct',3],
  ['stereo','STEREO','deg'],['width','WIDTH','width']],
 [['utime','TIME','fltime',0],['urate','RATE','rate',0],['uamt','AMOUNT','pct',0],['ufb','FEEDBACK','bip',0],['udrive','DRIVE','pct',0],
  ['irate','RATE','irate',1],['idepth','DEPTH','pct',1],['ifb','FEEDBACK','bip',1],['iman','MANUAL','pct',1],['isens','SENSITIVITY','pct',1],
  ['hrate','RATE','rate',2],['hdepth','DEPTH','pct',2],['hman','MANUAL','pct',2],['hres','RESONANCE','pct',2],['hblend','BLEND','pct',2],
  ['srate','RATE','rate',3],['sdepth','DEPTH','pct',3],['sman','MANUAL','pct',3],['sregen','REGEN','bip',3],['ssens','SENSITIVITY','pct',3],
  ['aspeed','SPEED','rate',4],['arange','RANGE','pct',4],['aman','MANUAL','pct',4],['aenh','ENHANCE','pct',4],['athr','THRESHOLD','pct',4],
  ['stereo','STEREO','deg'],['width','WIDTH','width']],
 [['crate','RATE','crate',0],['cdepth','DEPTH','pct',0],
  ['trate','RATE','trate',2],['tdepth','DEPTH','pct',2],['ttone','TONE','pct',2],
  ['jrate','RATE','jrate',3],['jdepth','DEPTH','pct',3],['jlag','LAG','lag',3],['jdpv','D-P-V','dpv',3],['jphase','PHASE','deg',3],
  ['width','WIDTH','width']],
 [['tdet','DETUNE','cents',0],['tpitch','PITCH','pst',0],['tdel','DELAY','ms112',0],['tfb','FEEDBACK','pct',0],
  ['ndet','DETUNE','cents',1],['npitch','PITCH','pst',1],['ndel','DELAY','ms200',1],['nfb','FEEDBACK','pct',1],['nrand','RANDOM','pct',1],
  ['width','WIDTH','width']],
 [['pta','DELAY A','dtime',0],['ptb','DELAY B','dtime',0],['pfb','FEEDBACK','pct',0],['pmr','MOD RATE','trate',0],['pmd','MOD DEPTH','pct',0],['ptone','TONE','pct',0],
  ['ltime','TIME','dtime',1],['lrep','REPEATS','pct',1],['lmod','MOD','pct',1],['lfilt','FILTER','filt',1],['lgrit','GRIT','pct',1],
  ['xtl','TIME L','dtime',2],['xtr','TIME R','dtime',2],['xfb','FEEDBACK','pct',2],['xpitch','PITCH','pst',2],['xdet','DETUNE','cents',2],
  ['stime','TIME','dtime',3],['sfb','FEEDBACK','pct',3],['srate','MOD RATE','trate',3],['smod','MOD DEPTH','pct',3],['sdamp','HIGH DAMP','pct',3],['sdrive','DRIVE','pct',3],
  ['espeed','SPEED','espeed',4],['eswell','SWELL','pct',4],['etone','TONE','bip',4],
  ['htime','TIME','eptime',5],['hsus','SUSTAIN','pct',5],['hpre','PREAMP','pct',5],['hwow','WOW','pct',5],
  ['rrate','REPEAT RATE','rerate',6],['rint','INTENSITY','pct',6],['rbass','BASS','bip',6],['rtreb','TREBLE','bip',6],['rrev','REVERB','pct',6],['rwow','WOW','pct',6],
  ['cspeed','SPEED','cspeed',7],['cswell','SWELL','pct',7],['ctone','TONE','bip',7],['cdrive','VALVE DRIVE','pct',7],['cwow','WOW','pct',7]],
 [['ddecay','DECAY','rt1',RG.DIGITAL],['dpre','PRE-DELAY','ms250',RG.DIGITAL],['dsize','SIZE','pct',RG.DIGITAL],['ddamp','HIGH DAMP','pct',RG.DIGITAL],['dlow','LOW DECAY','lowm',RG.DIGITAL],['dmod','MOD','pct',RG.DIGITAL],
  ['sdwell','DWELL','pct',RG.SPRING],['sdecay','DECAY','pct',RG.SPRING],['stone','TONE','bip',RG.SPRING],
  ['cdecay','DECAY','rt3',RG.CHAMBER],['cpre','PRE-DELAY','ms250',RG.CHAMBER],['ctone','TONE','bip',RG.CHAMBER],['cmic','MIC DISTANCE','pct',RG.CHAMBER],
  ['pdecay','DAMPER','rt2',RG.PLATE],['ppre','PRE-DELAY','ms250',RG.PLATE],['ptone','TONE','bip',RG.PLATE],
  ['odecay','DECAY','rt4',RG.ROOM],['opre','PRE-DELAY','ms250',RG.ROOM],['odist','MIC DISTANCE','pct',RG.ROOM],['otone','TONE','bip',RG.ROOM],
  ['width','WIDTH','width']],
 [['size','GRAIN SIZE','gsize'],['pitch','PITCH','gpitch'],['spray','SPRAY','pct'],
  ['dens','DENSITY','gdens',[0,1,2]],['pos','POSITION','gpos',[0,1,2]],['rate','RATE','gnote',3],
  ['drive','DRIVE','pct',0],['filt','FILTER','filt',[0,2]],['tex','WINDOW','wshape',[1,2]],['contour','CONTOUR','wshape',3],
  ['rev','REVERSE','pct',1],['fb','FEEDBACK','pct',1],['scan','SCAN','bip',2],['spread','SPREAD','pct',2],['warp','WARP','pct',3]],
 [['rtune','TUNE','fbhz',0],['rfb','FEEDBACK','pct',0],['rdrive','DRIVE','pct',0],['rtone','TONE','filt',0],['rspread','SPREAD','cents',0],
  ['tfb','FEEDBACK','pct',1],['ttone','TONE','pct',1],['tloop','LOOP','loopms',1],['tvol','VOLUME','pct',1]],
 [['crush','BITS','bits'],['rate','SAMPLE RATE','srdiv'],
  ['bfilt','FILTER','filt',0],['bgate','GATE','pct',0],
  ['ofilt','FILTER','lpf',1],['ostut','STUTTER','pct',1],['oseq','SEQUENCE','pct',1],
  ['jvol','VOLUME','pct',2],
  ['djit','JITTER','pct',3],['dnoise','NOISE','pct',3],['ddrop','DROPOUT','pct',3],['ddrive','DRIVE','pct',3]],
 
 [['freq','FREQUENCY','rmfreq'],
  ['rlrate','LFO RATE','rate',0],['rldepth','LFO DEPTH','pct',0],
  ['xfine','FINE','bip',1],['xspread','SPREAD','pct',1],
  ['mlrate','LFO RATE','rate',2],['mlamt','LFO AMOUNT','pct',2],['mdrive','DRIVE','pct',2],
  ['jvol','VOLUME','pct',3]],
 [['gate','GATE','pct'],
  ['rrate','RATE','stdiv',0],['rpitch','PITCH','stpitch',0],['rfade','FADE','pct',0],['rchance','CHANCE','pct',0],
  ['sdur','DURATION','sedur',1],['sstart','START RATE','stdiv',1],['send','END RATE','stdiv',1],['scurve','CURVE','curve',1]],
 [['time','TIME','rtime'],
  ['tspeed','SPEED','speed',0],['trand','RANDOM','pct',0],
  ['fb','REPEATS','pct',[1,2]],['smod','MOD','pct',1],['stone','TONE','pct',1],
  ['dtweak','TWEAK','pct',2],['dtweez','TWEEZ','pct',2]],
 [['wsize','SIZE','wsize',0],['wpitch','PITCH','pst',0],['wglide','GLIDE','pct',0],['wtone','TONE','filt',0],
  ['plevel','LEVEL','pct',1],['pin','FADE IN','ftime',1],['pout','FADE OUT','ftime',1],
  ['xsus','SUSTAIN','sustime',2],['xrise','RISE','ftime',2],['xtail','TAIL','ftime',2],['xlevel','LEVEL','pct',2]],
 [['dsat','SATURATION','pct',0],['dvol','VOLUME','outdb',0],['dtone','TONE','bip',0],['dlag','LAG TIME','declag',0],['dwob','WOBBLE','pct',0],['dblend','BLEND','blend',0],
  ['cdrive','DRIVE','pct',1],['cwow','WOW','pct',1],['cflut','FLUTTER','pct',1],['chiss','HISS','pct',1],['ctone','TONE','bip',1],['cwear','WEAR','pct',1],
  ['ain','INPUT','pct',2],['aout','OUTPUT','outdb',2],['abias','BIAS','bip',2],['ahiss','HISS','pct',2]],[['bdecay','DECAY','shrt',0],['bpre','PRE-DELAY','ms500',0],['btone','TONE','bip',0],['bmod','MOD','pct',0],['bamt','AMOUNT','pct',0],
  ['alen','LENGTH','shrt',1],['afilt','FILTER','filt',1],['arate','RATE','pct',1],['adepth','DEPTH','pct',1],['ashim','SHIMMER','pct',1],
  ['kdecay','DECAY','shrt',2],['kdwell','DWELL','pct',2],['kmod','MODIFY','pct',2],['ktone','TONE','bip',2],['kpre','PRE-DELAY','ms500',2],['kworld','WORLD','pct',2]],
 [['att','ATTACK','scatt'],['dec','DECAY','scdec'],['amt','AMOUNT','pct']]];
const FXDF={'f16att':.007,'f16dec':.6,'f16amt':.7,'f1mrate':.3,'f1msweep':.6,'f1mres':.4,'f1mdrive':.2,'f1xspeed':.35,'f1irate':.3,'f1idepth':.7,'f1ifb':.4,'f1iman':.5,'f1isens':.5,'f5pta':.55,'f5ptb':.72,'f5pfb':.35,'f5pmr':.3,'f5pmd':.2,'f5ptone':.6,'f5ltime':.55,'f5lrep':.4,'f5lmod':.2,'f5lfilt':0,'f5lgrit':0,'f5xtl':.55,'f5xtr':.72,'f5xfb':.35,'f5xpitch':.5,'f5xdet':.1,'f5stime':.55,'f5sfb':.4,'f5srate':.3,'f5smod':.15,'f5sdamp':.4,'f5sdrive':.2,'f5espeed':.5,'f5eswell':.4,'f5etone':0,'f5htime':.4,'f5hsus':.4,'f5hpre':.4,'f5hwow':.3,'f5rrate':.5,'f5rint':.45,'f5rbass':0,'f5rtreb':0,'f5rrev':.2,'f5rwow':.3,'f5cspeed':.5,'f5cswell':.4,'f5ctone':0,'f5cdrive':.3,'f5cwow':.3,'f3crate':.3,'f3cdepth':.6,'f3trate':.3,'f3tdepth':.5,'f3ttone':.6,'f3jrate':.3,'f3jdepth':.5,'f3jlag':.3,'f3jdpv':.5,'f3jphase':.5,'f3width':.5,'f1brate1':.3,'f1brate2':.45,'f1bdepa':.7,'f1bfba':.4,'f1bdepb':.7,'f1bfbb':.4,'f1stereo':.5,'f1width':.5,'f0vdrive':.45,'f0vbias':0,'f0vlow':0,'f0vhigh':0,'f0vout':.5,'f0hsat':.35,'f0hdens':.4,'f0hair':.2,'f0hblend':1,'f0hout':.5,'f0bdrive':.5,'f0bcomp':.4,'f0bfilt':0,'f0bgate':0,'f0bout':.5,'f1rate':.3,'f4tdet':.2,'f4tpitch':.5,'f4tdel':.08,'f4tfb':0,'f4ndet':.2,'f4npitch':.5,'f4ndel':.06,'f4nfb':0,'f4nrand':0,'f4width':.5,'f10freq':.45,'f10rlrate':.3,'f10rldepth':0,'f10xfine':0,'f10xspread':.3,'f10mlrate':.3,'f10mlamt':0,'f10mdrive':.2,'f10jvol':.5,'f13wsize':.3,'f13wpitch':.5,'f13wglide':.2,'f13wtone':0,'f13plevel':.7,'f13pin':.3,'f13pout':.5,'f13xsus':.7,'f13xrise':.25,'f13xtail':.55,'f13xlevel':.7,'f12tspeed':.25,'f12trand':0,'f12smod':.3,'f12stone':.6,'f12dtweak':.2,'f12dtweez':.3,'f11gate':1,'f11rrate':.6,'f11rpitch':.5,'f11rfade':0,'f11rchance':.3,'f11sdur':.6,'f11sstart':.2,'f11send':.8,'f11scurve':.5,'f9bfilt':0,'f9bgate':0,'f9ofilt':.8,'f9ostut':0,'f9oseq':0,'f9jvol':.5,'f9djit':.2,'f9dnoise':.15,'f9ddrop':0,'f9ddrive':.2,'f8rtune':.4,'f8rfb':.6,'f8rdrive':.3,'f8rtone':0,'f8rspread':.2,'f8tfb':.35,'f8ttone':.5,'f8tloop':.3,'f8tvol':.6,'f2utime':.4,'f2urate':.25,'f2uamt':.6,'f2ufb':.5,'f2udrive':.1,'f2irate':.25,'f2idepth':.7,'f2ifb':.4,'f2iman':.5,'f2isens':.5,'f2hrate':.25,'f2hdepth':.6,'f2hman':.4,'f2hres':.5,'f2hblend':.5,'f2srate':.25,'f2sdepth':.6,'f2sman':.4,'f2sregen':.5,'f2ssens':.5,'f2aspeed':.25,'f2arange':.8,'f2aman':.3,'f2aenh':.6,'f2athr':.3,'f2stereo':.5,'f2width':.5,'f3rate':.3,'f4cents':.3,'f5time':.45,'f5fb':.4,'f6ddecay':.55,'f6dpre':.08,'f6dsize':.5,'f6ddamp':.35,'f6dlow':.5,'f6dmod':.4,'f6sdwell':.4,'f6sdecay':.55,'f6stone':0,'f6cdecay':.5,'f6cpre':.06,'f6ctone':0,'f6cmic':.5,'f6pdecay':.5,'f6ppre':.04,'f6ptone':0,'f6odecay':.5,'f6opre':.0,'f6odist':.5,'f6otone':0,'f6width':.5,'f7size':.4,'f7dens':.5,'f7pitch':.5,'f7spray':.3,'f7pos':.2,'f7rate':.4,'f7drive':.2,'f7filt':0,'f7tex':.5,'f7contour':.5,'f7rev':.2,'f7fb':0,'f7scan':0,'f7spread':.6,'f7warp':.1,'f8freq':.4,'f8fb':.6,'f9crush':.5,'f9rate':.3,'f10freq':.35,'f10shape':0,'f11len':.5,'f11chance':.5,'f12time':.5,'f12fb':.3,'f13size':.5,'f13hold':0,'f14dsat':.35,'f14dvol':.5,'f14dtone':0,'f14dlag':.45,'f14dwob':.3,'f14dblend':.5,'f14cdrive':.4,'f14cwow':.35,'f14cflut':.3,'f14chiss':.25,'f14ctone':0,'f14cwear':.15,'f14ain':.5,'f14aout':.5,'f14abias':0,'f14ahiss':.1,'f15bdecay':.55,'f15bpre':.06,'f15btone':0,'f15bmod':.3,'f15bamt':.5,'f15alen':.55,'f15afilt':0,'f15arate':.3,'f15adepth':.3,'f15ashim':.5,'f15kdecay':.5,'f15kdwell':.4,'f15kmod':.5,'f15ktone':0,'f15kpre':.04,'f15kworld':.4};
const FXMIX0={0:.35,3:.4,5:.3,6:.3,16:1};
function defFx(){
C('f0model','OVERDRIVE MODEL',ODM,0);C('f1model','PHASER MODEL',PHM,0);C('f1mstages','MF-103 STAGES',['6','12'],0);C('f1xint','PHASE 100 INTENSITY',['1','2','3','4'],1);C('f1isrc','INSTANT SWEEP',['LFO','ENVELOPE','MANUAL'],0);C('f3model','CHORUS MODEL',CHM,0);C('f5model','DELAY MODEL',DLM,1);C('f2model','FLANGER MODEL',FLM,0);C('f4model','MICRO PITCH MODEL',MPM,0);C('f7model','GRANULAR MODEL',GRM,0);C('f8model','FEEDBACK MODEL',FBM,0);C('f9model','BITCRUSH MODEL',BCM,0);C('f10model','RING MOD MODEL',RMM,0);C('f11model','STUTTER MODEL',STM,0);C('f12model','REVERSE MODEL',RVSM,0);C('f13model','FREEZE MODEL',FZM,1);C('f14model','TAPE MODEL',TPM,0);C('f15model','SHIMMER MODEL',SHM,0);C('f15bpitch','BIGSKY PITCH',['+OCT','+5TH','+OCT +5TH','\u2212OCT','+OCT \u2212OCT'],0);C('f15amode','ASTRAL MODE',['OCT UP','OCT DOWN','UP + DOWN','5TH + OCT','2 OCT UP','ASTRAL'],0);B('f15aramp','ASTRAL RAMP',0);C('f15kworldt','DARK WORLD WORLD',['HALL','PLATE','SPRING'],0);C('f15kroute','DARK WORLD ROUTING',['DARK > WORLD','PARALLEL','WORLD > DARK'],1);B('f14dson','DECO SATURATION',1);B('f14ddbl','DECO DOUBLETRACK',1);C('f14dtype','DECO TYPE',['SUM','INVERT','BOUNCE'],0);B('f14dwide','DECO WIDE',0);C('f14ctype','CASSETTE TYPE',['NORMAL','CHROME'],0);C('f14aspeed','A800 SPEED',['7.5 IPS','15 IPS','30 IPS'],1);C('f14atape','A800 TAPE',['250','456','900','GP9'],1);B('f13on','FREEZE',0);C('f13pmode','DEEP FREEZE MODE',['LATCH','AUTO'],1);C('f13xmode','PLUS MODE',['SINGLE','LAYER'],1);B('f12thold','TENSOR HOLD',0);B('f11on','STUTTER ENGAGE',0);B('f11rtrip','RYSUP TRIPLET',0);C('f11rmode','RYSUP TRIGGER',['MANUAL','RANDOM'],1);C('f11smode','STUTTER EDIT MODE',['ONE SHOT','LOOP','EVERY BAR'],2);C('f10range','RING MOD RANGE',['LO','HI'],1);C('f10wave','RING MOD WAVE',['SINE','SQUARE'],0);C('f10xmode','RADIUS MODE',['RING','SHIFT UP','SHIFT DOWN'],0);C('f10lshape','MF-102 LFO SHAPE',['SINE','SQUARE'],0);B('f9jfilt','3 SERIES FILTER',1);C('f8rmode','RIFT MODE',['COMB','RESONATOR'],0);B('f8thold','TSA 2 FEEDBACK HOLD',0);B('f8tkill','TSA 2 CUT INPUT',0);B('f7freeze','GRANULAR FREEZE',0);C('f7pat','S-4 PATTERN',['FORWARD','REVERSE','RANDOM'],0);C('f2ushape','CLUSTER FLUX SHAPE',['SINE','SQUARE'],0);C('f2isrc','INSTANT FLANGER SWEEP',['LFO','ENVELOPE','MANUAL'],0);B('f2itzf','INSTANT THROUGH ZERO',1);C('f2hpol','HOAX POLARITY',['+','\u2212'],0);C('f2smode','STARLIGHT MODE',['LFO','ENVELOPE','LFO + ENV'],0);C('f2amode','A/DA MODE',['SWEEP','MANUAL','TRIGGER'],0);C('f6model','REVERB MODEL',RVM,0);C('f5ltype','TIMELINE TYPE',['DIGITAL','DUAL','PING PONG','REVERSE'],0);
C('f5eheads','ECHOREC HEADS',['1','2','3','4','1+2','2+3','3+4','1+3','2+4','ALL'],9);C('f5emode','ECHOREC MODE',['ECHO','REPEAT','SWELL'],1);C('f5rmode','RE-201 HEADS',['1','2','3','2+3','1+3','1+2','1+2+3'],3);for(let k=1;k<=5;k++)B('f5ch'+k,'COPICAT HEAD '+k,k%2===1);C('f3dmode','DIMENSION D MODE',['1','2','3','4'],1);C('f3jshape','JULIANNA SHAPE',['SINE','TRIANGLE','RANDOM'],0);C('f1bshape1','BI-PHASE OSC 1 SHAPE',['SINE','SQUARE'],0);C('f1bshape2','BI-PHASE OSC 2 SHAPE',['SINE','SQUARE'],0);C('f1bsrcb','BI-PHASE B SWEEP',['OSC 1','OSC 2'],1);C('f1broute','BI-PHASE ROUTING',['SERIES','PARALLEL'],1);C('f0vtype','VULTURE TYPE',['TRIODE','PENTODE 1','PENTODE 2'],0);B('f0vx10','VULTURE x10',0);B('f0halt','HG-2 ALT SAT',0);C('f0bmode','BOUM MODE',['WARM','HOT','FUZZ'],0);
for(let f=0;f<16;f++){FXK[f].forEach(([k,nm,fm,md])=>{const id='f'+f+k;F(id,knobOwner(f,md)+' '+nm,fm==='bip'||fm==='filt'?-1:0,1,FXDF[id]!=null?FXDF[id]:.5,fm)});F('f'+f+'mix',FXN[f]+' MIX',0,1,FXMIX0[f]||0)}
B('f5sync','DELAY TEMPO SYNC',1);
// effects added later: at the end of the slot, so the first 16 effects' controls keep their places (saved sounds, CC maps)
C('f16src','SIDECHAIN SOURCE',['PART 1','PART 2','PART 3','PART 4','LFO','SEQUENCE'],0);C('f16key','SIDECHAIN KEY',['SOUND','NOTES','TRACK 1','TRACK 2','TRACK 3','TRACK 4','TRACK 5','TRACK 6','TRACK 7','TRACK 8'],2);
C('f16rate','SIDECHAIN RATE',['1/1','1/2','1/4','1/8','1/16'],4);for(let k=0;k<16;k++)B('f16st'+k,'SIDECHAIN STEP '+(k+1),k%4===0);
for(let f=16;f<NFX;f++){FXK[f].forEach(([k,nm,fm,md])=>{const id='f'+f+k;F(id,knobOwner(f,md)+' '+nm,fm==='bip'||fm==='filt'?-1:0,1,FXDF[id]!=null?FXDF[id]:.5,fm)});F('f'+f+'mix',FXN[f]+' MIX',0,1,FXMIX0[f]||0)}}
for(let q=0;q<NS;q++){__pf='q'+q;__pn='S'+(q+1)+' ';defFx()}__pf='';__pn='';
// chain: 8 slots, any effect in any slot, the same effect as many times as you like, like the prototype's FX ORDER page
const SLOTN=['OFF',...FXN];const SLOT0=[1,4,6,7,0,0,0,0];
for(let q=0;q<NS;q++)C('slot'+q,'SLOT '+(q+1),SLOTN,SLOT0[q]);
// input, output
C('insrc','INPUT',['DEMO LOOP','KEYBOARD SYNTH','AUDIO IN'],0);F('ingain','INPUT GAIN',0,1,.5,'gain');F('drywet','DRY / WET',0,1,1);
for(let i=0;i<4;i++){F('l'+i+'rate','LFO '+(i+1),0,1,.35+.1*i,'hz');C('l'+i+'shape','SHAPE',['SINE','TRI','SAW','SQUARE','RANDOM'],i);}
F('shrate','S&H RATE',0,1,.55,'hz');F('shsm','SMOOTH',0,1,0);F('sh2rate','S&H 2 RATE',0,1,.4,'hz');F('sh2sm','SMOOTH',0,1,.3);
const SRC=['---','LFO 1','LFO 2','LFO 3','S&H','SEQ 1','SEQ 2','SEQ 3','SEQ 4','SEQ 5','SEQ 6','ENV FOLLOWER','LFO 4','S&H 2'];
const DST=[['---',-1]];
for(let q=0;q<NS;q++)for(let f=0;f<16;f++){const S='S'+(q+1)+' ',pf='q'+q+'f'+f;FXK[f].forEach(([k,nm,fm,md])=>DST.push([S+knobOwner(f,md)+' '+nm,pf+k]));DST.push([S+FXN[f]+' MIX',pf+'mix'])}
DST.push(['INPUT GAIN','ingain'],['DRY / WET','drywet'],['LFO 1 RATE','l0rate'],['LFO 2 RATE','l1rate'],['LFO 3 RATE','l2rate'],['LFO 4 RATE','l3rate'],['S&H RATE','shrate'],['S&H 2 RATE','sh2rate'],['VOLUME','master']);
// effects added later go after, so the saved sounds' modulation destinations keep their numbers
for(let q=0;q<NS;q++)for(let f=16;f<NFX;f++){const S='S'+(q+1)+' ',pf='q'+q+'f'+f;FXK[f].forEach(([k,nm,fm,md])=>DST.push([S+knobOwner(f,md)+' '+nm,pf+k]));DST.push([S+FXN[f]+' MIX',pf+'mix'])}
for(let s=0;s<6;s++){for(let k=0;k<16;k++)F('s'+s+'_'+k,'',-1,1,0,'bip');C('s'+s+'dst','DESTINATION',DST.map(d=>d[0]),0);F('s'+s+'amt','AMOUNT',-1,1,.5,'bip');}
for(let m=0;m<8;m++){C('m'+m+'src','SOURCE',SRC,0);C('m'+m+'dst','DEST',DST.map(d=>d[0]),0);F('m'+m+'amt','AMOUNT',-1,1,0,'bip');}
F('bpm','TEMPO',40,200,112,'bpm');F('master','VOLUME',0,1,.7);
const DSTP=DST.map(d=>d[1]===-1?-1:IDX[d[1]]);
const P=new Float32Array(D.length);D.forEach((d,i)=>P[i]=d.df);
// value curves, shared with the DSP
const lfoHz=k=>.01*Math.pow(3000,k);
const NOTEV=[['1/32',.125],['1/16T',1/6],['1/16',.25],['1/8T',1/3],['1/16.',.375],['1/8',.5],['1/4T',2/3],['1/8.',.75],['1/4',1],['1/2T',4/3],['1/4.',1.5],['1/2',2],['1/1',4]];
function fmt(i,v){const d=D[i];if(d.t==='c')return d.ch[Math.round(v)];if(d.t==='b')return v>.5?'ON':'OFF';
 switch(d.fmt){case'pct':return Math.round(v*100)+'%';case'bip':return(v>=0?'+':'')+Math.round(v*100);case'hz':return lfoHz(v).toFixed(2)+' Hz';case'bpm':return Math.round(v)+' BPM';
 case'deg':return Math.round(v*180)+'°';case'width':return Math.round(v*200)+'%';case'pst':{const s=Math.round((v-.5)*24);return(s>=0?'+':'')+s+' st'}case'rmfreq':{const m=Math.round(P[IDX.f10model]),lo=(m===0||m===2)&&Math.round(P[IDX.f10range])===0;const f=lo?.5*Math.pow(160,v):(m===0||m===2)?30*Math.pow(133,v):15*Math.pow(200,v);return f<10?f.toFixed(2)+' Hz':Math.round(f)+' Hz'}
 case'speed':{const r=(v-.5)*4;return Math.abs(r)<.13?(r<0?'\u2212':'+')+'0.13\u00d7':(r>0?'+':'\u2212')+Math.abs(r).toFixed(2)+'\u00d7'+(r<0?' REVERSE':'')}case'shrt':return(1*Math.pow(40,v)).toFixed(1)+' s';case'ms500':return Math.round(v*500)+' ms';case'declag':{const ms=v<.3?-4+v/.3*14:10*Math.pow(50,(v-.3)/.7);return Math.abs(ms)<.3?'0 ms THRU ZERO':ms<0?'\u2212'+(-ms).toFixed(1)+' ms AHEAD':ms<100?ms.toFixed(1)+' ms':Math.round(ms)+' ms'}case'blend':return v<.03?'REFERENCE':v>.97?'LAG':Math.round((1-v)*100)+' / '+Math.round(v*100);case'wsize':return(3*Math.pow(40,v)).toFixed(1)+' ms';case'ftime':{const t=.005*Math.pow(600,v);return t<1?Math.round(t*1000)+' ms':t.toFixed(2)+' s'}case'sustime':return v>.97?'INFINITE':(1+29*v*v).toFixed(1)+' s';
 case'stdiv':return ['1/2','1/4','1/8','1/16','1/32','1/64'][Math.round(v*5)];case'sedur':return ['1/2 BEAT','1 BEAT','2 BEATS','1 BAR','2 BARS','4 BARS'][Math.round(v*5)];case'stpitch':{const s=(v-.5)*4;return(s>=0?'+':'')+s.toFixed(1)+' st / repeat'}case'curve':return v<.4?'FAST START':v>.6?'SLOW START':'LINEAR';
 case'lpf':return Math.round(200*Math.pow(90,v))+' Hz';case'loopms':return(1+v*59).toFixed(1)+' ms';case'gpos':return(v*1.6).toFixed(2)+' s back';case'gnote':return ['1/32','1/16','1/8','1/4','1/2'][Math.round(v*4)];case'wshape':return v<.2?'FLAT':v<.6?'SMOOTH':'PERCUSSIVE';case'ms112':return(v*112.5).toFixed(1)+' ms';case'ms200':return Math.round(v*200)+' ms';case'fltime':return(.5+v*11.5).toFixed(1)+' ms';case'rt1':return(.3*Math.pow(40,v)).toFixed(2)+' s';case'rt2':return(.5*Math.pow(12,v)).toFixed(2)+' s';case'rt3':return(.8*Math.pow(6,v)).toFixed(2)+' s';case'rt4':return(.2*Math.pow(12,v)).toFixed(2)+' s';case'ms250':return Math.round(v*250)+' ms';case'lowm':return '×'+(.5*Math.pow(4,v)).toFixed(2);
 case'cspeed':return Math.round(110-v*60)+' ms per head';case'espeed':return Math.round(60+v*90)+'% speed';case'eptime':return Math.round(60+v*690)+' ms';case'rerate':return Math.round(180-v*120)+' ms head 1';
 case'crate':return(.1*Math.pow(90,v)).toFixed(2)+' Hz';case'trate':return(.05*Math.pow(120,v)).toFixed(2)+' Hz';case'jrate':return(.05*Math.pow(200,v)).toFixed(2)+' Hz';case'lag':return(1.5+v*13.5).toFixed(1)+' ms';case'dpv':return v<.03?'DRY':v>.97?'VIBRATO':Math.abs(v-.5)<.03?'CHORUS':v<.5?'DRY > CHORUS':'CHORUS > VIB';
 case'brate':return(.03*Math.pow(400,v)).toFixed(2)+' Hz';case'mrate':return(.08*Math.pow(300,v)).toFixed(2)+' Hz';case'xspeed':return(.06*Math.pow(140,v)).toFixed(2)+' Hz';case'irate':return(.02*Math.pow(500,v)).toFixed(2)+' Hz';
 case'rate':return(.03*Math.pow(300,v)).toFixed(2)+' Hz';case'cents':return '±'+(v*50).toFixed(1)+' ct';
 case'dtime':return P[IDX.f5sync]>.5?NOTEV[Math.round(v*12)][0]:Math.round(20*Math.pow(75,v))+' ms';
 case'gsize':return Math.round(15*Math.pow(20,v))+' ms';case'gdens':return(1+v*39).toFixed(0)+' /s';case'gpitch':{const s=Math.round((v-.5)*24);return(s>=0?'+':'')+s+' st'}
 case'fbhz':return Math.round(30*Math.pow(70,v))+' Hz';case'bits':return(16-v*14).toFixed(1)+' bit';case'srdiv':return '÷'+Math.round(1+v*v*49);
 case'rmhz':return Math.round(15*Math.pow(200,v))+' Hz';case'shape':return v<.05?'SINE':v>.95?'SQUARE':'SINE>SQR';
 case'stlen':return ['1/64','1/32','1/16','1/8','1/4'][Math.round(v*4)];case'rtime':return Math.round(80*Math.pow(25,v))+' ms';case'fzsize':return Math.round(40*Math.pow(12.5,v))+' ms';
 case'scatt':return Math.round(v*300)+' ms';case'scdec':return Math.round(10*Math.pow(200,v))+' ms';case'outdb':{const db=(v-.5)*24;return(db>=0?'+':'')+db.toFixed(1)+' dB'}case'filt':return Math.abs(v)<.03?'OPEN':v<0?'LOW PASS '+Math.round(-v*100):'HIGH PASS '+Math.round(v*100);
 case'shshift':{const s=Math.round(v*12);return '+'+s+' st'}case'gain':{const db=(v-.5)*36;return(db>=0?'+':'')+db.toFixed(1)+' dB'}
 default:return v.toFixed(2)}}

