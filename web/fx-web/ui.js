// ===================== Pages: the chain (order of the effects) and the bank of 16 effects =====================
document.getElementById('masterSlot').appendChild(knob('master',false,'VOLUME'));
const inApp=typeof __pg==='function';
const pageOf=n=>inApp?__pg(n):document.getElementById('voice');
const slotIds=[0,1,2,3,4,5,6,7].map(q=>IDX['slot'+q]);
const slotFx=q=>Math.round(P[slotIds[q]])-1;
// the controls of each slot (same order in every slot), and the mod destinations by id, to carry them along when boxes move
const SLOTP=[0,1,2,3,4,5,6,7].map(q=>D.map((d,i)=>d.id.startsWith('q'+q+'f')?i:-1).filter(i=>i>=0));const DSTI={};DST.forEach((d,j)=>DSTI[d[1]]=j);
function setSlot(q,v){setP(slotIds[q],v)}
// a box's name: the effect, numbered when it is in the chain more than once
function slotLabel(q){const f=slotFx(q);if(f<0)return '';const all=[0,1,2,3,4,5,6,7].filter(k=>slotFx(k)===f);return all.length>1?FXN[f]+' '+(all.indexOf(q)+1):FXN[f]}
// ---- page 1: CHAIN — small boxes in a row: drag one to change the order, tap it to choose its effect, the knob under it is its mix
const CP=pageOf('CHAIN');let s=section(CP,'FX CHAIN','var(--hdr)',true);
const board=el('div','board');s.appendChild(board);
const ioIn=el('div','pedal io','<b>IN</b>');const ioOut=el('div','pedal io','<b>OUT</b>');
const lane=el('div','pedals');board.append(ioIn,lane,ioOut);
const slotEls=[];
for(let q=0;q<NS;q++){const col=el('div','pcol');const p=el('div','pedal');p.tabIndex=0;p.setAttribute('role','button');
  const led=el('i','sled');const nm=el('b');const num=el('span','pnum',String(q+1));p.append(led,nm,num);const mixHolder=el('div','pmix');col.append(p,mixHolder);lane.appendChild(col);
  slotEls.push({p,nm,led,mixHolder,col,cur:-1});
  // drag to reorder, tap to pick
  let x0=0,y0=0,drag=false,pid=null;
  p.addEventListener('pointerdown',e=>{x0=e.clientX;y0=e.clientY;drag=false;pid=e.pointerId;p.setPointerCapture(pid);e.preventDefault()});
  p.addEventListener('pointermove',e=>{if(pid!==e.pointerId||!p.hasPointerCapture(pid))return;const dx=e.clientX-x0;
    if(!drag&&Math.abs(dx)+Math.abs(e.clientY-y0)>8){drag=true;p.classList.add('drag')}
    if(drag){const z=parseFloat(getComputedStyle(engBody.closest('.fit')).zoom)||1;p.style.transform=`translate(${dx/z}px,${(e.clientY-y0)/z}px)`;const t=targetAt(e.clientX,e.clientY);slotEls.forEach((S,k)=>S.col.classList.toggle('drop',k===t&&k!==q))}});
  const end=e=>{if(pid!==e.pointerId)return;pid=null;p.classList.remove('drag');p.style.transform='';slotEls.forEach(S=>S.col.classList.remove('drop'));
    if(drag){const t=targetAt(e.clientX,e.clientY);if(t!==q)reorder(q,t)}else openPicker(q)};
  p.addEventListener('pointerup',end);p.addEventListener('pointercancel',e=>{pid=null;p.classList.remove('drag');p.style.transform='';slotEls.forEach(S=>S.col.classList.remove('drop'))});
  p.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();openPicker(q)}if(e.key==='ArrowLeft'&&q>0){reorder(q,q-1);slotEls[q-1].p.focus()}if(e.key==='ArrowRight'&&q<NS-1){reorder(q,q+1);slotEls[q+1].p.focus()}})}
