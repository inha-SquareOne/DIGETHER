// 개발 웹 화면(HTML 한 장, 외부 파일·CDN 없음), DigetherDevWebServer.cpp에서만 include
// MSVC 문자열 상수 한 조각 16KB 제한으로 조각 배열, 조각마다 8KB 안쪽 유지(한글은 3바이트)
static const char* const DigetherDevWebPageParts[] = {
R"DGH(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DIGETHER 개발 치트</title>
<style>
:root{--bg:#f5f5f2;--panel:#ffffff;--ink:#1d1f22;--mute:#6b6f76;--line:#dcdcd6;--accent:#c2410c;--ok:#15803d;--bad:#b91c1c;--chip:#eef0ec}
@media (prefers-color-scheme: dark){:root{--bg:#16181b;--panel:#1f2226;--ink:#e8e8e4;--mute:#9a9ea5;--line:#33373d;--accent:#fb923c;--ok:#4ade80;--bad:#f87171;--chip:#2a2e33}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.5 "Malgun Gothic","Apple SD Gothic Neo",system-ui,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--panel);border-bottom:1px solid var(--line);padding:10px 16px}
h1{font-size:16px;margin:0 8px 0 0;display:inline}
.band{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.chip{background:var(--chip);border-radius:6px;padding:2px 8px;font-size:13px;white-space:nowrap}
.chip b{font-weight:600}
.warn{background:var(--accent);color:#fff}
.notice{margin:8px 16px 0;padding:8px 12px;border-radius:8px;border:1px solid var(--accent);display:none}
main{display:grid;grid-template-columns:minmax(0,1fr) 320px;gap:16px;padding:16px}
@media (max-width:900px){main{grid-template-columns:minmax(0,1fr)}}
nav{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:12px}
nav button{border:1px solid var(--line);background:var(--panel);color:var(--ink);border-radius:999px;padding:4px 12px;cursor:pointer;font:inherit}
nav button.on{background:var(--ink);color:var(--bg);border-color:var(--ink)}
#search{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:var(--panel);color:var(--ink);font:inherit;margin-bottom:10px}
.cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px 12px;display:flex;flex-direction:column;gap:6px}
.card.off{opacity:.55}
.card .top{display:flex;justify-content:space-between;gap:8px;align-items:baseline}
.card code{font:12px/1.4 Consolas,monospace;color:var(--mute);word-break:break-all}
.ids{font-size:11px;color:var(--mute);white-space:nowrap}
.why{font-size:12px;color:var(--bad)}
.params{display:flex;flex-wrap:wrap;gap:6px}
.params label{display:flex;flex-direction:column;font-size:12px;color:var(--mute);min-width:90px;flex:1}
.params input,.params select{font:inherit;padding:5px 6px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--ink)}
.run{align-self:flex-start;border:0;border-radius:6px;padding:6px 14px;background:var(--accent);color:#fff;font:inherit;cursor:pointer}
.run:disabled{background:var(--mute);cursor:not-allowed}
aside section{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px 12px;margin-bottom:12px}
aside h2{font-size:14px;margin:0 0 6px}
table{width:100%;border-collapse:collapse;font-size:13px}
td,th{padding:3px 4px;border-bottom:1px solid var(--line);text-align:left}
.toggle{display:flex;justify-content:space-between;align-items:center;padding:3px 0;font-size:13px}
#log{max-height:320px;overflow:auto;font-size:12px}
#log div{padding:4px 0;border-bottom:1px solid var(--line)}
#log .ok{color:var(--ok)}#log .bad{color:var(--bad)}
#log button{font-size:11px;margin-left:6px;cursor:pointer;background:none;border:1px solid var(--line);border-radius:4px;color:var(--mute)}
.preset{display:block;width:100%;text-align:left;margin:4px 0;padding:6px 8px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--ink);font:inherit;cursor:pointer}
footer{padding:0 16px 16px;color:var(--mute);font-size:12px}
</style>
</head>
)DGH",
R"DGH(<body>
<header>
<h1>DIGETHER 개발 치트</h1><span class="chip" id="conn">연결 중…</span>
<div class="band" id="band"></div>
</header>
<div class="notice" id="notice"></div>
<main>
<div>
<nav id="tabs"></nav>
<input id="search" placeholder="명령 이름·설명·요청 ID 검색 (예: A-01, 잔고, 몬스터)">
<div class="cards" id="cards"></div>
</div>
<aside>
<section><h2>참가자</h2><table id="players"><tbody></tbody></table></section>
<section><h2>화면 표시</h2><div id="panels"></div></section>
<section><h2>자주 쓰는 묶음</h2><div id="presets"></div></section>
<section><h2>실행 기록</h2><div id="log"></div></section>
</aside>
</main>
<footer>개발 빌드 전용 화면입니다. '호스트' 명령은 호스트 게임에서 실행하면 모든 참가자에게 반영됩니다. 명령 문서: <a href="/commands.md" id="mdlink">commands.md</a></footer>
<script>
const H={'X-Digether-Dev':'1'};
let commands=[],state=null,tab=null;
// 묶음: 존재하는 명령만 표시, 순서대로 실행
const PRESETS=[
 {name:'5일차 실패 직전 (4일차·목표 미달)',steps:[['DigetherSetDay','4','4'],['DigetherSetQuotaProgress','0']]},
 {name:'할당량 달성 직후 확인',steps:[['DigetherCompleteQuota']]},
 {name:'시간 종료 10초 전',steps:[['DigetherSetRunTime','10']]},
 {name:'몬스터 없이 시험',steps:[['DigetherMonsterSpawning','0'],['DigetherMonsterClear']]},
 {name:'전원 잔고 1000',steps:[['DigetherSetBalance','1000','all']]}
];
function el(t,a,...c){const e=document.createElement(t);if(a)for(const[k,v]of Object.entries(a)){if(k==='class')e.className=v;else if(k.startsWith('on'))e.addEventListener(k.slice(2),v);else if(v!==undefined&&v!==null)e.setAttribute(k,v)}for(const x of c)if(x!=null)e.append(x);return e}
async function api(path,body){const o={headers:{...H}};if(body){o.method='POST';o.headers['Content-Type']='application/json';o.body=JSON.stringify(body)}const r=await fetch(path,o);if(!r.ok)throw new Error(r.status+' '+(await r.text()));return r.json()}
function log(name,args,res){const line=[name,...args].join(' ');const d=el('div',null,el('span',{class:res.ok?'ok':'bad'},res.ok?'성공 ':'거절 '),el('b',null,line),el('br'),res.message,el('button',{onclick:()=>navigator.clipboard&&navigator.clipboard.writeText(line)},'콘솔 명령 복사'));const L=document.getElementById('log');L.prepend(d);while(L.children.length>60)L.lastChild.remove()}
async function run(name,args,destructive){if(destructive&&!confirm(name+'\n되돌리기 어려운 명령입니다(저장 슬롯이 바뀔 수 있음). 실행할까요?'))return null;try{const r=await api('/api/run',{name,args});log(name,args,r);refresh();return r}catch(e){log(name,args,{ok:false,message:String(e)});return {ok:false}}}
)DGH",
R"DGH(function playerOptions(){const o=['self','all'];if(state&&state.players)for(const p of state.players)o.push(String(p.id));return o}
function input(p){const typ=p.type;if(typ==='enum'||typ==='bool'||typ==='player'){const opts=typ==='bool'?['1','0']:typ==='player'?playerOptions():p.options;const s=el('select');if(p.optional)s.append(el('option',{value:''},'(생략)'));for(const o of opts){const label=typ==='bool'?(o==='1'?'켜기':'끄기'):o;s.append(el('option',{value:o},label))}if(p.default)s.value=p.default;return s}
const i=el('input',{type:(typ==='int'||typ==='float')?'number':'text',placeholder:p.hint||(p.optional?'생략 가능':''),step:typ==='float'?'any':'1'});if(p.min!==undefined)i.min=p.min;if(p.max!==undefined)i.max=p.max;if(p.default)i.value=p.default;return i}
function card(c){const fields=c.params.map(p=>({p,i:input(p)}));const off=!!c.unavailable;const blocked=c.scope==='host'&&state&&state.hasWorld&&!state.host;
const btn=el('button',{class:'run',onclick:async()=>{const args=[];for(const f of fields){const v=String(f.i.value).trim();if(v===''&&f.p.optional)break;args.push(v)}await run(c.name,args,c.destructive)}},'실행');if(off||blocked)btn.disabled=true;
return el('div',{class:'card'+(off?' off':'')},el('div',{class:'top'},el('b',null,c.summary),el('span',{class:'ids'},c.requestIds+(c.scope==='host'?' · 호스트':' · 각자'))),el('code',null,c.usage),off?el('div',{class:'why'},'아직 사용 불가: '+c.unavailable):null,blocked?el('div',{class:'why'},'참가자 게임에 연결돼 있습니다. 호스트 PC에서 실행하세요.'):null,fields.length?el('div',{class:'params'},...fields.map(f=>el('label',null,f.p.name+(f.p.optional?' (선택)':''),f.i))):null,btn)}
function render(){const cats=[...new Set(commands.map(c=>c.category))];if(!tab||!cats.includes(tab))tab=cats[0];const T=document.getElementById('tabs');T.replaceChildren(...cats.map(c=>el('button',{class:c===tab?'on':'',onclick:()=>{tab=c;render()}},c)));
const q=document.getElementById('search').value.trim().toLowerCase();const list=commands.filter(c=>q?(c.name+c.summary+c.requestIds+c.category).toLowerCase().includes(q):c.category===tab);document.getElementById('cards').replaceChildren(...list.map(card));
const P=document.getElementById('presets');const names=new Set(commands.filter(c=>!c.unavailable).map(c=>c.name));P.replaceChildren(...PRESETS.filter(p=>p.steps.every(s=>names.has(s[0]))).map(p=>el('button',{class:'preset',onclick:async()=>{for(const s of p.steps){const r=await run(s[0],s.slice(1),false);if(!r||!r.ok)break}}},p.name)));if(!P.children.length)P.append('사용 가능한 묶음이 없습니다.')}
function fmtTime(s){if(s<0)return '-';const m=Math.floor(s/60);return m+':'+String(Math.floor(s%60)).padStart(2,'0')}
function renderState(){const s=state;const B=document.getElementById('band');const chips=[];const c=(k,v,w)=>chips.push(el('span',{class:'chip'+(w?' warn':'')},k+' ',el('b',null,v)));
c('빌드',s.build||'-');c('맵',s.map||'-');c('역할',s.netMode||'-');if(s.gameplay){c('일차',s.totalDay+' / 기간 '+s.periodDay);c('목표',s.deposited>=0?s.deposited+' / '+s.target:String(s.target));c('남은 시간',fmtTime(s.remainingSeconds));c('엘리베이터',s.elevatorPhase+' · 상승 '+s.ascents);c('몬스터',s.monster.director?(s.monster.alive+'마리 · 타이머 '+s.monster.timerSeconds.toFixed(0)+'초 · 확률 '+(s.monster.chance*100).toFixed(1)+'%'):'호스트 전용');if(s.paymentDeadline)c('마감','4일차',true);if(s.gameOver)c('게임 오버','진행 중',true)}if(s.cheatUsed)c('치트','사용됨 · 자동 저장에 남습니다',true);B.replaceChildren(...chips);
const N=document.getElementById('notice');if(!s.hasWorld){N.style.display='block';N.textContent='게임 월드가 없습니다. 게임(또는 PIE)을 시작하세요.'}else if(!s.host){N.style.display='block';N.textContent='이 화면은 참가자 게임입니다. 판정을 바꾸는 명령은 호스트 PC의 화면에서 실행하세요.'}else if(!s.gameplay){N.style.display='block';N.textContent='게임 맵(거점·갱도)이 아닙니다. 메뉴·대기실에서는 대부분의 명령을 쓸 수 없습니다.'}else N.style.display='none';
const TB=document.querySelector('#players tbody');TB.replaceChildren(el('tr',null,el('th',null,'번호'),el('th',null,'이름'),el('th',null,'생존'),el('th',null,'잔고')),...(s.players||[]).map(p=>el('tr',null,el('td',null,String(p.id)),el('td',null,p.name+(p.self?' (나)':'')),el('td',null,p.life),el('td',null,p.balance>=0?String(p.balance):'비공개'))));
const PN=document.getElementById('panels');PN.replaceChildren(...(s.panels||[]).map(p=>{const b=el('input',{type:'checkbox'});b.checked=p.on;b.addEventListener('change',()=>run('DigetherShow',[p.id,b.checked?'1':'0'],false));return el('label',{class:'toggle'},p.id,b)}));if(!PN.children.length)PN.append('표시 항목이 없습니다.')}
async function refresh(){try{state=await api('/api/state');document.getElementById('conn').textContent='연결됨 '+(state.url||'');renderState()}catch(e){document.getElementById('conn').textContent='게임과 연결 끊김';}}
async function init(){try{commands=await api('/api/commands')}catch(e){document.getElementById('conn').textContent='명령 목록을 받지 못했습니다: '+e;return}await refresh();render();setInterval(refresh,1000)}
document.getElementById('search').addEventListener('input',render);
init();
</script>
</body>
</html>
)DGH"
};
