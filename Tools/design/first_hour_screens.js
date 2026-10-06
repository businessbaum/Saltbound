/* ===== D75: M6's new screens, drawn with the approved pages' own kit (UI_Satchel.html, UI_Prompts.html, Menus.html) =====
   Every part is a thing from the boat or the harbour: parchment pinned with brass tacks, stitched leather, wood with iron,
   wax seals, ink. 1920 x 1080 frames, as the design pages. */
const FL='"IM Fell English",Georgia,serif',FSC='"IM Fell English SC",Georgia,serif',FT='Alegreya,Georgia,serif',INK='rgba(48,30,16,.94)';
const KS=58,KSEED=12,PF=36,TURQ='rgba(90,230,210,.95)';
let GRAIN,SALT,PARCH,PARCH_BIG,PARCH_PAGE,PARCH_TAG,SPECK,ICON={},LEATHER_BIG,LEATHER_DARK,LEATHER_DISC,LEATHER_SMALL,WOOD,WOOD_DARK,SLATE;
const circlePts=(cx,cy,r,n=64)=>Array.from({length:n+1},(_,i)=>[cx+Math.cos(i/n*TAU)*r,cy+Math.sin(i/n*TAU)*r]);
const SH={shadow:['rgba(0,0,0,.9)',6,0,2]};

/* --- small parts ------------------------------------------------------------------------------------------------- */
/* the prompt's key on a scrap, with any letter */
function keyScrapL(c,kx,ky,L,s=KS,seed=KSEED){const fs=Math.round(s*.55);scrap(c,kx-s/2,ky-s/2,s,s,{seed,rot:-.05,tear:4},()=>{
  /* letters in the prompt's small capitals; figures painted (the face's old-style 1 reads as an I) */
  if(/\d/.test(L)){const f=`700 ${fs}px Alegreya,Georgia,serif`;c.save();c.filter='blur(.7px)';txt(c,L,kx,ky+fs*.36,f,'rgba(48,30,16,.35)',{align:'center'});c.restore();txt(c,L,kx,ky+fs*.36,f,INK,{align:'center'})}
  else inkText(c,L,kx,ky+fs*.34,fs,{align:'center',sc:true})})}
/* a brass upholstery tack and an iron nail head */
function tack(c,x,y,r=8){c.save();c.fillStyle='rgba(0,0,0,.35)';c.filter='blur(3px)';c.beginPath();c.ellipse(x+3,y+5,r,r*.8,0,0,TAU);c.fill();c.restore();rivet(c,x,y,r)}
function nail(c,x,y,r=6){c.save();c.fillStyle='rgba(0,0,0,.4)';c.filter='blur(2px)';c.beginPath();c.arc(x+2,y+3,r,0,TAU);c.fill();c.restore();ironRivet(c,x,y,r)}
/* ink drawn by hand: a rough double stroke along points */
function inkPath(c,pts,w=2.4,col='rgba(48,30,16,.9)',seed=1){const R=rng(seed);[[1,w,0],[.4,w*.55,.7]].forEach(([a,lw,o])=>{c.save();c.globalAlpha=a;c.strokeStyle=col;c.lineWidth=lw;c.lineCap='round';c.lineJoin='round';
  c.beginPath();pts.forEach(([x,y],i)=>{const jx=(R()-.5)*.8+o,jy=(R()-.5)*.8+o;i?c.lineTo(x+jx,y+jy):c.moveTo(x+jx,y+jy)});c.stroke();c.restore()})}
function inkCurve(c,f,n,w,col,seed){const pts=[];for(let i=0;i<=n;i++)pts.push(f(i/n));inkPath(c,pts,w,col,seed)}
/* handwriting: ink, a slight slant and a wandering baseline */
function splitLines(c,s,font,maxW){c.save();c.font=font;let line='';const lines=[];for(const w of s.split(' ')){const t=line?line+' '+w:w;if(c.measureText(t).width>maxW&&line){lines.push(line);line=w}else line=t}lines.push(line);c.restore();return lines}
function hand(c,s,x,y,size,{rot=-.012,italic=true,col='rgba(42,24,10,.92)',maxW=0,lh=0}={}){
  const f=`${italic?'italic ':''}400 ${size}px ${FL}`;c.save();c.translate(x,y);c.rotate(rot);
  if(maxW){const lines=splitLines(c,s,f,maxW);
    lines.forEach((l,i)=>{c.save();c.translate(0,i*(lh||size*1.35));c.rotate((i%2?.004:-.003));c.filter='blur(.6px)';txt(c,l,0,0,f,'rgba(48,30,16,.32)');c.filter='none';txt(c,l,0,0,f,col);c.restore()});
    c.restore();return lines.length*(lh||size*1.35)}
  c.filter='blur(.6px)';txt(c,s,0,0,f,'rgba(48,30,16,.32)');c.filter='none';txt(c,s,0,0,f,col);c.restore();return size*1.35}
/* an ink rule like a quill stroke */
function quillRule(c,x0,x1,y,seed=3,w=1.6){inkCurve(c,t=>[x0+(x1-x0)*t,y+Math.sin(t*9+seed)*1.2+(t<.05?-.6:0)],40,w,'rgba(58,38,22,.6)',seed)}
/* a bar of rock salt (the currency, D61): long, grey-white and crystalline, its corners chipped, bound with a fibre band
   (a smooth white block reads as soap, a white block with a red mark as a first-aid box) */
function saltCake(c,x,y,w=70,seed=3){const h=w*.34,R=rng(seed),x0=x-w/2,y0=y-h/2,ch=()=>w*(.03+R()*.05);
  const top=[[x0+ch(),y0],[x0+w*.4,y0-R()*h*.06],[x0+w-ch(),y0],[x0+w,y0+ch()*.8],[x0+w-R()*w*.02,y0+h-ch()*.6],[x0+w-ch(),y0+h],[x0+w*.55,y0+h+R()*h*.05],[x0+ch(),y0+h],[x0,y0+h-ch()*.7],[x0+R()*w*.02,y0+ch()*.8]];
  const p=poly(top),side=poly(top.map(([a,b])=>[a,b+h*.42]));
  dropShadow(c,side,10,'rgba(0,0,0,.65)',2,5);c.save();c.fillStyle='#b4afa4';c.fill(side);c.clip(side);c.globalAlpha=.5;c.drawImage(SALT,x0-((seed*23)%60),y0-((seed*11)%40));c.restore();
  c.save();c.clip(p);c.drawImage(SALT,x0-((seed*29)%60),y0-((seed*17)%50));
  const g=c.createLinearGradient(0,y0,0,y0+h);g.addColorStop(0,'rgba(255,255,255,.3)');g.addColorStop(1,'rgba(80,86,98,.25)');c.fillStyle=g;c.fill(p);c.restore();
  /* glints on the crystals */
  c.save();c.clip(p);
  for(let i=0;i<Math.round(w/9);i++){const gx=x0+w*R(),gy=y0+h*R(),r=.6+R()*w*.012;c.fillStyle='rgba(255,255,255,.95)';c.beginPath();c.arc(gx,gy,r,0,TAU);c.fill()}c.restore();
  innerShadow(c,p,w*.05,'rgba(80,86,98,.55)',-1,-1);innerShadow(c,p,w*.02,'rgba(255,255,255,.9)',1,1);
  /* the fibre band at a third of its length */
  const bx=x0+w*.3,band=poly([[bx,y0-2],[bx+w*.09,y0-2],[bx+w*.09+2,y0+h*1.42+2],[bx+2,y0+h*1.42+2]]);c.save();c.fillStyle='#8a6a3e';c.fill(band);c.clip(band);
  c.strokeStyle='rgba(50,32,14,.6)';c.lineWidth=1;for(let i=-4;i<20;i++){c.beginPath();c.moveTo(bx-6,y0+i*h*.12);c.lineTo(bx+w*.1+6,y0+i*h*.12+h*.18);c.stroke()}c.restore();
  c.save();c.strokeStyle='rgba(30,18,8,.55)';c.lineWidth=1;c.stroke(band);c.restore()}