function targetAt(x,y){let best=0,bd=1e9;slotEls.forEach((S,k)=>{const r=S.p.getBoundingClientRect();const d=Math.hypot(x-(r.left+r.width/2),(y-(r.top+r.height/2))*1.5);if(d<bd){bd=d;best=k}});return best}
// moving a box moves its settings, its sound (delay lines, reverb tails...) and the modulation aimed at it
function reorder(from,to){const arr=[0,1,2,3,4,5,6,7];const v=arr.splice(from,1)[0];arr.splice(to,0,v);
  const sv=slotIds.map(i=>P[i]),pv=SLOTP.map(L=>L.map(i=>P[i]));arr.forEach((o,k)=>{P[slotIds[k]]=sv[o];SLOTP[k].forEach((i,j)=>P[i]=pv[o][j])});
  const inv=[];arr.forEach((o,k)=>inv[o]=k);
  ['m0dst','m1dst','m2dst','m3dst','m4dst','m5dst','m6dst','m7dst','s0dst','s1dst','s2dst','s3dst','s4dst','s5dst'].forEach(id=>{const i=IDX[id],j=Math.round(P[i]);const m=/^q(\d)(.*)$/.exec(DST[j][1]);if(m)P[i]=DSTI['q'+inv[+m[1]]+m[2]]});
  if(typeof node!=='undefined'&&node)node.port.postMessage({t:'perm',map:arr,p:P});
  for(let q=0;q<NS;q++)built[q]=-2;views.forEach(w=>w.update());send();rebuildPages()}
// effect picker
const picker=el('div','picker');picker.hidden=true;s.appendChild(picker);let pickQ=-1;
function openPicker(q){pickQ=q;picker.innerHTML='';picker.appendChild(el('div','ptit','SLOT '+(q+1)));const g=el('div','pgrid');picker.appendChild(g);
  const cur=Math.round(P[slotIds[q]]);
  FXN.forEach((n,f)=>{const k=[0,1,2,3,4,5,6,7].filter(x=>x!==q&&slotFx(x)===f).length;const b=el('button',cur===f+1?'on':'',n+(k?' \u00b7 '+k+' IN RACK':''));b.onclick=()=>{setSlot(q,f+1);closePicker()};g.appendChild(b)});
  const rm=el('button','rm','EMPTY');rm.onclick=()=>{setSlot(q,0);closePicker()};const cx=el('button','rm','CLOSE');cx.onclick=closePicker;const r=el('div','pacts');r.append(rm,cx);picker.appendChild(r);picker.hidden=false}
function closePicker(){picker.hidden=true;pickQ=-1}
// input and output settings under the chain
const ioRow=el('div','iorow');s.appendChild(ioRow);
const fileIn=el('input');fileIn.type='file';fileIn.accept='audio/*';fileIn.id='fxfile';fileIn.hidden=true;
const fileB=el('button',null,'LOAD AUDIO FILE');fileB.onclick=()=>fileIn.click();const micB=el('button',null,'MIC / LINE IN');const inMsg=el('div','cmsg');const envM=el('div','meter','<i></i>');
const ig=el('div','iog');ig.appendChild(el('div','cap','INPUT'));const igr=el('div','row');igr.append(select('insrc','SOURCE'),knob('ingain',false,'GAIN'));const igb=el('div','iobtn');igb.append(fileB,micB,inMsg,envM,fileIn);ig.append(igr,igb);
const og=el('div','iog');og.appendChild(el('div','cap','OUTPUT'));const ogr=el('div','row');ogr.append(knob('drywet',false,'DRY / WET'));og.appendChild(ogr);
ioRow.append(ig,og);
s.appendChild(el('div','hint','Drag a box to another place to change the order: the sound goes from IN through the boxes to OUT. Tap a box to choose its effect. The knob under a box is that effect’s mix. An effect can go in more than one box, each with its own settings.'));
function refreshSlots(){slotEls.forEach((S,q)=>{const v=Math.round(P[slotIds[q]]);S.p.classList.toggle('empty',v===0);S.nm.textContent=v?slotLabel(q):'+';S.p.setAttribute('aria-label','Slot '+(q+1)+': '+(v?slotLabel(q):'empty')+'. Tap to choose, drag to move');
  if(S.cur!==v||S.built!==built.gen){S.cur=v;S.built=built.gen;if(S.mv)dropViews(S.mv);S.mixHolder.innerHTML='';if(v>0){const v0=views.length;S.mixHolder.appendChild(knob('q'+q+'f'+(v-1)+'mix',false,'MIX'));S.mv=views.splice(v0);views.push(...S.mv)}}})}
slotIds.forEach(i=>views.push({i,update:refreshSlots}));
// ---- one page per effect; only the effects that are in the rack have a tab, in the order of the chain
function seg(id,name,labels){const i=IDX[id],d=D[i];const c=el('div','ctl esw');const box=el('div','segs');box.setAttribute('role','radiogroup');box.setAttribute('aria-label',name||d.nm);
  const bs=d.ch.map((t,j)=>{const b=el('button',null,(labels||d.ch)[j]);b.setAttribute('role','radio');b.onclick=()=>setP(i,j);box.appendChild(b);return b});
  const update=()=>bs.forEach((b,j)=>{const on=j===Math.round(P[i]);b.classList.toggle('on',on);b.setAttribute('aria-checked',on)});c.append(box,el('div','nm',name||d.nm));views.push({i,update});update();c.dataset.p=id;return c}
