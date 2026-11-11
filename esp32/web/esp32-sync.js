/* Browser-native fetch is bound to its global context. Only the session token is retained when requested. Passwords are never stored. */
(function(root){
'use strict';
class Esp32Store {
 constructor(config={},request=fetch){this.config=config;this.request=request.bind(globalThis);this.sha=null;this.token='';this.user=null;this.remember=true;this.sessionExpires=0;this.storageKey='ralf-esp32-session-v1';}
 storage(name){try{return root[name]}catch{return null}}
 clearStoredSession(){for(const name of ['localStorage','sessionStorage'])try{this.storage(name)?.removeItem(this.storageKey)}catch{}}
 clearSession(){this.clearStoredSession();this.token='';this.user=null;this.sessionExpires=0;}
 persistSession(){this.clearStoredSession();if(!this.token)return;try{this.storage(this.remember?'localStorage':'sessionStorage')?.setItem(this.storageKey,JSON.stringify({token:this.token,expiresAt:this.sessionExpires}));}catch{}}
 async restoreSession(){
  for(const name of ['localStorage','sessionStorage']){
   let saved;try{saved=JSON.parse(this.storage(name)?.getItem(this.storageKey)||'null')}catch{}
   if(!saved)continue;
   if(!/^[a-f0-9]{64}$/.test(saved.token)||!Number.isFinite(saved.expiresAt)||saved.expiresAt<=Date.now()){try{this.storage(name)?.removeItem(this.storageKey)}catch{}continue;}
   this.token=saved.token;this.sessionExpires=saved.expiresAt;this.remember=true;this.persistSession();
   try{await this.refreshUser();return true}catch{return false;}
  }return false;
 }
 setToken(token){this.token=token.trim();}
 async response(path,options={},timeout=45000){
  if(!/^\/api\/[a-z]+(?:-[a-z]+)?(?:\?[^#]*)?$/.test(path))throw Error('INVALID_ENDPOINT');
  const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),timeout);
  try{const response=await this.request(path,{...options,cache:'no-store',credentials:'same-origin',signal:controller.signal,headers:{Accept:'application/json',...(this.token?{Authorization:`Bearer ${this.token}`}:{ }),...options.headers}});
   if(!response.ok){let detail={};try{detail=await response.json()}catch{}const names={400:'INVALID_DATA',401:'LOGIN_REQUIRED',403:'FORBIDDEN',409:'CONFLICT',413:'PROJECT_TOO_LARGE',428:'REVISION_REQUIRED',429:'TRY_LATER',503:'STORAGE_UNAVAILABLE',507:'STORAGE_WRITE_FAILED'};const error=Error(typeof detail.error==='string'?detail.error:(names[response.status]||`HTTP_${response.status}`));error.status=response.status;if(response.status===401&&!['INVALID_CREDENTIALS','INVALID_CURRENT_PASSWORD'].includes(error.message)){this.clearSession();}throw error;}return response;
  }finally{clearTimeout(timer);}
 }
 async api(path,options={}){return (await this.response(path,options)).json();}
 async json(path,method,body){return this.api(path,{method,headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}
 acceptSession(result){if(!result||typeof result.token!=='string'||!/^[a-f0-9]{64}$/.test(result.token)||!result.user?.id)throw Error('INVALID_RESPONSE');this.token=result.token;this.user=result.user;this.sessionExpires=Date.now()+Math.min(Number(result.expiresInSeconds)||28800,28800)*1000;this.persistSession();return result.user;}
 async login(email,password,remember=true){const result=await this.json('/api/login','POST',{email,password});this.remember=remember;return this.acceptSession(result);}
 async changePassword(currentPassword,newPassword){return this.acceptSession(await this.json('/api/password','POST',{currentPassword,newPassword}));}
 async logout(){try{if(this.token)await this.json('/api/logout','POST',{})}finally{this.clearSession();}}
 async refreshUser(){const result=await this.api('/api/me');this.user=result.user;return this.user;}
 async read(){const result=await this.api('/api/project');if(!result||typeof result.sha!=='string'||!/^[a-f0-9]{64}$/.test(result.sha)||!result.data)throw Error('INVALID_DATA');return result;}
 accept(result){this.sha=result.sha;return result.data;}
 async load(){return this.accept(await this.read());}
 async write(data){if(!this.token)throw Error('LOGIN_REQUIRED');if(!this.sha)throw Error('NOT_LOADED');const body=JSON.stringify(data);if(new TextEncoder().encode(body).length>262144)throw Error('PROJECT_TOO_LARGE');const response=await this.api('/api/project',{method:'PUT',headers:{'Content-Type':'application/json','If-Match':`"${this.sha}"`},body});if(typeof response.sha!=='string'||!/^[a-f0-9]{64}$/.test(response.sha))throw Error('INVALID_RESPONSE');this.sha=response.sha;return response.sha;}
 async uploadDocument(file,date,category){if(file.size>1024*1024)throw Error('DOCUMENT_TOO_LARGE');const query=new URLSearchParams({name:file.name,date,category});const form=new FormData();form.append('file',file,file.name);return (await this.response('/api/documents?'+query,{method:'POST',body:form},120000)).json();}
 async downloadDocument(id){return (await this.response('/api/document?id='+encodeURIComponent(id),{},120000)).blob();}
}
root.RalfEsp32Store=Esp32Store;if(typeof module!=='undefined')module.exports=Esp32Store;
})(typeof window!=='undefined'?window:globalThis);