/* brass dividers lying on the board */
function dividers(c,x,y,len,ang){c.save();c.translate(x,y);c.rotate(ang);c.shadowBlur=10;c.shadowColor='rgba(0,0,0,.7)';c.shadowOffsetX=4;c.shadowOffsetY=8;
  [[-.09],[.09]].forEach(([a])=>{c.save();c.rotate(a);const p=poly([[-6,0],[6,0],[1.5,len],[-1.5,len]]);c.fillStyle=brassGrad(c,-6,0,6,len);c.fill(p);c.restore()});c.restore();
  c.save();c.translate(x,y);c.rotate(ang);c.beginPath();c.arc(0,0,11,0,TAU);c.fillStyle=brassGrad(c,-11,-11,11,11);c.shadowBlur=6;c.shadowColor='rgba(0,0,0,.6)';c.fill();c.restore();rivet(c,x,y,4)}
/* a pencil */
function pencil(c,x,y,len,ang){c.save();c.translate(x,y);c.rotate(ang);c.shadowBlur=8;c.shadowColor='rgba(0,0,0,.6)';c.shadowOffsetY=6;
  c.fillStyle='#6a4a2a';c.fillRect(0,-6,len,12);c.shadowBlur=0;c.fillStyle='rgba(255,230,180,.25)';c.fillRect(0,-6,len,3);c.fillStyle='#d8b98a';c.beginPath();c.moveTo(len,-6);c.lineTo(len+26,0);c.lineTo(len,6);c.fill();
  c.fillStyle='#2a1a10';c.beginPath();c.moveTo(len+18,-2);c.lineTo(len+26,0);c.lineTo(len+18,2);c.fill();c.restore()}
/* a wooden board: planks, a moulded frame and iron corners */
function board(c,x,y,w,h,{seed=1,planks=5,vertical=true,frame=34,dark=.8}={}){
  const outer=rr(x,y,w,h,12);dropShadow(c,outer,40,'rgba(0,0,0,.8)',0,18);
  const n=planks,pw=(vertical?w:h)/n;
  for(let k=0;k<n;k++){const p=vertical?rr(x+k*pw,y,pw,h,0):rr(x,y+k*pw,w,pw,0);woodAt(c,p,vertical?x+k*pw:x,vertical?y:y+k*pw,vertical?pw:w,vertical?h:pw,seed+k,dark+.06*(k%2),vertical);
    c.save();c.strokeStyle='rgba(20,10,4,.7)';c.lineWidth=3;c.beginPath();if(vertical){c.moveTo(x+k*pw,y);c.lineTo(x+k*pw,y+h)}else{c.moveTo(x,y+k*pw);c.lineTo(x+w,y+k*pw)}c.stroke();c.restore()}
  innerShadow(c,outer,60,'rgba(0,0,0,.55)',0,20);
  if(frame){const fr=new Path2D();fr.addPath(rr(x,y,w,h,12));fr.addPath(rr(x+frame,y+frame,w-2*frame,h-2*frame,4));
    [[x,y,w,frame,false,2],[x,y+h-frame,w,frame,false,5],[x,y,frame,h,true,3],[x+w-frame,y,frame,h,true,7]].forEach(([fx,fy,fw,fh,v,s])=>{const p=new Path2D();p.rect(fx,fy,fw,fh);c.save();c.clip(fr,'evenodd');woodAt(c,p,fx,fy,fw,fh,s,.95,v);c.restore()});
    c.save();c.clip(fr,'evenodd');bevel(c,rr(x,y,w,h,12),1.1);c.restore();innerShadow(c,rr(x+frame,y+frame,w-2*frame,h-2*frame,4),18,'rgba(0,0,0,.7)',4,8);
    [[x,y,1,1],[x+w,y,-1,1],[x,y+h,1,-1],[x+w,y+h,-1,-1]].forEach(([cx,cy,sx,sy])=>{const a=74,t=frame;ironPlate(c,poly([[cx,cy],[cx+sx*a,cy],[cx+sx*a,cy+sy*t],[cx+sx*t,cy+sy*t],[cx+sx*t,cy+sy*a],[cx,cy+sy*a]]),[[cx+sx*17,cy+sy*17],[cx+sx*54,cy+sy*17],[cx+sx*17,cy+sy*54]])})}}
/* a parchment sheet pinned at its corners */
function sheet(c,x,y,w,h,{seed=3,rot=0,tear=6,tacks=4,tone=1,lift=0}={},draw){
  c.save();c.translate(x+w/2,y+h/2);c.rotate(rot);c.translate(-(x+w/2),-(y+h/2));
  const p=scrapPath(x,y,w,h,seed,tear);dropShadow(c,p,lift?30:16,'rgba(0,0,0,.6)',lift?8:3,lift?22:7);c.save();c.clip(p);if(tone!==1)c.filter=`brightness(${tone})`;
  c.drawImage(PARCH_PAGE,x-((seed*37)%Math.max(1,PARCH_PAGE.width-w)),y-((seed*53)%Math.max(1,PARCH_PAGE.height-h)));c.restore();
  innerShadow(c,p,22,'rgba(125,72,24,.55)');innerShadow(c,p,2.5,'rgba(90,52,18,.65)');
  draw&&draw(p);
  const T=[[x+18,y+18],[x+w-18,y+18],[x+18,y+h-18],[x+w-18,y+h-18]];if(tacks===1)tack(c,x+w/2,y+20,8);else if(tacks===2){tack(c,x+20,y+20,8);tack(c,x+w-20,y+20,8)}else if(tacks)T.forEach(([tx,ty])=>tack(c,tx,ty,8));
  c.restore()}