const __knob=knob,__toggle=toggle,__seg=seg,__select=select;
function dropViews(list){const s=new Set(list);for(let i=views.length-1;i>=0;i--)if(s.has(views[i]))views.splice(i,1)}
const slotPg=[0,1,2,3,4,5,6,7].map(q=>{const pg=pageOf('SLOT '+(q+1));pg.classList.add('fxsolo');return pg});
const built=[-2,-2,-2,-2,-2,-2,-2,-2];built.gen=0;const slotViews=[[],[],[],[],[],[],[],[]];
// the controls each model shows, per slot (for the MIDI map): FXOWN[q][model] = ids
const FXOWN=[[],[],[],[],[],[],[],[]];
function rebuildPages(){let ch=false;for(let q=0;q<NS;q++){const f=slotFx(q);if(built[q]!==f){buildSlot(q);ch=true}}if(ch){built.gen++;refreshSlots();refreshAll()}}
let __rbq=false;const queueRebuild=()=>{if(__rbq)return;__rbq=true;queueMicrotask(()=>{__rbq=false;rebuildPages()})};
function buildSlot(q){const f=slotFx(q),pg=slotPg[q],pf='q'+q;built[q]=f;dropViews(slotViews[q]);pg.innerHTML='';FXOWN[q]=[];if(f<0)return;
  const knob=(id,...a)=>__knob(pf+id,...a),toggle=(id,...a)=>__toggle(pf+id,...a),seg=(id,...a)=>__seg(pf+id,...a);const v0=views.length;
  const inner=section(pg,slotLabel(q),'var(--hdr)',true);const sec=inner.parentNode;sec.classList.add('fxsec');
  const badge=el('button','fxbadge','SLOT '+(q+1)+' \u00b7 REMOVE FROM RACK');badge.onclick=()=>setSlot(q,0);sec.firstChild.appendChild(badge);
  if(MODELS[f]){const MN=MODELS[f],mid=pf+'f'+f+'model';const msel=seg('f'+f+'model','MODEL');const groups=[];
    if(MGROUPS[f]){const tabs=el('div','segs subt');const gb=MGROUPS[f].map(([gn,list])=>{const b=el('button',null,gn);b.onclick=()=>{if(!list.includes(Math.round(P[IDX[mid]])))setP(IDX[mid],list[0])};tabs.appendChild(b);return b});
      const mb=[...msel.querySelectorAll('button')];views.push({i:IDX[mid],update:()=>{const cur=Math.round(P[IDX[mid]]);const gi=MGROUPS[f].findIndex(g=>g[1].includes(cur));gb.forEach((b,k)=>b.classList.toggle('on',k===gi));mb.forEach((b,k)=>b.hidden=!MGROUPS[f][gi][1].includes(k))}});
      views[views.length-1].update();grp(inner,'TYPE',[tabs,msel])}
    else grp(inner,'MODEL',[msel]);
    const extra=f===0?[[seg('f0vtype','TYPE',['TRIODE','PENTODE 1','PENTODE 2']),toggle('f0vx10','x10 OVERDRIVE')],[toggle('f0halt','ALT SAT')],[seg('f0bmode','MODE')]]
      :f===6?RVM.map(()=>[])
      :f===15?[[seg('f15bpitch','PITCH')],[seg('f15amode','MODE'),toggle('f15aramp','RAMP')],[seg('f15kworldt','WORLD'),seg('f15kroute','ROUTING')]]
      :f===14?[[toggle('f14dson','SATURATION'),toggle('f14ddbl','DOUBLETRACK'),seg('f14dtype','TYPE'),toggle('f14dwide','WIDE')],[seg('f14ctype','TYPE')],[seg('f14aspeed','SPEED'),seg('f14atape','TAPE')]]
      :f===13?[[toggle('f13on','WISH')],[toggle('f13on','FREEZE'),seg('f13pmode','MODE')],[toggle('f13on','PRESS'),seg('f13xmode','MODE')]]
      :f===12?[[toggle('f12thold','HOLD')],[],[]]
      :f===11?[[toggle('f11on','ENGAGE'),seg('f11rmode','TRIGGER'),toggle('f11rtrip','TRIPLET')],[toggle('f11on','ENGAGE'),seg('f11smode','MODE')]]
      :f===10?[[seg('f10range','RANGE'),seg('f10wave','WAVE')],[seg('f10xmode','MODE')],[seg('f10range','RANGE'),seg('f10lshape','LFO SHAPE')],[seg('f10wave','WAVE')]]
      :f===9?[[],[],[toggle('f9jfilt','FILTER')],[]]
      :f===8?[[seg('f8rmode','MODE')],[toggle('f8thold','HOLD'),toggle('f8tkill','CUT INPUT')]]
      :f===7?[[toggle('f7freeze','FREEZE')],[toggle('f7freeze','FREEZE')],[toggle('f7freeze','FREEZE')],[seg('f7pat','PATTERN'),toggle('f7freeze','FREEZE')]]
      :f===4?[[],[]]
      :f===2?[[seg('f2ushape','SHAPE')],[seg('f2isrc','SWEEP'),toggle('f2itzf','THROUGH ZERO')],[seg('f2hpol','POLARITY')],[seg('f2smode','MODE')],[seg('f2amode','MODE')]]
      :f===5?[[toggle('f5sync','SYNC')],[seg('f5ltype','TYPE'),toggle('f5sync','SYNC')],[toggle('f5sync','SYNC')],[toggle('f5sync','SYNC')],[seg('f5eheads','HEADS'),seg('f5emode','MODE')],[],[seg('f5rmode','HEADS')],[1,2,3,4,5].map(k=>toggle('f5ch'+k,'HEAD '+k))]
      :f===3?[[],[seg('f3dmode','MODE')],[],[seg('f3jshape','SHAPE')]]
      :[[seg('f1mstages','STAGES')],[seg('f1xint','INTENSITY')],[seg('f1isrc','SWEEP')],[seg('f1bshape1','OSC 1'),seg('f1bshape2','OSC 2'),seg('f1bsrcb','B SWEPT BY'),seg('f1broute','ROUTING')]];
    const hint=f===15?['A big shimmer hall: the tail is pitch shifted and fed back into the reverb, so it climbs. AMOUNT sets how much of it climbs, PITCH picks the voices (octave up, fifth, both, octave down, up and down), DECAY the length, PRE-DELAY the gap before it, TONE darker or brighter, MOD the chorus in the tail.',
      'An octave reverb: LENGTH is the decay, SHIMMER how much of the shifted sound feeds back, FILTER darkens (left) or thins (right) the tail, RATE and DEPTH its warble. MODE picks the octaves (ASTRAL stacks up, down and a fifth). RAMP slides the octaves back to unison while it is on, and back out when you switch it off.',
      'Two reverbs. DARK is the shimmer: MODIFY sets how much octave-up climbs in the tail, DWELL drives the input, DECAY, TONE and PRE-DELAY shape it. WORLD is a second reverb (HALL, PLATE or SPRING) and the WORLD knob its share. ROUTING: DARK > WORLD runs the shimmer into the second reverb, PARALLEL runs them side by side, WORLD > DARK feeds the second reverb into the shimmer.'].map(h=>h+' MIX sets how much reverb you hear.')
     :f===14?['Two tape machines. SATURATION drives the first (VOLUME sets the output, TONE tilts darker or brighter). DOUBLETRACK adds a second deck that runs late by LAG TIME: at the far left it runs ahead and crosses through zero (tape flanging), around 10 to 50 ms it doubles, further right it slaps back. WOBBLE wavers the second deck, BLEND goes from the first deck only to the second only. SUM adds them, INVERT flips the second (deep cancelling flange), BOUNCE feeds it back for repeats. WIDE puts one deck on each side.',
      'A cassette deck: DRIVE pushes the tape, WOW is the slow pitch drift, FLUTTER the fast shake, HISS the tape noise, TONE tilts darker or brighter, WEAR dulls the highs and makes the level dip. NORMAL tape is darker and saturates sooner than CHROME.',
      'A studio multitrack: INPUT drives the tape (noon is clean, right is hot), OUTPUT sets the level. SPEED moves the low bump and the top end (7.5 IPS fat and dark, 30 IPS tight and open), TAPE sets how soon it saturates (250 first, GP9 last). BIAS: left is brighter and grittier, right darker and cleaner. HISS adds tape noise.'].map(h=>h+' MIX sets how much of the tape you hear.')
     :f===13?['Hold WISH to catch a tiny loop of the sound and hold it as a buzzing tone: SIZE sets the loop (short is buzzy, long is a texture), PITCH transposes it, GLIDE slides between pitches, TONE darkens (left) or thins (right) it.',
      'A smooth hold. LATCH: FREEZE catches the sound and holds it until you switch it off. AUTO: it catches a new sound at every new note you play. FADE IN and FADE OUT shape how the held sound comes and goes, LEVEL sets its volume.',
      'A sustain pedal: PRESS holds the sound while it is on. In LAYER every new press stacks on top of the sound already held (up to three), in SINGLE it replaces it. RISE fades the held sound in, SUSTAIN lets it fade while you hold (all the way right: forever), TAIL is the release.'].map(h=>h+' Your playing still passes through; MIX sets how much of it all you hear.')
     :f===12?['A playhead that runs through the last TIME of sound at any SPEED: negative is backward (reverse), positive forward, faster or slower changes the pitch. HOLD freezes the window, RANDOM picks a new speed and direction on each pass.',
      'A reverse delay: each TIME window comes back backward, REPEATS feed it again, MOD adds a slow chorus and TONE darkens the repeats.',
      'The reverse mode of a classic delay modeler: TIME, REPEATS, TWEAK (modulation depth) and TWEEZ (modulation speed).']
     :f===11?['A performance stutter: ENGAGE repeats the last slice at RATE (TRIPLET for triplets), starting on the next 16th note. PITCH bends each repeat up or down, FADE lowers each repeat, GATE shortens the slices. In RANDOM it also kicks in by itself, CHANCE sets how often.',
      'Stutter gestures: the repeat rate glides from START RATE to END RATE over the DURATION, shaped by CURVE, with GATE on the slices. ONE SHOT plays one gesture each time you ENGAGE, LOOP keeps playing them while engaged, EVERY BAR fires one at the start of each bar (always when engaged, half the time otherwise).']
     :f===10?['A ring modulator with an LFO that sweeps the carrier: LFO RATE and LFO DEPTH. RANGE LO gives tremolo-like throbs, HI the bell and robot tones. WAVE picks a sine or square carrier.',
      'Ring modulation or a true frequency shifter: SHIFT UP and SHIFT DOWN move every partial by the same number of Hz, for barber-pole and detuned metallic sounds. FINE trims the frequency, SPREAD sets the right side apart from the left.',
      'The classic two-range ring modulator: LO (0.5 to 80 Hz) or HI (30 Hz to 4 kHz), an LFO with RATE, AMOUNT and SHAPE that moves the carrier, and DRIVE on the input.',
      'A simple ring modulator: FREQUENCY, VOLUME and a sine or square carrier.']
     :f===9?['Bit and sample-rate reduction, then a FILTER (left low pass, right high pass) and a GATE that closes when you stop playing.',
      'Crush with a resonant low-pass FILTER, a STUTTER chance on each 16th note, and a SEQUENCE that steps the sample rate up and down on the tempo grid (a new pattern every bar).',
      'Two knobs, BITS and SAMPLE RATE, a VOLUME and a FILTER switch that tames the top.',
      'A badly behaved converter: JITTER wobbles the sample clock, NOISE adds a hiss floor, DROPOUT mutes the sound for short moments, DRIVE pushes the input.'].map(h=>h+' BITS goes from 16 down to 2, SAMPLE RATE divides the rate by up to 50.')
     :f===8?['A tuned feedback: in COMB the sound runs through a short delay tuned by TUNE, in RESONATOR through a ringing band pass. FEEDBACK past 3 o clock rings on by itself, DRIVE saturates the loop, TONE darkens (left) or thins (right) it, SPREAD detunes left and right.',
      'A feedback loop box: the sound circles through a short LOOP, a TONE band and a saturating gain. Raise FEEDBACK until it starts to howl, HOLD pushes it to full feedback, CUT INPUT keeps only the loop singing. VOLUME sets its level.']
     :f===7?['Grains from the last second and a half of sound, then a DRIVE and a FILTER (left low pass, right high pass) to mangle them. POSITION picks how far back the grains start, SPRAY scatters them.',
      'Grains with a shaped WINDOW (flat, smooth or percussive), a REVERSE chance for each grain and FEEDBACK of the grains back into the buffer.',
      'Grains read by a moving playhead: SCAN moves it forward or backward through the buffer, SPREAD places the grains across the stereo field, WINDOW shapes them, FILTER colours them.',
      'Grains placed on the tempo grid (RATE in note values), shaped by CONTOUR. WARP throws each grain to a random pitch, PATTERN plays them forward, reversed or at random.'].map(h=>h+' FREEZE stops recording and keeps playing the frozen sound.')
     :f===4?['An early harmonizer: hard splices that glitch now and then, a 12-bit sound and up to 112 ms of delay. DETUNE pitches the left side up and the right side down for the classic wide doubling, PITCH shifts both by semitones, FEEDBACK sends the shifted sound back for rising or falling repeats.',
      'The de-glitched successor: smooth splices, up to 200 ms of delay and RANDOM, which wanders the delay time on each side for a looser double. DETUNE, PITCH and FEEDBACK work as on the H910.'].map(h=>h+' WIDTH spreads the result.')
     :f===2?['A bucket-brigade flanger that becomes a chorus when TIME is long: AMOUNT sets the sweep, FEEDBACK can be positive or negative, DRIVE pushes the input, SHAPE sine or square.',
      'A studio flanger after tape flanging: the sweep follows the LFO, the input level (ENVELOPE) or MANUAL. THROUGH ZERO delays the dry sound too, so the notches cross zero like two tape machines.',
      'A dual-path pedal flanger: MANUAL sets the centre, DEPTH the triangle sweep, RESONANCE the feedback, BLEND the mix of the two paths, POLARITY flips the wet path for hollow sounds.',
      'A flanger whose sweep comes from the LFO, from your playing (ENVELOPE, with SENSITIVITY) or both. REGEN can be positive or negative.',
      'A wide-range flanger (about 35 to 1): SPEED and RANGE set the sweep, ENHANCE the feedback. MANUAL holds a position, TRIGGER starts one sweep each time the input crosses THRESHOLD.'].map(h=>h+' STEREO offsets the right sweep, WIDTH spreads the result.')
     :f===6?['A lush digital hall with a slowly moving, random tail.','A dark, smooth early digital plate-and-hall sound, band-limited, with a little grain.','A bright, dense digital reverb.','A very smooth, natural, wide modern digital reverb.',
      'A studio spring unit: dense and smooth, little drip.','A studio spring box: three springs, clean and rich.','The tank of a guitar amp: two long springs, bright and drippy, DWELL drives it.','A spring with a darker tone and a slight wobble.','A pedal spring: three springs, bright and splashy.','A lo-fi, wobbly spring-style reverb.',
      'A large trapezoid echo chamber: speaker in, microphones out, long and warm.','A tiled echo chamber: bright reflections, a clear tail.','A smaller, darker echo chamber.','A small bright chamber with a short tail.',
      'A steel plate: dense, smooth, bright top, light lows.','A small gold-foil plate: brighter and shorter.','A dense, bright modern plate.','A more metallic plate.','A warm plate.',
      'A large live recording room.','A small live room with a quick slap.','A warm room with curved walls.','A large, balanced live room.'].map((h,i)=>h+((i>=10&&i<=13)||i>=19?' MIC DISTANCE moves the microphones away: fewer early reflections, more tail.':'')+(i>=14&&i<=18?' DAMPER sets the decay, like the damper plate.':'')+' WIDTH spreads the result.')
     :f===5?['A dual-tap digital delay with a 12-bit sound: DELAY A goes left, DELAY B right, FEEDBACK recirculates A through the TONE filter, MOD sweeps both taps (in opposite directions) for chorus and flange.',
      'A modern stereo delay: DIGITAL (two lines), DUAL (two times, 3:4), PING PONG (bounces left and right) or REVERSE. FILTER darkens (left) or thins (right) the repeats, GRIT adds dirt and lower resolution.',
      'A clean studio delay with a pitch shifter on each repeat: TIME L and TIME R are separate, PITCH climbs or falls with every repeat, DETUNE spreads left and right for a wide doubling.',
      'A digital delay with its input preamp: DRIVE warms the input, MOD gives the classic slow chorus on the repeats, HIGH DAMP darkens them.',
      'A magnetic drum with four playback heads at even spacing. HEADS picks which ones play, SPEED sets the drum speed, SWELL the regeneration. ECHO plays once, REPEAT feeds back the last head, SWELL feeds back all chosen heads.',
      'A tape loop with one moving playback head: TIME is the head position, SUSTAIN the repeats (past 3 o clock it runs away), PREAMP adds the famous bright drive, WOW the tape wobble.',
      'A tape echo with three heads and a spring reverb: REPEAT RATE sets the tape speed, INTENSITY the repeats, HEADS picks the heads, BASS and TREBLE shape the echoes, REVERB adds the spring.',
      'A valve tape echo with five playback heads you switch on one by one: SPEED sets the tape speed, SWELL the regeneration through all the heads that are on, VALVE DRIVE the warmth of the tube preamp, TONE and WOW the colour and the wobble.']
     :f===3?['A mono bucket-brigade chorus pedal: one triangle sweep, RATE and DEPTH, the wet and dry sound mixed half and half inside, a soft darker top.',
      'Two bucket-brigade lines swept in opposite directions at a slow fixed speed and cross-mixed: a wide, still shimmer with no pitch wobble. Modes 1 to 4 go from subtle to strong.',
      'Three bucket-brigade lines swept 120 degrees apart, sent left, centre and right: a big moving stereo field. TONE sets how dark the lines are.',
      'A stereo chorus / vibrato: LAG sets the centre delay, D-P-V goes from dry through chorus (noon) to pure vibrato, SHAPE picks the sweep, PHASE offsets the right side.']
     :f===0?['A tube distortion: BIAS moves the operating point (more even harmonics, and on PENTODE 2 the stage starves and sputters). LOW and HIGH tilt the tone after the tube.',
      'Two tube stages in series, run in parallel with the dry sound: SATURATION (pentode) adds edge, DENSITY (triode) adds body and even harmonics, AIR lifts the top, ALT SAT pushes the mids into the saturation.',
      'A drive with three characters, then a compressor, a filter (left: low pass, right: high pass) and a noise gate.']
     :['A ladder phaser with 6 or 12 stages: SWEEP sets how wide the notches travel, RESONANCE feeds the output back for a vocal peak, DRIVE warms the input.',
      'A pedal phaser: one SPEED knob and four INTENSITY settings, from a gentle 4-stage swirl (1) to a deep 10-stage sweep with strong feedback (4). The sweep is a skewed triangle.',
      'A studio phaser: the notches follow the LFO, the input level (ENVELOPE, with SENSITIVITY) or the MANUAL knob. FEEDBACK can be positive or negative.',
      'Two 6-stage phasers (A and B) and two sweep oscillators, each sine or rounded square. B can follow OSC 1 with A or run on OSC 2. SERIES puts B after A for a deep 12-stage sound; PARALLEL sends A to the left and B to the right for the classic wide stereo.'];
    if(f===3)hint.forEach((h,k)=>hint[k]=h+' WIDTH spreads the result (0% mono, 100% as built, 200% extra wide).');
    if(f===1)hint.forEach((h,k)=>hint[k]=h+' STEREO offsets the right sweep from the left (0° mono, 180° opposite), WIDTH spreads the result (100% normal, 200% extra wide).');
    for(let m=0;m<MN.length;m++){const g=el('div','odm');const items=FXK[f].filter(x=>ownsKnob(x[3],m)).map(([k,nm])=>knob('f'+f+k,false,nm));items.push(knob('f'+f+'mix',true,'MIX'));
      grp(g,MN[m],items);if(extra[m].length)grp(g,null,extra[m]);g.appendChild(el('div','hint',hint[m]));inner.appendChild(g);groups.push(g);FXOWN[q][m]=[...new Set([...g.querySelectorAll('[data-p]')].map(e=>e.dataset.p))]}
    views.push({i:IDX[mid],update:()=>groups.forEach((g,m)=>g.hidden=m!==Math.round(P[IDX[mid]]))})}
  else{const items=FXK[f].map(([k,nm])=>knob('f'+f+k,false,nm));items.push(knob('f'+f+'mix',true,'MIX'));grp(inner,null,items);
    if(f===16){grp(inner,null,[seg('f16src','SOURCE',['PART 1','PART 2','PART 3','PART 4','LFO','SEQUENCE']),seg('f16rate','RATE')]);
      const st=[];for(let k=0;k<16;k++)st.push(toggle('f16st'+k,String(k+1)));grp(inner,'STEPS',st);
      inner.appendChild(el('div','hint','Ducks the sound: on an LFO (a smooth pump on the tempo, deepest on the beat) or on the 16-step SEQUENCE (ATTACK, DECAY). The PART sources key on the other parts of the Octavio hardware synth; here they do nothing.'))}
    FXOWN[q][0]=[...inner.querySelectorAll('[data-p]')].map(e=>e.dataset.p)}
  if(f===10){const fr=IDX[pf+'f10freq'];[pf+'f10model',pf+'f10range'].forEach(id=>views.push({i:IDX[id],update:()=>views.forEach(w=>{if(w.i===fr)w.update()})}))}
  views.push({i:IDX[mid0(q,f)],update:refreshAll});
  slotViews[q]=views.splice(v0);views.push(...slotViews[q]);slotViews[q].forEach(w=>{if(w.update!==refreshAll)w.update()});if(typeof fitAll==='function')fitAll()}
