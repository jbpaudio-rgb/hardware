
let lastStep=-1;
function meters(m){envM.firstChild.style.width=(m.env*100)+'%';slotEls.forEach((S,q)=>{const v=S.cur;S.led.classList.toggle('on',v>0&&m.mix[q]>.01)});
  if(m.step!==lastStep){stepEls.forEach(a=>a.forEach((e,k)=>e.classList.toggle('now',k===m.step)));lastStep=m.step}}
rebuildPages();refreshSlots();refreshAll();

window.__fxUseInput=useInput;window.__fxReorder=reorder;window.__ccOrder=__ccOrder;window.__ccHint=__ccHint;if(window.__midiReady)midiBuild();