/* the warm light round a lifted (chosen) paper: a soft glow behind its edge */
function rimGlow(c,p){c.save();c.shadowBlur=26;c.shadowColor='rgba(255,186,96,.95)';c.fillStyle='rgba(255,200,120,.9)';c.fill(p);c.fill(p);c.restore()}
/* a slip of paper nailed to a board */
function slip(c,x,y,w,h,{seed=5,rot=0,lift=false,taken=false},draw){
  c.save();c.translate(x+w/2,y+h/2);c.rotate(rot);if(lift)c.scale(1.05,1.05);c.translate(-(x+w/2),-(y+h/2)+(lift?-12:0));
  if(lift){const g=c.createRadialGradient(x+w/2,y+h/2,20,x+w/2,y+h/2,w*.9);g.addColorStop(0,'rgba(255,196,110,.35)');g.addColorStop(1,'rgba(255,170,80,0)');c.fillStyle=g;c.fillRect(x-w*.4,y-h*.4,w*1.8,h*1.8)}
  const p=scrapPath(x,y,w,h,seed,5);dropShadow(c,p,lift?34:12,'rgba(0,0,0,.65)',lift?10:3,lift?26:6);if(lift)rimGlow(c,p);c.save();c.clip(p);c.drawImage(PARCH,x-((seed*37)%Math.max(1,PARCH.width-w)),y-((seed*53)%Math.max(1,PARCH.height-h)));c.restore();
  innerShadow(c,p,14,'rgba(125,72,24,.55)');innerShadow(c,p,2.5,'rgba(90,52,18,.65)');
  /* a fold across it, and the corner curling */
  c.save();c.clip(p);const fg=c.createLinearGradient(x,y+h*.42,x,y+h*.5);fg.addColorStop(0,'rgba(90,52,18,0)');fg.addColorStop(.5,'rgba(90,52,18,.18)');fg.addColorStop(.52,'rgba(255,245,220,.25)');fg.addColorStop(1,'rgba(255,245,220,0)');c.fillStyle=fg;c.fillRect(x,y+h*.42,w,h*.08);c.restore();
  draw&&draw();
  if(taken){waxSeal(c,x+w-40,y+40,30,'',11);sealGlyph(c,'anchor',x+w-40,y+40,30)}
  nail(c,x+w/2,y+18,7);c.restore()}
/* a name board nailed over a frame's top, its letters burnt in */
function plaque(c,t,cx,y,w,size=62,seed=31){const x=cx-w/2,hb=rr(x,y,w,96,10);dropShadow(c,hb,20,'rgba(0,0,0,.8)',0,10);woodAt(c,hb,x,y,w,96,seed,.95);bevel(c,hb,1);
  [[x+30,y+48],[x+w-30,y+48]].forEach(([a,b])=>nail(c,a,b,7));burn(c,t,cx,y+70,`400 ${size}px "IM Fell English SC",Georgia,serif`,{align:'center',ls:size*.22})}
/* the hints along the bottom, as the satchel's */
function hints(c,items,y=1052){let tw=0;const f='400 27px Alegreya,Georgia,serif';items.forEach(([keys,l])=>{tw+=keys.reduce((a,k)=>a+38+(k.length>1?(k.length-1)*12:0)+8,0)+measure(c,l,f)+70});let x=960-tw/2;
  items.forEach(([keys,l])=>{keys.forEach(k=>{x+=keycap(c,k,x,y-28,38)+8});x+=6;txt(c,l,x,y,f,'#f0e2c4',SH);x+=measure(c,l,f)+64})}

/* ===== 1. A person speaks ======================================================================================== */
function speaker(c,x,foot,w,{name,seal,line,choices,sel=0,seed=21}){
  /* the scrap: a torn half sheet, the speaker's mark in wax at its corner; it grows upwards from its foot */
  const LF=`400 32px ${FL}`,lines=splitLines(c,line,LF,w-150),LH=42,CH=54,h=104+lines.length*LH+22+choices.length*CH+14,y=foot-h;
  scrap(c,x,y,w,h,{seed,rot:-.006,tear:6},()=>{
    const L=x+92;
    inkText(c,name.toUpperCase(),L,y+52,24,{sc:true,col:'rgba(107,74,44,.95)'});
    quillRule(c,L,L+measure(c,name.toUpperCase(),`400 24px ${FSC}`)+30,y+64,4,1.4);
    hand(c,line,L,y+112,32,{italic:false,maxW:w-150,lh:LH,rot:-.004,col:'rgba(42,24,10,.94)'});
    let yy=y+104+lines.length*LH+22+CH/2;
    choices.forEach(([k,t],i)=>{const on=i===sel,ky=yy+i*CH;
      if(on){const g=c.createLinearGradient(L-20,0,L+640,0);g.addColorStop(0,'rgba(255,236,190,.0)');g.addColorStop(.08,'rgba(255,236,190,.55)');g.addColorStop(1,'rgba(255,236,190,0)');c.fillStyle=g;c.fillRect(L-30,ky-28,700,52)}
      keyScrapL(c,L+20,ky-4,k,44,30+i);
      txt(c,t,L+62,ky+6,`400 29px ${FL}`,on?'rgba(30,16,6,.98)':'rgba(58,38,22,.85)');
      if(on)quillRule(c,L+62,L+62+measure(c,t,`400 29px ${FL}`),ky+16,7+i,1.8)})});
  waxSeal(c,x+38,y+46,34,'',seed);sealGlyph(c,seal,x+38,y+46,34);return h}
async function drawSpeak(c,bg){backdrop(c,bg,{blur:0,dark:.92,vig:.45});
  /* the line low in the middle, where the prompt sits */
  speaker(c,480,1036,960,{name:'The Harbour Keeper',seal:'anchor',line:'“Another one from the Gale. Nobody leaves; you get used to it. That old boat at the pier was the Cartographer’s. Make her sail and she’s yours.”',
    choices:[['1','Who was the Cartographer?'],['2','The sail is torn.'],['E','I’ll see to her.']],sel:0})}
async function drawSpeakB(c,bg){backdrop(c,bg,{blur:0,dark:.88,vig:.45});
  /* the speaker's place (no figure yet) and a small scrap pinned beside it, a twine to the hook */
  c.save();c.setLineDash([10,10]);c.strokeStyle='rgba(242,226,191,.65)';c.lineWidth=2.5;c.beginPath();c.ellipse(1090,560,58,128,0,0,TAU);c.stroke();c.restore();
  txt(c,'the speaker',1090,720,'italic 400 24px "IM Fell English",Georgia,serif','#f0e2c4',{align:'center',...SH});
  c.save();c.lineCap='round';c.strokeStyle='#c7ae80';c.lineWidth=3;c.shadowBlur=4;c.shadowColor='rgba(0,0,0,.7)';c.beginPath();c.moveTo(1150,470);c.quadraticCurveTo(1200,430,1236,402);c.stroke();c.restore();
  scrap(c,1220,388,500,200,{seed:44,rot:.025,tear:5},()=>{
    inkText(c,'THE HARBOUR KEEPER',1300,438,20,{sc:true,col:'rgba(107,74,44,.95)'});
    hand(c,'“Mind the reef by the Spire. She doesn’t like rocks.”',1300,486,28,{italic:false,maxW:380,lh:36});
    keyScrapL(c,1650,548,'E',40,9);txt(c,'go on',1588,556,`400 24px ${FL}`,'rgba(58,38,22,.85)',{align:'right'})});
  waxSeal(c,1252,428,26,'',21);sealGlyph(c,'anchor',1252,428,26);tack(c,1468,398,8)}