const mid0=(q,f)=>'q'+q+'f'+f+'model';
function refreshBadges(){for(let q=0;q<NS;q++){const h=slotPg[q].querySelector('.fxsec h2,.fxsec .blabel');if(h&&slotFx(q)>=0)h.firstChild&&(h.firstChild.nodeType===3)&&(h.firstChild.textContent=slotLabel(q))}}
// one tab per filled slot, in chain order, named after its effect
function refreshTabs(){if(!inApp)return;for(let q=0;q<NS;q++){const b=[...subTabs.children].find(x=>x.dataset.n==='SLOT '+(q+1));if(!b)continue;const f=slotFx(q);b.hidden=f<0;b.textContent=f<0?'':slotLabel(q)}
  const on=[...subTabs.children].find(b=>b.classList.contains('on'));if(on&&on.hidden)__showSub('CHAIN');subTabs.hidden=false;if(typeof fitAll==='function')fitAll()}
// modulation destinations: only the controls of the boxes in the rack, and of the model each box uses
function dstAllowed(j){const id=DST[j][1];if(id===-1)return true;const m=/^q(\d)f(\d+)(.*)$/.exec(id);if(!m)return true;const q=+m[1],f=+m[2];if(slotFx(q)!==f)return false;
  if(MODELS[f]&&m[3]!=='mix'){const k=FXK[f].find(x=>x[0]===m[3]);if(k&&!ownsKnob(k[3],Math.round(P[IDX[mid0(q,f)]])))return false}return true}
