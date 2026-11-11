/* ESP32 local transport. Editor keys stay in memory and use same-origin API only. */
(function(root){
'use strict';
class Esp32Store {
 constructor(config={},request=fetch){this.config=config;this.request=request;this.sha=null;this.token='';}
 setToken(token){this.token=token.trim();}
 async api(path,options={}){
  if(!/^\/api\/[a-z]+$/.test(path))throw Error('INVALID_ENDPOINT');
  const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),15000);
  try{
   const response=await this.request(path,{...options,cache:'no-store',credentials:'same-origin',signal:controller.signal,headers:{Accept:'application/json',...(this.token?{Authorization:`Bearer ${this.token}`}:{ }),...options.headers}});
   if(!response.ok){const names={400:'INVALID_DATA',401:'AUTH',403:'ORIGIN',409:'CONFLICT',413:'PROJECT_TOO_LARGE',428:'REVISION_REQUIRED',503:'STORAGE_UNAVAILABLE',507:'STORAGE_WRITE_FAILED'};const error=Error(names[response.status]||`HTTP_${response.status}`);error.status=response.status;throw error;}
   return await response.json();
  }finally{clearTimeout(timer);}
 }
 async authenticate(){if(!this.token)throw Error('AUTH');const result=await this.api('/api/session',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});if(result.authenticated!==true)throw Error('INVALID_RESPONSE');return result;}
 async read(){const result=await this.api('/api/project');if(!result||typeof result.sha!=='string'||!/^[a-f0-9]{64}$/.test(result.sha)||!result.data)throw Error('INVALID_DATA');return result;}
 accept(result){this.sha=result.sha;return result.data;}
 async load(){return this.accept(await this.read());}
 async write(data){
  if(!this.token)throw Error('AUTH');if(!this.sha)throw Error('NOT_LOADED');
  const body=JSON.stringify(data);if(new TextEncoder().encode(body).length>65536)throw Error('PROJECT_TOO_LARGE');
  const response=await this.api('/api/project',{method:'PUT',headers:{'Content-Type':'application/json','If-Match':`"${this.sha}"`},body});
  if(typeof response.sha!=='string'||!/^[a-f0-9]{64}$/.test(response.sha))throw Error('INVALID_RESPONSE');
  this.sha=response.sha;return response.sha;
 }
}
root.RalfEsp32Store=Esp32Store;
if(typeof module!=='undefined')module.exports=Esp32Store;
})(typeof window!=='undefined'?window:globalThis);