/* ===== 2. The Shipwright's plans ================================================================================= */
function inkTarSeams(c,x,y){
  /* a hull's planks in section, the seams payed with tar, the brush and the pot beside: ink with a little wash */
  const pl=[[0,0],[0,52],[0,104],[0,156]];
  pl.forEach(([dx,dy],k)=>{const yy=y+dy;c.save();c.fillStyle=`rgba(176,128,74,${.18+.05*(k%2)})`;c.beginPath();c.moveTo(x,yy+6);c.quadraticCurveTo(x+260,yy-14,x+520,yy+6);c.lineTo(x+520,yy+46);c.quadraticCurveTo(x+260,yy+26,x,yy+46);c.closePath();c.fill();c.restore();
    inkCurve(c,t=>[x+520*t,yy+6-20*Math.sin(t*Math.PI)*.95],30,2.2,undefined,k*3+1);inkCurve(c,t=>[x+520*t,yy+46-20*Math.sin(t*Math.PI)*.95],30,2.2,undefined,k*3+2);
    for(let g=0;g<5;g++){const gx=x+60+g*100+(k*23)%40;inkCurve(c,t=>[gx+t*70,yy+20-20*Math.sin((gx-x+t*70)/520*Math.PI)*.95+t*2],8,1,'rgba(48,30,16,.45)',k*7+g)}
    if(k<3){c.save();c.strokeStyle='rgba(20,12,6,.9)';c.lineWidth=7;c.lineCap='round';c.beginPath();for(let i=0;i<=40;i++){const t=i/40,xx=x+6+508*t,yy2=yy+49-20*Math.sin(t*Math.PI)*.95;i?c.lineTo(xx,yy2):c.moveTo(xx,yy2)}c.stroke();c.restore()}});
  /* the pot of tar and its brush */
  const px=x+600,py=y+120;inkCurve(c,t=>[px-34+68*t,py-40],10,2.2);inkCurve(c,t=>[px-34-4*Math.sin(t*Math.PI),py-40+90*t],12,2.2);inkCurve(c,t=>[px+34+4*Math.sin(t*Math.PI),py-40+90*t],12,2.2);
  inkCurve(c,t=>[px-34+68*t,py+50+6*Math.sin(t*Math.PI)],12,2.2);c.save();c.fillStyle='rgba(20,12,6,.85)';c.beginPath();c.ellipse(px,py-40,34,9,0,0,TAU);c.fill();c.restore();
  for(let h=0;h<7;h++)inkCurve(c,t=>[px-26+h*9,py-28+t*70],4,.9,'rgba(48,30,16,.4)',h);
  inkCurve(c,t=>[px+10+t*80,py-62-t*70],8,3,undefined,9);c.save();c.fillStyle='rgba(30,18,8,.9)';c.translate(px+92,py-136);c.rotate(-.7);c.beginPath();c.ellipse(0,0,22,10,0,0,TAU);c.fill();c.restore();
  hand(c,'every seam payed hot, below the wale and above',x,y+222,22,{col:'rgba(70,46,26,.85)'});
  /* the scale bar of a draught: an ink rule with ticks */
  inkPath(c,[[x,y+250],[x+200,y+250]],1.6);for(let i=0;i<=4;i++)inkPath(c,[[x+i*50,y+244],[x+i*50,y+256]],1.2);hand(c,'1 ft',x+210,y+256,18,{italic:true})}
async function drawPlans(c,bg){backdrop(c,bg,{blur:8,dark:.5,vig:.7});
  board(c,150,70,1620,880,{seed:11,planks:9,vertical:true,frame:38,dark:.72});
  plaque(c,'THE SLIP',960,30,560,56,33);
  /* the list: slips pinned down the left */
  const plans=[['Reef points','done'],['Tarred seams','sel'],['A new sail',''],['A stern lantern',''],['Ribs and knees','lock'],['A salvage winch','lock']];
  plans.forEach(([n,st],i)=>{const sx=230,sy=200+i*108,w=360,h=86,seed=60+i,rot=((i*37)%5-2)*.006,lift=st==='sel';
    c.save();c.translate(sx+w/2,sy+h/2);c.rotate(rot);if(lift)c.translate(14,-4);c.translate(-(sx+w/2),-(sy+h/2));
    if(lift){const g=c.createRadialGradient(sx+w/2,sy+h/2,10,sx+w/2,sy+h/2,w*.75);g.addColorStop(0,'rgba(255,196,110,.38)');g.addColorStop(1,'rgba(255,170,80,0)');c.fillStyle=g;c.fillRect(sx-80,sy-80,w+160,h+160)}
    const p=scrapPath(sx,sy,w,h,seed,4);dropShadow(c,p,lift?26:10,'rgba(0,0,0,.65)',lift?8:2,lift?16:5);if(lift)rimGlow(c,p);c.save();c.clip(p);if(st==='lock')c.filter='brightness(.78) saturate(.7)';c.drawImage(PARCH,sx-(seed*37)%300,sy-(seed*53)%200);c.restore();
    innerShadow(c,p,12,'rgba(125,72,24,.55)');innerShadow(c,p,2.5,'rgba(90,52,18,.65)');
    txt(c,n,sx+46,sy+56,`${st==='lock'?'italic ':''}400 32px ${FL}`,st==='lock'?'rgba(80,58,38,.6)':st==='done'?'rgba(70,48,28,.7)':INK);
    if(st==='done'){quillRule(c,sx+40,sx+50+measure(c,n,`400 32px ${FL}`),sy+46,i,2.2);waxSeal(c,sx+w-42,sy+h/2,24,'',33);sealGlyph(c,'anchor',sx+w-42,sy+h/2,24)}
    if(st==='lock'){c.save();c.strokeStyle='rgba(60,40,22,.55)';c.lineWidth=2;const lx=sx+w-46,ly=sy+h/2;c.beginPath();c.arc(lx,ly-8,9,Math.PI,0);c.stroke();c.strokeRect(lx-12,ly-6,24,18);c.restore()}
    tack(c,sx+22,sy+h/2,7);
    c.restore()});
  /* the purse */
  saltCake(c,282,878,70,5);countCoin(c,318,900,240,20);txt(c,'in your purse',352,892,'italic 400 26px "IM Fell English",Georgia,serif','#f0e2c4',SH);
  /* the chosen plan, pinned large */
  sheet(c,650,190,1040,690,{seed:9,rot:.006,tear:7,tacks:4},()=>{
    inkText(c,'Tarred seams',720,282,60);hand(c,'for the double-ender · the Shipwright’s hand',724,322,24,{col:'rgba(90,62,36,.9)'});
    inkTarSeams(c,730,368);
    wrap(c,'Every seam payed with hot pine tar. Small leaks close by themselves, and the sea comes in through a hole a third slower.',1450,420,200,32,'400 24px Alegreya,Georgia,serif','rgba(58,38,22,.92)');
    quillRule(c,720,1620,680,12,1.4);inkText(c,'WHAT IT TAKES',720,718,20,{sc:true,col:'rgba(107,74,44,.95)'})});
  /* what it takes: a stitched leather roll across the foot of the sheet, its pockets pressed rings, as the satchel's */
  const rx=700,ry=742,rw=820,rh=148,roll=rr(rx,ry,rw,rh,22);dropShadow(c,roll,22,'rgba(0,0,0,.75)',0,10);leatherAt(c,roll,rx,ry,rw,rh,3,.95);innerShadow(c,roll,26,'rgba(0,0,0,.6)',0,10);bevel(c,roll,1);
  stitches(c,rrPts(rx+12,ry+12,rw-24,rh-24,14),{step:15,len:8});
  const need=[['Tar','Keg of tar',1,2],['Canvas','Sailcloth',0,1],['RopeCoil','Coil of rope',3,2]];
  need.forEach(([id,n,have,want],i)=>{const cx=rx+104+i*184,cy=ry+rh/2-6;ringSlot(c,cx,cy,{id,c:1},false,46);
    const short=have<want,lab=`${have} / ${want}`;const p=rr(cx+18,cy+22,74,34,17);dropShadow(c,p,5,'rgba(0,0,0,.7)',1,3);const g=c.createLinearGradient(0,cy+22,0,cy+56);g.addColorStop(0,short?'#8a2a1c':'#6a4228');g.addColorStop(1,short?'#4a1008':'#2a160b');c.fillStyle=g;c.fill(p);
    c.save();c.lineWidth=2.4;c.strokeStyle=brassGrad(c,cx+18,cy+22,cx+92,cy+56);c.stroke(p);c.restore();txt(c,lab,cx+55,cy+40,'700 20px Alegreya,Georgia,serif','#f3e6c8',{align:'center',base:'middle'})});
  /* the salt, on its own cake at the roll's end */
  stitches(c,[[rx+640,ry+24],[rx+640,ry+rh-24]],{step:13,len:7,w:2.4});
  saltCake(c,rx+730,ry+rh/2-10,96,7);countCoin(c,rx+776,ry+rh/2+28,120,22);
  /* the Shipwright's things on the board */
  dividers(c,1640,610,160,.55);pencil(c,1520,905,150,-.08);
  hints(c,[[['E'],'have it fitted at the fitting berth'],[['Q','E'],'plans'],[['Esc'],'step back']])}

