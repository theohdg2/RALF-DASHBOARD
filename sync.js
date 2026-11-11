/* Shared persistence over GitHub Contents API. No credentials are stored. */
(function(root){
'use strict';
class GithubStore {
 constructor(config,request=fetch){this.config=config;this.request=request.bind(globalThis);this.sha=null;this.token='';}
 setToken(token){this.token=token.trim();}
 endpoint(){const c=this.config;return `https://api.github.com/repos/${encodeURIComponent(c.owner)}/${encodeURIComponent(c.repo)}/contents/${c.path.split('/').map(encodeURIComponent).join('/')}`;}
 async api(url,options={}){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),20000);try{const response=await this.request(url,{...options,signal:controller.signal,cache:'no-store',headers:{Accept:'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28',...(this.token?{Authorization:`Bearer ${this.token}`}:{ }),...options.headers}});if(!response.ok){const error=new Error(response.status===409||response.status===422?'CONFLICT':response.status===401||response.status===403?'AUTH':response.status===404?'NOT_FOUND':`HTTP_${response.status}`);error.status=response.status;throw error;}return await response.json();}finally{clearTimeout(timer);}}
 async read(){const file=await this.api(`${this.endpoint()}?ref=${encodeURIComponent(this.config.branch)}`);if(file.encoding!=='base64'||typeof file.sha!=='string')throw Error('INVALID_DATA');const raw=atob(file.content.replace(/\s/g,'')),bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));const data=JSON.parse(new TextDecoder().decode(bytes));return {data,sha:file.sha};}
 accept(result){this.sha=result.sha;return result.data;}
 async load(){return this.accept(await this.read());}
 async write(data){if(!this.token)throw Error('AUTH');if(!this.sha)throw Error('NOT_LOADED');const expected=this.sha;const latest=await this.read();if(latest.sha!==expected)throw Error('CONFLICT');const bytes=new TextEncoder().encode(JSON.stringify(data,null,2)+'\n');let binary='';for(const b of bytes)binary+=String.fromCharCode(b);const result=await this.api(this.endpoint(),{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:'dashboard: enregistrer le suivi RALF',branch:this.config.branch,sha:expected,content:btoa(binary)})});if(!result.content?.sha)throw Error('INVALID_RESPONSE');this.sha=result.content.sha;return this.sha;}
}
root.RalfGithubStore=GithubStore;
if(typeof module!=='undefined')module.exports=GithubStore;
})(typeof window!=='undefined'?window:globalThis);
