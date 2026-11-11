#include "Accounts.h"
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
namespace { constexpr uint32_t SESSION_MS=8UL*60*60*1000;constexpr unsigned HASH_ROUNDS=60000; }
String Accounts::randomHex(size_t bytes){String out;out.reserve(bytes*2);for(size_t i=0;i<bytes;i++){char b[3];snprintf(b,3,"%02x",(unsigned)(uint8_t)esp_random());out+=b;}return out;}
bool Accounts::equal(const String&a,const String&b){if(a.length()!=b.length())return false;uint8_t x=0;for(size_t i=0;i<a.length();i++)x|=a[i]^b[i];return x==0;}
String Accounts::passwordHash(const String&p,const String&s){
 mbedtls_md_context_t ctx;mbedtls_md_init(&ctx);uint8_t bytes[32];
 if(mbedtls_md_setup(&ctx,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1)!=0){mbedtls_md_free(&ctx);return "";}
 delay(1);int result=mbedtls_pkcs5_pbkdf2_hmac(&ctx,(const unsigned char*)p.c_str(),p.length(),(const unsigned char*)s.c_str(),s.length(),HASH_ROUNDS,32,bytes);mbedtls_md_free(&ctx);delay(1);
 if(result)return "";String out;out.reserve(64);for(uint8_t x:bytes){char b[3];snprintf(b,3,"%02x",(unsigned)x);out+=b;}return out;
}
void Accounts::error(int status,const char*code){JsonDocument d;d["error"]=code;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(status,"application/json; charset=utf-8",body);}
bool Accounts::parse(JsonDocument&doc){String body=web.arg("plain");if(body.length()>4096||deserializeJson(doc,body)){error(400,"INVALID_DATA");return false;}return true;}
bool Accounts::validEmail(String email){if(email.length()<3||email.length()>120||email.indexOf('@')<=0||email.lastIndexOf('@')!=email.indexOf('@')||email.endsWith("@"))return false;for(char c:email)if(c<=32||c==127)return false;return true;}
int Accounts::byId(const String&id){for(size_t i=0;i<users.size();i++)if(users[i].id==id)return i;return -1;}
int Accounts::byEmail(const String&mail){for(size_t i=0;i<users.size();i++)if(users[i].email==mail)return i;return -1;}
size_t Accounts::activeAdmins(){size_t count=0;for(auto&u:users)if(u.active&&u.role=="admin")count++;return count;}
void Accounts::publicUser(JsonObject d,const ProjectUser&u){d["id"]=u.id;d["email"]=u.email;d["name"]=u.name;d["role"]=u.role;d["active"]=u.active;d["mustChangePassword"]=u.mustChange;}
bool Accounts::persist(const std::vector<ProjectUser>&next){
 JsonDocument d;d["version"]=1;JsonArray a=d["users"].to<JsonArray>();for(auto&u:next){JsonObject o=a.add<JsonObject>();publicUser(o,u);o["salt"]=u.salt;o["hash"]=u.hash;}
 String value;serializeJson(d,value);if(value.length()>8000||db.putString("users",value)!=value.length()){error(507,"ACCOUNT_STORAGE_FAILED");return false;}return true;
}
bool Accounts::begin(){
 if(!db.begin("ralf-accounts",false))return false;
 String saved=db.getString("users","");
 if(saved.length()){
  JsonDocument d;if(deserializeJson(d,saved)||d["version"].as<int>()!=1||!d["users"].is<JsonArray>()||d["users"].size()>RalfPolicy::MAX_USERS)return false;
  for(JsonVariantConst o:d["users"].as<JsonArrayConst>()){
   ProjectUser u;u.id=o["id"].as<String>();u.email=o["email"].as<String>();u.name=o["name"].as<String>();u.role=o["role"].as<String>();u.salt=o["salt"].as<String>();u.hash=o["hash"].as<String>();u.active=o["active"]|true;u.mustChange=o["mustChangePassword"]|true;
   if(!validEmail(u.email)||u.id.length()!=32||u.salt.length()!=32||u.hash.length()!=64||(u.role!="admin"&&u.role!="member")||byId(u.id)>=0||byEmail(u.email)>=0)return false;
   users.push_back(u);
  }
  if(!activeAdmins())return false;
 }else{
  ProjectUser u;u.id=randomHex(16);u.name="Administrateur";u.email="admin@ralf.local";u.role="admin";u.salt=randomHex(16);String pass=randomHex(12);u.hash=passwordHash(pass,u.salt);if(u.hash.length()!=64)return false;
  std::vector<ProjectUser> next={u};if(!persist(next))return false;users=next;
  Serial.printf("INITIAL ADMIN EMAIL: %s\nINITIAL ADMIN PASSWORD: %s\nChange this password after login.\n",u.email.c_str(),pass.c_str());
 }
 available=true;Serial.printf("Accounts ready: %u members. USB recovery command: RESET_ADMIN\n",(unsigned)users.size());return true;
}
bool Accounts::origin(){String o=web.header("Origin");if(o.length()&&o!="http://"+web.hostHeader()){error(403,"ORIGIN");return false;}return true;}
int Accounts::current(bool allowChange){if(!available)return -1;String auth=web.header("Authorization");if(!auth.startsWith("Bearer "))return -1;String token=auth.substring(7);for(auto&s:sessions){if(RalfPolicy::sessionAlive(millis(),s.expires)&&equal(token,s.token)){int i=byId(s.userId);if(i>=0&&users[i].active&&(allowChange||!users[i].mustChange))return i;}}return -1;}
int Accounts::require(bool admin,bool allowChange){
 if(!available){error(503,"ACCOUNTS_UNAVAILABLE");return -1;}
 if(web.method()!=HTTP_GET&&!origin())return -1;
 int i=current(true);if(i<0){error(401,"LOGIN_REQUIRED");return -1;}
 if(!allowChange&&!RalfPolicy::mayEdit(users[i].active,users[i].mustChange)){error(403,"PASSWORD_CHANGE_REQUIRED");return -1;}
 if(admin&&!RalfPolicy::mayManage(users[i].active,users[i].mustChange,users[i].role=="admin")){error(403,"ADMIN_REQUIRED");return -1;}return i;
}
void Accounts::invalidate(const String&id){for(auto&s:sessions)if(s.userId==id){s.token="";s.expires=0;}}
String Accounts::newSession(const String&id){ProjectSession s;s.token=randomHex(32);s.userId=id;s.expires=millis()+SESSION_MS;for(auto&slot:sessions)if(!slot.token.length()||!RalfPolicy::sessionAlive(millis(),slot.expires)){slot=s;return s.token;}if(sessions.size()>=12)sessions.erase(sessions.begin());sessions.push_back(s);return s.token;}
void Accounts::login(){
 if(!available){error(503,"ACCOUNTS_UNAVAILABLE");return;}if(!origin())return;
 if(blockedUntil&&RalfPolicy::sessionAlive(millis(),blockedUntil)){error(429,"TRY_LATER");return;}
 JsonDocument input;if(!parse(input))return;String email=input["email"]|"",pass=input["password"]|"";email.trim();email.toLowerCase();
 if(!validEmail(email)||pass.length()>128){error(401,"INVALID_CREDENTIALS");return;}
 int i=byEmail(email);String salt=i>=0?users[i].salt:"00000000000000000000000000000000";String hash=passwordHash(pass,salt);
 if(i<0||!users[i].active||hash.length()!=64||!equal(hash,users[i].hash)){if(++failures>=5){blockedUntil=millis()+30000;failures=0;}error(401,"INVALID_CREDENTIALS");return;}
 failures=0;blockedUntil=0;JsonDocument d;d["token"]=newSession(users[i].id);publicUser(d["user"].to<JsonObject>(),users[i]);d["expiresInSeconds"]=SESSION_MS/1000;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);
}
void Accounts::logout(){int i=require(false,true);if(i<0)return;String token=web.header("Authorization").substring(7);for(auto&s:sessions)if(equal(s.token,token)){s.token="";s.expires=0;}web.send(200,"application/json","{\"ok\":true}");}
void Accounts::list(){if(require(true)<0)return;JsonDocument d;JsonArray a=d["users"].to<JsonArray>();for(auto&u:users)publicUser(a.add<JsonObject>(),u);d["limit"]=RalfPolicy::MAX_USERS;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);}
void Accounts::create(){
 if(require(true)<0)return;if(users.size()>=RalfPolicy::MAX_USERS){error(409,"USER_LIMIT");return;}JsonDocument input;if(!parse(input))return;
 ProjectUser u;u.email=input["email"]|"";u.email.trim();u.email.toLowerCase();u.name=input["name"]|"";u.name.trim();u.role=input["role"]|"member";
 if(!validEmail(u.email)||u.name.length()<1||u.name.length()>80||(u.role!="admin"&&u.role!="member")){error(400,"INVALID_DATA");return;}
 if(byEmail(u.email)>=0){error(409,"EMAIL_EXISTS");return;}u.id=randomHex(16);u.salt=randomHex(16);String pass=randomHex(12);u.hash=passwordHash(pass,u.salt);if(u.hash.length()!=64){error(500,"CRYPTO_FAILED");return;}
 auto next=users;next.push_back(u);if(!persist(next))return;users=next;JsonDocument d;publicUser(d["user"].to<JsonObject>(),u);d["temporaryPassword"]=pass;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(201,"application/json",body);
}
void Accounts::update(){
 if(require(true)<0)return;JsonDocument input;if(!parse(input))return;int i=byId(input["id"]|"");if(i<0){error(404,"USER_NOT_FOUND");return;}
 auto next=users;ProjectUser&u=next[i];u.email=input["email"]|u.email;u.email.trim();u.email.toLowerCase();u.name=input["name"]|u.name;u.name.trim();u.role=input["role"]|u.role;if(input["active"].is<bool>())u.active=input["active"];
 if(!validEmail(u.email)||!u.name.length()||u.name.length()>80||(u.role!="admin"&&u.role!="member")){error(400,"INVALID_DATA");return;}
 int duplicate=byEmail(u.email);if(duplicate>=0&&duplicate!=i){error(409,"EMAIL_EXISTS");return;}
 if(!RalfPolicy::adminChangeAllowed(activeAdmins(),users[i].active&&users[i].role=="admin",u.active&&u.role=="admin")){error(409,"LAST_ADMIN");return;}
 if(!persist(next))return;users=next;if(!users[i].active)invalidate(users[i].id);web.send(200,"application/json","{\"ok\":true}");
}
void Accounts::reset(){
 if(require(true)<0)return;JsonDocument input;if(!parse(input))return;int i=byId(input["id"]|"");if(i<0){error(404,"USER_NOT_FOUND");return;}
 auto next=users;String pass=randomHex(12);next[i].salt=randomHex(16);next[i].hash=passwordHash(pass,next[i].salt);next[i].mustChange=true;if(next[i].hash.length()!=64){error(500,"CRYPTO_FAILED");return;}
 if(!persist(next))return;users=next;invalidate(users[i].id);JsonDocument d;publicUser(d["user"].to<JsonObject>(),users[i]);d["temporaryPassword"]=pass;String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);
}
void Accounts::password(){
 int i=require(false,true);if(i<0)return;JsonDocument input;if(!parse(input))return;String currentPass=input["currentPassword"]|"",newPass=input["newPassword"]|"";
 if(currentPass.length()>128||newPass.length()<12||newPass.length()>128){error(400,"PASSWORD_LENGTH");return;}
 if(newPass==currentPass){error(400,"PASSWORD_REUSE");return;}
 if(!equal(passwordHash(currentPass,users[i].salt),users[i].hash)){error(401,"INVALID_CURRENT_PASSWORD");return;}
 auto next=users;next[i].salt=randomHex(16);next[i].hash=passwordHash(newPass,next[i].salt);next[i].mustChange=false;if(next[i].hash.length()!=64){error(500,"CRYPTO_FAILED");return;}
 if(!persist(next))return;users=next;invalidate(users[i].id);JsonDocument d;d["token"]=newSession(users[i].id);publicUser(d["user"].to<JsonObject>(),users[i]);String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);
}
void Accounts::profile(){
 int i=require();if(i<0)return;JsonDocument input;if(!parse(input))return;auto next=users;next[i].email=input["email"]|next[i].email;next[i].email.trim();next[i].email.toLowerCase();next[i].name=input["name"]|next[i].name;next[i].name.trim();
 if(!validEmail(next[i].email)||!next[i].name.length()||next[i].name.length()>80){error(400,"INVALID_DATA");return;}int dup=byEmail(next[i].email);if(dup>=0&&dup!=i){error(409,"EMAIL_EXISTS");return;}if(!persist(next))return;users=next;JsonDocument d;publicUser(d["user"].to<JsonObject>(),users[i]);String body;serializeJson(d,body);web.send(200,"application/json",body);
}
void Accounts::physicalAdminReset(){if(!available)return;for(size_t i=0;i<users.size();i++)if(users[i].active&&users[i].role=="admin"){auto next=users;String pass=randomHex(12);next[i].salt=randomHex(16);next[i].hash=passwordHash(pass,next[i].salt);next[i].mustChange=true;if(next[i].hash.length()!=64||!persist(next))return;users=next;invalidate(users[i].id);Serial.printf("ADMIN RESET EMAIL: %s\nTEMPORARY PASSWORD: %s\n",users[i].email.c_str(),pass.c_str());return;}}
void Accounts::routes(){
 web.on("/api/team",HTTP_GET,[this]{if(require()<0)return;JsonDocument d;JsonArray a=d["users"].to<JsonArray>();for(auto&u:users)if(u.active){JsonObject o=a.add<JsonObject>();o["id"]=u.id;o["name"]=u.name;o["email"]=u.email;}String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);});
 web.on("/api/login",HTTP_POST,[this]{login();});web.on("/api/logout",HTTP_POST,[this]{logout();});web.on("/api/me",HTTP_GET,[this]{int i=require(false,true);if(i<0)return;JsonDocument d;publicUser(d["user"].to<JsonObject>(),users[i]);String body;serializeJson(d,body);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",body);});
 web.on("/api/users",HTTP_GET,[this]{list();});web.on("/api/users",HTTP_POST,[this]{create();});web.on("/api/users",HTTP_PUT,[this]{update();});web.on("/api/reset-password",HTTP_POST,[this]{reset();});web.on("/api/password",HTTP_POST,[this]{password();});web.on("/api/profile",HTTP_POST,[this]{profile();});
}