function refreshDst(){document.querySelectorAll('select').forEach(s=>{if(!/^sel-(m\d|s\d)dst$/.test(s.id))return;const cur=+s.value;const frag=document.createDocumentFragment();
  DST.forEach((d,j)=>{if(j!==cur&&!dstAllowed(j))return;const o=el('option',null,d[0]);o.value=j;if(j===cur&&!dstAllowed(j))o.textContent=d[0]+' (not in rack)';frag.appendChild(o)});s.innerHTML='';s.appendChild(frag);s.value=cur})}
const refreshAll=()=>{refreshBadges();refreshTabs();refreshDst();if(typeof __midiReady!=='undefined'&&__midiReady){midiBuild();if(midiOpen)midiRender()}};
slotIds.forEach(i=>views.push({i,update:queueRebuild}));
['m0dst','m1dst','m2dst','m3dst','m4dst','m5dst','m6dst','m7dst','s0dst','s1dst','s2dst','s3dst','s4dst','s5dst'].forEach(id=>views.push({i:IDX[id],update:()=>setTimeout(refreshDst,0)}));
function __ccOrder(){const o=[];for(let q=0;q<NS;q++){const f=slotFx(q),blk=[];if(f>=0){const m=Math.round(P[IDX[mid0(q,f)]]);const ids=['q'+q+'f'+f+'model',...((FXOWN[q][m]||FXOWN[q][0]||[]).filter(id=>id!=='q'+q+'f'+f+'model'))];
    ids.forEach(id=>{if(IDX[id]!=null&&blk.length<12)blk.push(IDX[id])})}while(blk.length<12)blk.push(null);o.push(...blk)}
  return [...o,IDX.ingain,IDX.drywet,IDX.master]}