/* ===== 3. The log ================================================================================================ */
function bookCorner(c,x,y,sx,sy){const a=70;const p=poly([[x,y],[x+sx*a,y],[x,y+sy*a]]);c.save();dropShadow(c,p,6,'rgba(0,0,0,.6)',1,3);c.fillStyle=brassGrad(c,x,y,x+sx*a,y+sy*a);c.fill(p);
  c.strokeStyle='rgba(40,25,5,.7)';c.lineWidth=1.4;c.stroke(p);c.restore();rivet(c,x+sx*18,y+sy*18,4);rivet(c,x+sx*40,y+sy*14,3);rivet(c,x+sx*14,y+sy*40,3)}
function inkPalmSketch(c,x,y){
  inkCurve(c,t=>[x+t*30,y-t*150+Math.sin(t*3)*8],20,3.2,undefined,2);
  [[-80,-30],[-60,-70],[10,-80],[70,-50],[80,0],[-30,20]].forEach(([dx,dy],i)=>inkCurve(c,t=>[x+30+dx*t,y-150+dy*t+Math.sin(t*Math.PI)*(-18)],12,2.2,undefined,i+4));
  for(let i=0;i<5;i++)inkPath(c,[[x+3+i*5,y-20-i*26],[x+12+i*5,y-26-i*26]],1.4);
  inkCurve(c,t=>[x-140+t*420,y+10+Math.sin(t*5)*3],30,2,undefined,8);
  for(let i=0;i<6;i++)inkCurve(c,t=>[x+170+i*22+t*14,y+26+i*3],3,1.2,'rgba(48,30,16,.55)',i);
  c.save();c.setLineDash([6,7]);inkPath(c,[[x+30,y+2],[x+168,y+2]],1.6);c.restore();
  inkPath(c,[[x+176,y-8],[x+196,y+12]],3,'rgba(122,26,16,.9)',3);inkPath(c,[[x+196,y-8],[x+176,y+12]],3,'rgba(122,26,16,.9)',4);
  hand(c,'three paces seaward',x+40,y-10,20,{col:'rgba(48,30,16,.8)'})}
function glyph(c,kind,x,y){c.save();c.strokeStyle='rgba(58,38,22,.8)';c.lineWidth=2;c.lineCap='round';c.beginPath();
  if(kind==='anchor'){c.arc(x,y-9,3,0,TAU);c.moveTo(x,y-6);c.lineTo(x,y+9);c.moveTo(x-6,y-3);c.lineTo(x+6,y-3);c.moveTo(x-8,y+3);c.quadraticCurveTo(x-6,y+10,x,y+9);c.quadraticCurveTo(x+6,y+10,x+8,y+3)}
  if(kind==='shovel'){c.moveTo(x-8,y-10);c.lineTo(x+4,y+2);c.moveTo(x+2,y);c.quadraticCurveTo(x+14,y+2,x+10,y+12);c.quadraticCurveTo(x+2,y+12,x+2,y);c.moveTo(x-11,y-13);c.lineTo(x-5,y-7)}
  if(kind==='tower'){c.moveTo(x-6,y+10);c.lineTo(x-5,y-8);c.lineTo(x+5,y-8);c.lineTo(x+6,y+10);c.moveTo(x-6,y-8);c.lineTo(x-6,y-12);c.moveTo(x,y-8);c.lineTo(x,y-12);c.moveTo(x+6,y-8);c.lineTo(x+6,y-12)}
  c.stroke();c.restore()}
