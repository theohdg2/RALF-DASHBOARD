#pragma once
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <cmath>
#include "AuthPolicy.h"
namespace RalfWorkbench {
inline bool text(JsonVariantConst v,size_t max=16000){return v.is<const char*>()&&strlen(v.as<const char*>())<=max;}
inline bool num(JsonVariantConst v,double min,double max){return v.is<double>()&&std::isfinite(v.as<double>())&&v.as<double>()>=min&&v.as<double>()<=max;}
inline bool one(JsonVariantConst v,std::initializer_list<const char*> values){if(!v.is<const char*>())return false;for(auto x:values)if(strcmp(v.as<const char*>(),x)==0)return true;return false;}
inline bool date(JsonVariantConst v){return v.is<const char*>()&&RalfPolicy::validIsoDate(v.as<const char*>());}
inline bool ids(JsonVariantConst v,size_t max=24){if(!v.is<JsonArrayConst>()||v.size()>max)return false;std::vector<std::string> seen;for(auto id:v.as<JsonArrayConst>()){if(!text(id,100))return false;std::string s=id.as<const char*>();for(auto &x:seen)if(x==s)return false;seen.push_back(s);}return true;}
inline bool criteria(JsonVariantConst v){if(!v.is<JsonArrayConst>()||v.size()>5)return false;std::vector<std::string> seen;for(auto c:v.as<JsonArrayConst>()){if(!one(c["metric"],{"openingErrorCm","closingErrorCm","stopErrorCm","durationS","releasedMassG"})||!one(c["operator"],{"absMax","min","max"})||!num(c["value"],0,10000))return false;std::string key=c["metric"].as<const char*>();for(auto &x:seen)if(x==key)return false;seen.push_back(key);}return true;}
inline bool robotConfig(JsonVariantConst c){for(auto k:{"before","during","after","drop","contrast"})if(!num(c[k],0,100)||c[k].as<double>()<=0)return false;for(auto k:{"start","end","stop"})if(!num(c[k],0,320))return false;return c["stop"].as<double>()>0&&c["end"].as<double>()-c["stop"].as<double>()>2&&c["start"].as<double>()-c["end"].as<double>()>2&&one(c["mode"],{"CONTRAST_OR_DISTANCE","DISTANCE_ONLY"});}
inline bool dependencies(JsonArrayConst tasks){
 std::vector<std::string> keys;std::vector<JsonVariantConst> entries;size_t i=0;
 for(auto t:tasks){std::string key=t["id"].is<const char*>()?t["id"].as<const char*>():"legacy-task-"+std::to_string(i);for(auto &x:keys)if(x==key)return false;keys.push_back(key);entries.push_back(t);i++;}
 std::vector<std::vector<size_t>> edges(keys.size());
 for(size_t n=0;n<entries.size();n++){auto deps=entries[n]["dependencies"];if(deps.isNull())continue;if(!ids(deps))return false;for(auto id:deps.as<JsonArrayConst>()){size_t target=0;while(target<keys.size()&&keys[target]!=id.as<const char*>())target++;if(target==keys.size()||target==n)return false;edges[n].push_back(target);}}
 std::vector<unsigned char> color(keys.size(),0);for(size_t n=0;n<keys.size();n++){if(color[n])continue;std::vector<std::pair<size_t,size_t>> stack={{n,0}};color[n]=1;while(!stack.empty()){auto &top=stack.back();if(top.second==edges[top.first].size()){color[top.first]=2;stack.pop_back();continue;}size_t child=edges[top.first][top.second++];if(color[child]==1)return false;if(!color[child]){color[child]=1;stack.push_back({child,0});}}}return true;
}
inline bool valid(JsonVariantConst project){
 if(!dependencies(project["tasks"].as<JsonArrayConst>()))return false;
 for(auto t:project["trials"].as<JsonArrayConst>()){
  for(auto key:{"campaignId","robotVersionId"})if(!t[key].isNull()&&!text(t[key],100))return false;
  auto c=t["conditions"];if(!c.isNull()){if(!c.is<JsonObjectConst>())return false;for(auto key:{"light","cable","notes"})if(!c[key].isNull()&&!text(c[key],8000))return false;if(!c["slopePct"].isNull()&&!num(c["slopePct"],0,100))return false;if(!c["loadG"].isNull()&&!num(c["loadG"],0,10000))return false;if(!c["batteryV"].isNull()&&!num(c["batteryV"],0,100))return false;}
  auto m=t["measurements"];if(!m.isNull()){if(!m.is<JsonObjectConst>())return false;for(JsonPairConst pair:m.as<JsonObjectConst>()){const char*key=pair.key().c_str();bool sign=!strcmp(key,"openingErrorCm")||!strcmp(key,"closingErrorCm")||!strcmp(key,"stopErrorCm");if(!sign&&strcmp(key,"durationS")&&strcmp(key,"releasedMassG"))return false;if(!pair.value().isNull()&&!num(pair.value(),sign?-320:0,sign?320:10000))return false;}}
  if(!t["criteria"].isNull()&&!criteria(t["criteria"]))return false;
  auto version=t["robotVersionSnapshot"];if(!version.isNull()){if(!version.is<JsonObjectConst>()||!text(version["name"],400))return false;for(auto key:{"mechanical","electronics","software"})if(!text(version[key],8000))return false;}
  auto pre=t["preflight"];if(!pre.isNull()){if(!pre.is<JsonObjectConst>()||!text(pre["at"],100))return false;for(auto key:{"battery","fixation","trapdoor","sensors","area"})if(!pre[key].is<bool>())return false;}
 }
 auto w=project["workbench"];if(w.isNull())return true;if(!w.is<JsonObjectConst>())return false;
 for(auto key:{"inventory","decisions","unknowns","versions","campaigns"}){if(!w[key].is<JsonArrayConst>()||w[key].size()>50)return false;std::vector<std::string> seen;for(auto item:w[key].as<JsonArrayConst>()){if(!text(item["id"],100))return false;std::string id=item["id"].as<const char*>();for(auto &x:seen)if(x==id)return false;seen.push_back(id);}}
 for(auto x:w["inventory"].as<JsonArrayConst>())if(!text(x["name"],800)||!text(x["kind"],400)||!text(x["status"],400)||!num(x["quantity"],0,10000)||!text(x["unit"],120)||(!x["cost"].isNull()&&!num(x["cost"],0,100000))||!text(x["notes"]))return false;
 for(auto x:w["decisions"].as<JsonArrayConst>()){if(!text(x["title"],800)||!date(x["date"]))return false;for(auto key:{"choice","alternatives","reason","owner"})if(!text(x[key]))return false;}
 for(auto x:w["unknowns"].as<JsonArrayConst>()){if(!text(x["title"],800)||!one(x["status"],{"Inconnue","Provisoire","Confirmée"})||(!x["confidence"].isNull()&&!num(x["confidence"],0,100)))return false;for(auto key:{"hypothesis","source","action"})if(!text(x[key]))return false;}
 for(auto x:w["versions"].as<JsonArrayConst>()){if(!text(x["name"],400)||!date(x["date"])||!one(x["status"],{"Brouillon","À tester","Validée"}))return false;for(auto key:{"mechanical","electronics","software","notes"})if(!text(x[key],8000))return false;}
 for(auto x:w["campaigns"].as<JsonArrayConst>())if(!text(x["title"],800)||!text(x["objective"])||!date(x["start"])||!date(x["end"])||strcmp(x["start"],x["end"])>0||!one(x["type"],{"Terrain","Simulation"})||!one(x["status"],{"Prévue","En cours","Terminée"})||!criteria(x["criteria"]))return false;
 auto plan=w["experiment"];if(!plan.isNull()){
  if(!plan.is<JsonObjectConst>()||!text(plan["createdAt"],100)||!one(plan["parameter"],{"before","during","after","start","end","stop","drop","contrast"})||!plan["values"].is<JsonArrayConst>()||plan["values"].size()<2||plan["values"].size()>8||!plan["repeats"].is<unsigned>()||plan["repeats"].as<unsigned>()<1||plan["repeats"].as<unsigned>()>10||!robotConfig(plan["baseConfig"]))return false;
  std::vector<double> seen;for(auto value:plan["values"].as<JsonArrayConst>()){if(!num(value,0,320))return false;for(auto x:seen)if(x==value.as<double>())return false;seen.push_back(value.as<double>());JsonDocument config;config.set(plan["baseConfig"]);config[plan["parameter"].as<const char*>()]=value;if(!robotConfig(config.as<JsonVariantConst>()))return false;}
 }
 auto s=w["session"];if(!s.is<JsonObjectConst>()||!ids(s["taskIds"],50))return false;for(auto key:{"date","location","objective","materials","trialPlan","notes"})if(!text(s[key]))return false;if(strlen(s["date"].as<const char*>())&&!date(s["date"]))return false;return true;
}
}
