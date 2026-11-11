#include "../src/AuthPolicy.h"
#include <cassert>
#include <iostream>
int main(){using namespace RalfPolicy;
 assert(mayEdit(true,false));assert(!mayEdit(false,false));assert(!mayEdit(true,true));
 assert(mayManage(true,false,true));assert(!mayManage(true,false,false));assert(!mayManage(true,true,true));
 assert(!adminChangeAllowed(1,true,false));assert(adminChangeAllowed(2,true,false));assert(adminChangeAllowed(1,true,true));assert(adminChangeAllowed(1,false,false));
 assert(sessionAlive(100,200));assert(!sessionAlive(200,200));assert(!sessionAlive(300,200));assert(sessionAlive(0xffffff00,0x100));
 assert(validDocumentId(std::string(32,'a')));assert(!validDocumentId("../../accounts.json"));assert(!validDocumentId(std::string(32,'G')));
 assert(allowedDocumentName("PC2.pdf"));assert(allowedDocumentName("rapport.DOCX"));assert(!allowedDocumentName("../../PC2.pdf"));assert(!allowedDocumentName("PC2.html"));assert(!allowedDocumentName("PC2\r\n.pdf"));
 assert(validIsoDate("2026-11-11"));assert(validIsoDate("2028-02-29"));assert(!validIsoDate("2026-02-29"));assert(!validIsoDate("2026-13-01"));
 std::cout<<"Policy checks passed: roles, last admin, expiry, paths, formats and dates.\n";
}
