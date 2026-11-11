const {test}=require('node:test'),assert=require('node:assert/strict');
const GithubStore=require('../sync.js'),Esp32Store=require('../esp32/web/esp32-sync.js');
for(const [name,Store,path,config] of [['GitHub',GithubStore,'https://api.github.com/repos/team/robot',{owner:'team',repo:'robot',branch:'main',path:'data/project.json'}],['ESP32',Esp32Store,'/api/status',{}]]){
 test(name+' : conserve le contexte global requis par fetch dans le navigateur',async()=>{
  let invoked=false;
  function nativeLikeFetch(){'use strict';if(this!==globalThis)throw new TypeError('Illegal invocation');invoked=true;return Promise.resolve({ok:true,json:async()=>({ok:true})});}
  const store=new Store(config,nativeLikeFetch);
  assert.equal((await store.api(path)).ok,true);assert.equal(invoked,true);
 });
}
