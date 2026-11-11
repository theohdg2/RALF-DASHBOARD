#include "Documents.h"
#include <esp_system.h>
String Documents::path(const String&id,const char*ext){return "/doc-"+id+ext;}
String Documents::randomId(){String s;for(int i=0;i<16;i++){char b[3];snprintf(b,3,"%02x",(unsigned)(uint8_t)esp_random());s+=b;}return s;}
bool Documents::metadata(const String&id,JsonDocument&doc){
 if(!RalfPolicy::validDocumentId(id.c_str()))return false;File f=fs.open(path(id,".json"),"r");if(!f||f.size()>1024)return false;auto err=deserializeJson(doc,f);f.close();if(err||doc["id"].as<String>()!=id||!doc["size"].is<size_t>()||!RalfPolicy::allowedDocumentName(doc["name"].as<String>().c_str()))return false;File data=fs.open(path(id,".bin"),"r");return data&&data.size()==doc["size"].as<size_t>();
}
size_t Documents::inventory(size_t&count){count=0;size_t total=0;File root=fs.open("/");for(File f=root.openNextFile();f;f=root.openNextFile()){String name=f.name();if(name.startsWith("/"))name=name.substring(1);f.close();if(name.startsWith("doc-")&&name.endsWith(".json")){String id=name.substring(4,name.length()-5);JsonDocument d;if(metadata(id,d)){count++;total+=d["size"].as<size_t>();}}yield();}return total;}
void Documents::list(){if(accounts.require()<0)return;JsonDocument d;JsonArray docs=d["documents"].to<JsonArray>();size_t total=0;File root=fs.open("/");for(File f=root.openNextFile();f;f=root.openNextFile()){String name=f.name();if(name.startsWith("/"))name=name.substring(1);f.close();if(name.startsWith("doc-")&&name.endsWith(".json")){JsonDocument item;if(metadata(name.substring(4,name.length()-5),item)){docs.add(item.as<JsonVariantConst>());total+=item["size"].as<size_t>();}}yield();}d["usedBytes"]=total;d["quotaBytes"]=RalfPolicy::DOCUMENT_QUOTA;d["maxFileBytes"]=RalfPolicy::MAX_DOCUMENT_BYTES;d["limit"]=RalfPolicy::MAX_DOCUMENTS;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json; charset=utf-8",body);}
void Documents::upload(){
 HTTPUpload &u=web.upload();
 if(u.status==UPLOAD_FILE_START){
  if(incoming)incoming.close();fs.remove("/document-upload.tmp");received=0;uploadError=0;started=true;finished=false;
  int index=accounts.current();if(index<0){uploadError=401;return;}String origin=web.header("Origin");if(origin.length()&&origin!="http://"+web.hostHeader()){uploadError=403;return;}
  uploadOwner=accounts.user(index).id;uploadId=randomId();uploadName=web.arg("name");uploadDate=web.arg("date");uploadCategory=web.arg("category");
  if(!RalfPolicy::allowedDocumentName(uploadName.c_str())||!RalfPolicy::validIsoDate(uploadDate.c_str())||(uploadCategory!="PC1"&&uploadCategory!="PC2"&&uploadCategory!="PC3"&&uploadCategory!="Rapport"&&uploadCategory!="Autre")){uploadError=400;return;}
  size_t count;size_t used=inventory(count);if(count>=RalfPolicy::MAX_DOCUMENTS||used>=RalfPolicy::DOCUMENT_QUOTA){uploadError=409;return;}
  incoming=fs.open("/document-upload.tmp","w");if(!incoming)uploadError=507;
 }else if(u.status==UPLOAD_FILE_WRITE){
  if(uploadError||!started)return;received+=u.currentSize;// Quota is checked at the end; free flash is checked for each chunk.
  if(received>RalfPolicy::MAX_DOCUMENT_BYTES){uploadError=413;incoming.close();return;}
  if(fs.totalBytes()-fs.usedBytes()<128*1024+u.currentSize||incoming.write(u.buf,u.currentSize)!=u.currentSize){uploadError=507;incoming.close();return;}
 }else if(u.status==UPLOAD_FILE_END){if(incoming){incoming.flush();incoming.close();}finished=true;}
 else if(u.status==UPLOAD_FILE_ABORTED){if(incoming)incoming.close();fs.remove("/document-upload.tmp");uploadError=400;started=false;}
}
void Documents::uploadDone(){
 if(accounts.require()<0){fs.remove("/document-upload.tmp");return;}
 if(!started||!finished||received==0){accounts.error(400,"UPLOAD_INCOMPLETE");fs.remove("/document-upload.tmp");return;}
 if(uploadError){fs.remove("/document-upload.tmp");accounts.error(uploadError,uploadError==413?"DOCUMENT_TOO_LARGE":uploadError==409?"DOCUMENT_LIMIT":uploadError==400?"INVALID_DOCUMENT":"DOCUMENT_STORAGE_FAILED");return;}
 size_t count;size_t used=inventory(count);if(used+received>RalfPolicy::DOCUMENT_QUOTA){fs.remove("/document-upload.tmp");accounts.error(409,"DOCUMENT_QUOTA");return;}
 if(!fs.rename("/document-upload.tmp",path(uploadId,".bin"))){accounts.error(507,"DOCUMENT_STORAGE_FAILED");return;}
 JsonDocument d;d["id"]=uploadId;d["name"]=uploadName;d["date"]=uploadDate;d["category"]=uploadCategory;d["size"]=received;d["ownerId"]=uploadOwner;
 String metadataJson;serializeJson(d,metadataJson);File meta=fs.open("/document-meta.tmp","w");bool saved=meta&&meta.write((const uint8_t*)metadataJson.c_str(),metadataJson.length())==metadataJson.length();if(meta){meta.flush();meta.close();}
 if(!saved||!fs.rename("/document-meta.tmp",path(uploadId,".json"))){fs.remove(path(uploadId,".bin"));accounts.error(507,"DOCUMENT_STORAGE_FAILED");return;}
 started=false;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(201,"application/json; charset=utf-8",body);
}
void Documents::download(){if(accounts.require()<0)return;String id=web.arg("id");JsonDocument d;if(!metadata(id,d)){accounts.error(404,"DOCUMENT_NOT_FOUND");return;}File f=fs.open(path(id,".bin"),"r");String name=d["name"].as<String>(); // ASCII fallback + RFC 5987 filename for accents.
 String encoded;const char*hex="0123456789ABCDEF";for(unsigned char c:name){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_')encoded+=(char)c;else{encoded+='%';encoded+=hex[c>>4];encoded+=hex[c&15];}}
 web.sendHeader("Content-Disposition","attachment; filename=\"document\"; filename*=UTF-8''"+encoded);web.sendHeader("Cache-Control","no-store");web.sendHeader("X-Content-Type-Options","nosniff");web.streamFile(f,"application/octet-stream");f.close();}
void Documents::remove(){int i=accounts.require(true);if(i<0)return;String id=web.arg("id");JsonDocument d;if(!metadata(id,d)){accounts.error(404,"DOCUMENT_NOT_FOUND");return;}if(!fs.remove(path(id,".json"))){accounts.error(507,"DOCUMENT_STORAGE_FAILED");return;}fs.remove(path(id,".bin"));web.send(200,"application/json","{\"ok\":true}");}
void Documents::routes(){web.on("/api/documents",HTTP_GET,[this]{list();});web.on("/api/documents",HTTP_POST,[this]{uploadDone();},[this]{upload();});web.on("/api/document",HTTP_GET,[this]{download();});web.on("/api/document",HTTP_DELETE,[this]{remove();});}

void Documents::begin(){
 fs.remove("/document-upload.tmp");fs.remove("/document-meta.tmp");
 std::vector<String> unfinished;
 File root=fs.open("/");for(File f=root.openNextFile();f;f=root.openNextFile()){String name=f.name();if(name.startsWith("/"))name=name.substring(1);f.close();if(name.startsWith("doc-")&&name.endsWith(".bin")){String id=name.substring(4,name.length()-4);if(RalfPolicy::validDocumentId(id.c_str())&&!fs.exists(path(id,".json")))unfinished.push_back(path(id,".bin"));}}
 for(const String&p:unfinished)fs.remove(p);
}
