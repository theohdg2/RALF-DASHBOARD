#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "Accounts.h"
class Documents {
 WebServer &web;fs::LittleFSFS &fs;Accounts &accounts;
 File incoming;String uploadId,uploadName,uploadDate,uploadCategory,uploadOwner;
 size_t received=0;int uploadError=0;bool started=false,finished=false;
 bool metadata(const String&id,JsonDocument&doc);
 String randomId();String path(const String&id,const char*ext);
 size_t inventory(size_t&count);
 void list();void upload();void uploadDone();void download();void remove();
 public:
 Documents(WebServer&w,fs::LittleFSFS&f,Accounts&a):web(w),fs(f),accounts(a){}
 void begin();
 void routes();
};