function __ccHint(){const rng=(a,b)=>{const L=CC_POOL.slice(a,b),out=[];let s=L[0],p=L[0];for(const c of L.slice(1).concat([-9])){if(c!==p+1){out.push(s===p?''+s:s+'\u2013'+p);s=c}p=c}return out.join(' and ')};
  return 'Only the boxes in the rack get CCs, and only the controls of the model each box uses. Each box has its own block of 12 CCs, so its CCs stay put when you change the other boxes: '+
    [0,1,2,3,4,5,6,7].map(q=>'box '+(q+1)+' CC '+rng(q*12,q*12+12)).join(', ')+'. In a block: the model first, then that model\u2019s controls and its mix. Input gain is CC '+CC_POOL[96]+', dry / wet '+CC_POOL[97]+', volume '+CC_POOL[98]+'.'}
// ---- audio input: file or microphone, fed into the rack
let inNode=null,rackNode=null;
function __onNode(n){rackNode=n;if(inNode)try{inNode.connect(n)}catch(e){}}
async function ensureCtx(){if(!ctx)startBtn.click();for(let k=0;k<40&&(!ctx||!rackNode);k++)await new Promise(r=>setTimeout(r,50));return ctx}
function useInput(n,label){try{if(inNode)inNode.disconnect()}catch(e){}if(inNode&&inNode.stop)try{inNode.stop()}catch(e){}inNode=n;if(rackNode)n.connect(rackNode);setP(IDX.insrc,2);inMsg.textContent=label}
fileIn.onchange=async()=>{const f=fileIn.files&&fileIn.files[0];if(!f)return;inMsg.textContent='Loading…';
  try{await ensureCtx();const buf=await ctx.decodeAudioData(await f.arrayBuffer());const src=ctx.createBufferSource();src.buffer=buf;src.loop=true;src.start();useInput(src,'Playing '+f.name+' (looped)')}
  catch(e){inMsg.textContent='This file could not be read. Try a WAV or MP3.'}};
micB.onclick=async()=>{try{await ensureCtx();const st=await navigator.mediaDevices.getUserMedia({audio:{echoCancellation:false,noiseSuppression:false,autoGainControl:false}});useInput(ctx.createMediaStreamSource(st),'Microphone / line in')}
  catch(e){inMsg.textContent='The microphone is blocked here. Open the installed version in a browser to use it, or load a file.'}};
const M=document.getElementById('mod');