/* a charcoal rubbing of the Spire's stone on thin paper: the cut letters stay pale, the stone's face takes the charcoal */
function rubbing(c,x,y,w,h,rot,text){c.save();c.translate(x+w/2,y+h/2);c.rotate(rot);c.translate(-(x+w/2),-(y+h/2));
  const p=scrapPath(x,y,w,h,77,3);paperFill(c,p,x,y,w,h,77,1.04);
  /* charcoal laid on in strokes, heavier in the middle; the cut letters (worn, a little skewed) stay paper */
  const cv=canvas(w,h),k=cv.getContext('2d'),R=rng(91);k.lineCap='round';
  for(let i=0;i<260;i++){const cx=w*(.06+R()*.88),cy=h*(.1+R()*.8),l=30+R()*70,a=-.5+R()*.18;k.strokeStyle=`rgba(36,33,30,${.10+R()*.12})`;k.lineWidth=8+R()*16;
    k.beginPath();k.moveTo(cx-Math.cos(a)*l/2,cy-Math.sin(a)*l/2);k.lineTo(cx+Math.cos(a)*l/2,cy+Math.sin(a)*l/2);k.stroke()}
  k.globalCompositeOperation='destination-out';k.save();k.translate(w/2+6,h*.7);k.transform(1,-.03,-.1,1,0,0);
  k.font=`900 ${Math.round(h*.44)}px "Arial Black",Arial,sans-serif`;k.textAlign='center';k.globalAlpha=.85;k.fillText(text,0,0);k.restore();
  k.globalCompositeOperation='source-over';for(let i=0;i<14;i++){k.fillStyle=`rgba(36,33,30,${.25+R()*.3})`;k.beginPath();k.ellipse(w*(.15+R()*.75),h*(.35+R()*.4),6+R()*16,3+R()*8,R()*3,0,TAU);k.fill()}
  k.globalCompositeOperation='destination-out';k.globalAlpha=.75;k.drawImage(GRAIN,-(w*.3|0),0);k.globalAlpha=1;
  k.globalCompositeOperation='destination-in';const g=k.createRadialGradient(w/2,h/2,h*.25,w/2,h/2,w*.55);g.addColorStop(0,'rgba(0,0,0,1)');g.addColorStop(.75,'rgba(0,0,0,.6)');g.addColorStop(1,'rgba(0,0,0,0)');k.fillStyle=g;k.fillRect(0,0,w,h);
  c.save();c.clip(p);c.globalAlpha=.9;c.drawImage(cv,x,y);c.restore();
  /* two strips of paper tape over its corners */
  [[x+6,y+4,-.6],[x+w-6,y+h-4,-.6]].forEach(([a,b,r],i)=>{c.save();c.translate(a,b);c.rotate(r);const t=rr(-34,-11,68,22,2);c.globalAlpha=.82;c.save();c.clip(t);c.drawImage(PARCH,-120-i*60,-60);c.restore();c.globalAlpha=1;innerShadow(c,t,4,'rgba(125,72,24,.45)');c.restore()});
  c.restore()}
async function drawLog(c,bg){backdrop(c,bg,{blur:8,dark:.45,vig:.75});
  const X=230,Y=110,Wd=1460,Ht=860;
  /* the cover: dark stitched leather, brass corners, the strap and buckle on the right */
  const cover=rr(X,Y,Wd,Ht,26);dropShadow(c,cover,50,'rgba(0,0,0,.85)',0,24);c.save();c.clip(cover);c.drawImage(LEATHER_DARK,X,Y);c.restore();innerShadow(c,cover,40,'rgba(0,0,0,.65)',0,14);bevel(c,cover,1.4);
  stitches(c,rrPts(X+16,Y+16,Wd-32,Ht-32,16),{step:16,len:9});
  [[X+2,Y+2,1,1],[X+Wd-2,Y+2,-1,1],[X+2,Y+Ht-2,1,-1],[X+Wd-2,Y+Ht-2,-1,-1]].forEach(([x,y,sx,sy])=>bookCorner(c,x,y,sx,sy));
  /* the pages: a stack's edge, the two leaves curving into the gutter */
  const PX=X+48,PY=Y+40,PW=Wd-96,PH=Ht-80,mid=PX+PW/2;
  for(let k=6;k>=1;k--){const e=rr(PX-k*2.5,PY+k*3,PW+k*5,PH,8);c.save();c.fillStyle=k%2?'#d8c39a':'#c9b083';c.fill(e);c.restore()}
  [[PX,mid,1],[mid,PX+PW,-1]].forEach(([a,b,side],li)=>{const pg=new Path2D();pg.moveTo(a,PY+(side>0?8:0));pg.quadraticCurveTo((a+b)/2,PY-10,b,PY+(side>0?0:8));pg.lineTo(b,PY+PH-(side>0?0:6));pg.quadraticCurveTo((a+b)/2,PY+PH+10,a,PY+PH-(side>0?6:0));pg.closePath();
    dropShadow(c,pg,10,'rgba(0,0,0,.5)',0,4);c.save();c.clip(pg);c.drawImage(PARCH_PAGE,a-(li*211)%300,PY-(li*97)%200);c.restore();
    const gx=side>0?b:a,g=c.createLinearGradient(gx-side*0,0,gx-side*110,0);g.addColorStop(0,'rgba(70,40,14,.55)');g.addColorStop(1,'rgba(70,40,14,0)');c.save();c.clip(pg);c.fillStyle=g;c.fillRect(Math.min(gx,gx-side*110),PY-20,110,PH+40);c.restore();
    innerShadow(c,pg,24,'rgba(125,72,24,.5)');innerShadow(c,pg,2,'rgba(90,52,18,.6)')});
  /* a water ring on the left page, a torn corner */
  c.save();c.globalAlpha=.25;c.strokeStyle='#8a5a2a';c.lineWidth=5;c.filter='blur(1px)';c.beginPath();c.arc(PX+560,PY+660,58,.3,5.6);c.stroke();c.restore();
  const tc=poly([[PX,PY+PH-110],[PX+70,PY+PH],[PX,PY+PH]]);c.save();c.fillStyle='#3a2416';c.fill(tc);c.restore();
  /* left: the Cartographer's page in their hand */
  const L=PX+70;inkText(c,'THE CARTOGRAPHER',L,PY+70,20,{sc:true,col:'rgba(107,74,44,.95)'});txt(c,'page i',mid-60,PY+70,'italic 400 20px "IM Fell English",Georgia,serif','rgba(107,74,44,.9)',{align:'right'});
  quillRule(c,L,mid-60,PY+84,5,1.3);
  hand(c,'The light on the Spire burns again. Something is waking, and I mean to be there when it does.',L,PY+138,32,{maxW:560,lh:44,rot:-.02});
  hand(c,'Spare canvas under the leaning palm, three paces seaward. I never trusted a store.',L,PY+282,29,{maxW:560,lh:40,rot:-.015});
  inkPalmSketch(c,L+110,PY+560);
  hand(c,'(the rest of the page is torn away)',L,PY+PH-60,22,{col:'rgba(90,62,36,.75)'});
  /* right: your notes, written by the game */
  const R=mid+60;inkText(c,'NOTES',R,PY+70,20,{sc:true,col:'rgba(107,74,44,.95)'});txt(c,'day 1',PX+PW-70,PY+70,'italic 400 20px "IM Fell English",Georgia,serif','rgba(107,74,44,.9)',{align:'right'});
  quillRule(c,R,PX+PW-70,PY+84,6,1.3);
  const notes=[['anchor','Morning · Saltbound Cove','Woke on the beach after the storm. The Harbour Keeper says nobody leaves. He gave me the Cartographer’s old boat, if I can make her sail.'],
    ['shovel','Morning · the pier’s beach','Dug up the canvas under the palm, as the page said. A pale float came up with it: hollow, light, a number pressed into it. 17.'],
    ['tower','Dusk · the Spire','A ring of light round the old tower’s top. Writing on the stone, or something like it. I took a rubbing.']];
  rubbing(c,R+40,PY+562,440,150,-.025,'ERTH 17');hand(c,'from the Spire’s stone · 17 again?',R+60,PY+752,22,{col:'rgba(70,46,26,.85)'});
  let yy=PY+136;notes.forEach(([g,h,t])=>{glyph(c,g,R+10,yy-8);inkText(c,h.toUpperCase(),R+34,yy,17,{sc:true,col:'rgba(122,86,52,.95)'});yy+=34;yy=wrap(c,t,R+34,yy,560,31,'400 23px Alegreya,Georgia,serif','rgba(48,30,16,.92)')+52});
  /* the ribbon marker from the top of the spine */
  const rb=poly([[mid-14,Y+20],[mid+14,Y+20],[mid+18,Y+Ht-40],[mid+2,Y+Ht-62],[mid-14,Y+Ht-40]]);dropShadow(c,rb,8,'rgba(0,0,0,.6)',2,4);const rg=c.createLinearGradient(mid-14,0,mid+18,0);rg.addColorStop(0,'#5a1810');rg.addColorStop(.5,'#9a3424');rg.addColorStop(1,'#4a120a');c.fillStyle=rg;c.fill(rb);
  /* the tabs: stitched leather tongues along the right edge, debossed */
  ['PAGES','NOTES','WORDS','PLACES','PLANS'].forEach((t,i)=>{const on=i===0,tx=X+Wd-14+(on?22:0),ty=Y+120+i*96,tw=150,th=74,p=rr(tx,ty,tw,th,[0,14,14,0]);
    dropShadow(c,p,12,'rgba(0,0,0,.7)',4,6);leatherAt(c,p,tx,ty,tw,th,i*3+2,on?1.25:.95);bevel(c,p,.7);stitches(c,[[tx+10,ty+10],[tx+tw-14,ty+10],[tx+tw-14,ty+th-10],[tx+10,ty+th-10]],{step:12,len:6,w:2});
    deboss(c,t,tx+tw/2-4,ty+th/2+8,'400 22px "IM Fell English SC",Georgia,serif',{align:'center',ls:2})});
  /* the strap that closes it, open and lying out to the left, its buckle at the end */
  const sy0=Y+Ht*.46,st=rr(X-120,sy0,170,60,10);dropShadow(c,st,12,'rgba(0,0,0,.7)',4,6);leatherAt(c,st,X-120,sy0,170,60,9,.85);bevel(c,st,.6);stitches(c,[[X-108,sy0+9],[X+40,sy0+9]],{step:12,len:6,w:2});stitches(c,[[X-108,sy0+51],[X+40,sy0+51]],{step:12,len:6,w:2});
  c.save();const bk=new Path2D();bk.addPath(rr(X-176,sy0-10,64,80,10));bk.addPath(rr(X-164,sy0+2,40,56,5));dropShadow(c,bk,8,'rgba(0,0,0,.8)',2,6,'evenodd');c.fillStyle=brassGrad(c,X-176,sy0-10,X-112,sy0+70);c.fill(bk,'evenodd');c.restore();
  c.save();c.fillStyle=brassGrad(c,X-150,sy0,X-140,sy0+60);c.shadowBlur=4;c.shadowColor='rgba(0,0,0,.7)';c.fillRect(X-148,sy0-4,8,68);c.restore();
  hints(c,[[['Q','E'],'turn the pages'],[['A','D'],'the tabs'],[['L'],'close the log']])}

