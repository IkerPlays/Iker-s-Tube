// Smoke test of the website with a mocked GitHub API (needs Playwright and a Chromium; set CHROME to its path).
// Run: CHROME=/path/to/chrome node tools/web_smoke.js  -- prints the console errors, the focus behaviour of the filter chips and the translated aria labels.
const { chromium } = require('playwright');
const http=require('http'),fs=require('fs'),path=require('path');
const root=path.join(__dirname,'..','web');
const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.json':'application/json','.jpg':'image/jpeg','.png':'image/png'};
const csp="default-src 'self'; img-src 'self' data: blob:; style-src 'self'; script-src 'self'; connect-src https://api.github.com; base-uri 'self'; form-action 'none'; frame-ancestors 'none'";
const srv=http.createServer((q,r)=>{let p=q.url.split('?')[0];if(p=='/')p='/index.html';const f=path.join(root,p);fs.readFile(f,(e,d)=>{if(e){r.writeHead(404);r.end('no');return;}r.writeHead(200,{'content-type':types[path.extname(f)]||'application/octet-stream','content-security-policy':csp});r.end(d);});}).listen(8097);
const rel=(tag,pre)=>({tag_name:tag,name:tag,prerelease:pre,draft:false,published_at:'2026-10-20T00:00:00Z',html_url:'https://github.com/x/'+tag,body:'### H\n- a',assets:[{name:'IkersTube.cia',size:1234567,download_count:5,browser_download_url:'https://github.com/x/a.cia'}]});
(async()=>{
 const b=await chromium.launch({executablePath:process.env.CHROME,args:['--no-sandbox']});
 const pg=await b.newPage({viewport:{width:360,height:800}});const errs=[];
 pg.on('pageerror',e=>errs.push(e.message));pg.on('console',m=>{if(m.type()=='error')errs.push(m.text());});
 await pg.route('https://api.github.com/**',r=>r.fulfill({json:[rel('v1.2.1',false),rel('v1.2.1-dev.3',true)]}));
 await pg.goto('http://localhost:8097/#v1.2.1');await pg.waitForTimeout(1200);
 await pg.focus('#rel-filter button:nth-child(2)');await pg.keyboard.press('Enter');
 const act=await pg.evaluate(()=>document.activeElement.textContent);
 await pg.focus('#filters button:nth-child(2)');await pg.keyboard.press('Enter');
 const act2=await pg.evaluate(()=>document.activeElement.closest('#filters')!==null);
 console.log('focus kept release chip:',act,'| roadmap chip kept:',act2,'| errors',JSON.stringify(errs));
 await pg.click('#lang');await pg.waitForTimeout(300);
 console.log('aria es:',await pg.getAttribute('#rel-search','aria-label'),await pg.getAttribute('nav','aria-label'));
 await b.close();srv.close();
})();
