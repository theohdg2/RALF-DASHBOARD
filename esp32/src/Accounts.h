#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>
#include "AuthPolicy.h"
struct ProjectUser {
 String id,email,name,role,salt,hash;
 bool active=true,mustChange=true;
};
struct ProjectSession { String token,userId; uint32_t expires=0; };
class Accounts {
 WebServer &web;
 Preferences db;
 std::vector<ProjectUser> users;
 std::vector<ProjectSession> sessions;
 bool available=false;
 unsigned failures=0;
 uint32_t blockedUntil=0;
 String randomHex(size_t bytes);
 String passwordHash(const String &password,const String &salt);
 bool equal(const String &a,const String &b);
 bool persist(const std::vector<ProjectUser> &next);
 int byId(const String &id);
 int byEmail(const String &email);
 String newSession(const String &id);
 void invalidate(const String &id);
 size_t activeAdmins();
 void publicUser(JsonObject out,const ProjectUser &user);
 bool parse(JsonDocument &doc);
 bool validEmail(String email);
 void login();void logout();void list();void create();void update();void reset();void password();void profile();
 public:
 explicit Accounts(WebServer &server):web(server){}
 bool begin();
 void routes();
 bool origin();
 int current(bool allowPasswordChange=false);
 int require(bool admin=false,bool allowPasswordChange=false);
 const ProjectUser &user(int index) const {return users[index];}
 void error(int status,const char *code);
 void physicalAdminReset();
};