/* ===== 4. The job board ========================================================================================== */
async function drawBoard(c,bg){backdrop(c,bg,{blur:8,dark:.5,vig:.7});
  board(c,140,60,1640,920,{seed:21,planks:11,vertical:true,frame:40,dark:.68});
  /* the header: a board of its own, letters cut and painted */
  plaque(c,'TALLY',960,26,520,62,31);
  /* what is left of slips torn down, and empty nail holes */
  [[860,640,81,.06],[960,880,82,-.08],[300,890,83,.04]].forEach(([x,y,sd,r])=>{scrap(c,x-56,y-14,112,38,{seed:sd,rot:r,tear:7,alpha:.92});nail(c,x,y,6)});
  [[820,560],[1330,900],[700,930],[1460,560]].forEach(([x,y])=>{c.save();c.fillStyle='rgba(12,6,2,.8)';c.beginPath();c.arc(x,y,3.2,0,TAU);c.fill();c.restore()});
  const jobs=[
    {x:230,y:180,w:330,h:320,rot:-.03,seed:71,t:'Driftwood for the Slip',d:'Ten pieces, dry. The Shipwright pays on the weighing bench.',k:'bring',pay:40,sk:'drift'},
    {x:610,y:160,w:340,h:330,rot:.022,seed:72,t:'The Spire’s reef',d:'Sail round the Spire and put its reef on a chart. The Archive wants it before the fog season.',k:'survey',pay:70,sk:'tower',lift:true},
    {x:1000,y:190,w:330,h:330,rot:-.012,seed:73,t:'A box on the islet',d:'A sailor buried his pay on the lagoon’s cay, under a stake. His widow wants it back.',k:'dig',pay:90,sk:'stake',taken:true},
    {x:1380,y:170,w:320,h:310,rot:.035,seed:74,t:'Floats for the nets',d:'Three glass floats, whole. The fishers at Longshoal pay well.',k:'deliver',pay:60,sk:'float'},
    {x:420,y:570,w:330,h:310,rot:.02,seed:75,t:'A whale off Ruin Isle',d:'Seen twice at dawn. Find it with the spyglass and say where.',k:'sighting',pay:50,sk:'whale'},
    {x:1110,y:580,w:330,h:300,rot:-.025,seed:76,t:'Tar for the Purser',d:'Two kegs, before the third day is out.',k:'bring',pay:45,sk:'keg'}];
  const sketch=(kind,x,y)=>{if(kind==='tower'){inkPath(c,[[x-20,y+40],[x-16,y-10],[x+16,y-10],[x+20,y+40]],2);inkPath(c,[[x-40,y+40],[x+40,y+40]],1.6);for(let i=0;i<5;i++)inkPath(c,[[x-30+i*15,y+48],[x-24+i*15,y+52]],1.2)}
    if(kind==='whale'){inkCurve(c,t=>[x-50+t*100,y+Math.sin(t*Math.PI)*-24],20,2.2);inkCurve(c,t=>[x-50+t*100,y+8],10,1.6);inkPath(c,[[x+50,y],[x+64,y-14],[x+62,y+2],[x+70,y+10]],2);inkPath(c,[[x-30,y-26],[x-34,y-40],[x-26,y-40]],1.4)}
    if(kind==='stake'){inkPath(c,[[x,y-30],[x+4,y+30]],3);inkPath(c,[[x-3,y-22],[x+12,y-26],[x+18,y-14]],2,'rgba(122,26,16,.85)');inkCurve(c,t=>[x-40+t*80,y+30+Math.sin(t*Math.PI)*-6],10,1.6)}
    if(kind==='drift'){inkCurve(c,t=>[x-46+t*92,y+Math.sin(t*4)*6],14,4);inkPath(c,[[x,y+2],[x+12,y+22]],3)}
    if(kind==='float'){c.save();c.strokeStyle='rgba(48,30,16,.85)';c.lineWidth=2.2;c.beginPath();c.arc(x,y,24,0,TAU);c.stroke();c.lineWidth=1.2;c.beginPath();c.ellipse(x,y,12,24,0,0,TAU);c.moveTo(x-24,y);c.lineTo(x+24,y);c.stroke();c.restore();inkPath(c,[[x,y-24],[x,y-38]],2)}
    if(kind==='keg'){inkPath(c,[[x-22,y-26],[x+22,y-26],[x+26,y+26],[x-26,y+26],[x-22,y-26]],2);inkPath(c,[[x-25,y-10],[x+25,y-10]],1.4);inkPath(c,[[x-26,y+12],[x+26,y+12]],1.4)}};
  jobs.forEach(j=>slip(c,j.x,j.y,j.w,j.h,{seed:j.seed,rot:j.rot,lift:j.lift,taken:j.taken},()=>{
    const L=j.x+30;hand(c,j.t,L,j.y+76,32,{italic:false,rot:-.01,col:'rgba(36,20,8,.95)'});
    wrap(c,j.d,L,j.y+118,j.w-60,28,'italic 400 22px "IM Fell English",Georgia,serif','rgba(58,38,22,.9)');
    sketch(j.sk,j.x+j.w-84,j.y+j.h-104);
    inkText(c,j.k.toUpperCase(),L,j.y+j.h-30,18,{sc:true,col:'rgba(107,74,44,.95)'});
    saltCake(c,j.x+j.w-108,j.y+j.h-40,42,j.seed);txt(c,String(j.pay),j.x+j.w-80,j.y+j.h-29,'700 25px Alegreya,Georgia,serif','rgba(48,30,16,.92)')}));
  /* the chalk slate on its nail, the Purser's tally */
  const sx=1500,sy=620,sw=200,sh=260,fr=rr(sx,sy,sw,sh,8);dropShadow(c,fr,20,'rgba(0,0,0,.8)',4,10);woodAt(c,fr,sx,sy,sw,sh,41,.9);bevel(c,fr,.8);
  const sl=rr(sx+16,sy+16,sw-32,sh-32,4);c.save();c.clip(sl);c.drawImage(SLATE,sx,sy);c.restore();innerShadow(c,sl,12,'rgba(0,0,0,.8)',2,4);
  c.save();c.globalAlpha=.85;txt(c,'pay at',sx+sw/2,sy+76,'italic 400 26px "IM Fell English",Georgia,serif','#e8e4d8',{align:'center'});txt(c,'the hatch',sx+sw/2,sy+108,'italic 400 26px "IM Fell English",Georgia,serif','#e8e4d8',{align:'center'});
  c.strokeStyle='#e8e4d8';c.lineWidth=3;c.lineCap='round';for(let g=0;g<2;g++){for(let i=0;i<4;i++){c.beginPath();c.moveTo(sx+44+g*70+i*12,sy+150);c.lineTo(sx+46+g*70+i*12,sy+200);c.stroke()}c.beginPath();c.moveTo(sx+36+g*70,sy+190);c.lineTo(sx+90+g*70,sy+160);c.stroke()}c.restore();
  nail(c,sx+sw/2,sy-6,7);c.save();c.strokeStyle='#c7ae80';c.lineWidth=2.5;c.beginPath();c.moveTo(sx+30,sy+16);c.lineTo(sx+sw/2,sy-6);c.lineTo(sx+sw-30,sy+16);c.stroke();c.restore();
  hints(c,[[['E'],'take the job'],[['A','D'],'the next slip'],[['Esc'],'step back']])}

/* ===== start ===== */
(async()=>{
  await new Promise(r=>{const l=document.querySelector('link[rel=stylesheet]');if(!l||l.sheet)return r();l.addEventListener('load',r);l.addEventListener('error',r);setTimeout(r,4000)});
  try{await Promise.all(['400 40px "IM Fell English"','italic 400 40px "IM Fell English"','400 40px "IM Fell English SC"','400 20px Alegreya','700 20px Alegreya','italic 400 20px Alegreya'].map(f=>document.fonts.load(f)))}catch(e){}
  for(const k in ICONS)ICON[k]=await img(ICONS[k]);
  const BGS={};for(const k in BG)BGS[k]=await img(BG[k]);
  PARCH=parchment(1100,640,6);PARCH_BIG=parchment(1400,260,8);PARCH_PAGE=parchment(1400,1000,11);PARCH_TAG=parchment(300,460,5);
  SPECK=tex(1400,80,(x,y)=>{const n=N3(x/1.7,y/1.7),m=N1(x/12,y/12);return[95,62,30,clamp(sstep(.28,.62,n)*.9+sstep(.35,.75,m)*.45)*255]});
  LEATHER_BIG=leather(1400,800,[118,70,40],1);LEATHER_DARK=leather(1500,900,[74,40,22],4,{scuff:.8});LEATHER_DISC=leather(200,200,[70,42,24],2,{scuff:.6});LEATHER_SMALL=leather(140,110,[128,78,44],3);
  WOOD=wood(1800,1000,[156,104,60],1,[[220,140,26],[640,420,22],[860,90,18],[1300,620,24],[1560,200,20]]);
  GRAIN=tex(900,200,(x,y)=>{const n=N3(x/1.3,y/1.3),s=N1((x+y*.35)/3,(y-x*.2)/40);return[0,0,0,clamp(sstep(.5,.85,n)*.8+sstep(.62,.9,s)*.6)*255]});
  SALT=tex(240,180,(x,y)=>{const S=4.5,cx=Math.floor(x/S+N1(x/9,y/9)*.8),cy=Math.floor(y/S+N2(x/9,y/9)*.8),r=N3(cx*7.13,cy*3.71),fx=x/S-cx,fy=y/S-cy;
    const v=214+30*(r-.5)+12*fbm(N4,x/30,y/30,2)+(fx<.18||fy<.18?14:0)-(fx>.82||fy>.82?20:0);return[v*1.0,v*.99,v*.965]});
  SLATE=tex(400,400,(x,y)=>{const m=fbm(N1,x/80,y/80,3),s=N3(x/2,y/2);const v=46+10*m+4*s;return[v*.92,v*.98,v*1.05]});
  const jobs={speak:[drawSpeak,'day'],speakb:[drawSpeakB,'evening'],plans:[drawPlans,'evening'],log:[drawLog,'night'],board:[drawBoard,'evening']};
  for(const k in jobs){const cv=document.getElementById('c-'+k);if(!cv)continue;await jobs[k][0](cv.getContext('2d'),BGS[jobs[k][1]]);await new Promise(r=>setTimeout(r,0))}
  document.body.dataset.ready='1';if(window.sbDone)window.sbDone();
})();
