#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace RalfPolicy {
constexpr size_t MAX_USERS = 12;
constexpr size_t MAX_DOCUMENT_BYTES = 1024 * 1024;
constexpr size_t DOCUMENT_QUOTA = 2560 * 1024;
constexpr size_t MAX_DOCUMENTS = 24;
inline bool sessionAlive(uint32_t now, uint32_t expires) { return static_cast<int32_t>(expires - now) > 0; }
inline bool mayEdit(bool active, bool mustChange) { return active && !mustChange; }
inline bool mayManage(bool active, bool mustChange, bool admin) { return mayEdit(active,mustChange) && admin; }
inline bool adminChangeAllowed(size_t activeAdmins, bool wasActiveAdmin, bool remainsActiveAdmin) {
 return !wasActiveAdmin || remainsActiveAdmin || activeAdmins > 1;
}
inline bool validDocumentId(const std::string &id) {
 if (id.size()!=32) return false;
 for(char c:id) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
 return true;
}
inline bool validIsoDate(const std::string &s) {
 if(s.size()!=10||s[4]!='-'||s[7]!='-')return false;
 for(size_t i=0;i<10;i++)if(i!=4&&i!=7&&(s[i]<'0'||s[i]>'9'))return false;
 int y=std::stoi(s.substr(0,4)),m=std::stoi(s.substr(5,2)),d=std::stoi(s.substr(8,2));
 if(y<1||m<1||m>12)return false;
 const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 int max=days[m-1]+(m==2&&(y%400==0||(y%4==0&&y%100!=0)));
 return d>=1&&d<=max;
}
inline bool allowedDocumentName(const std::string &name) {
 if(name.empty()||name.size()>120||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos)return false;
 for(unsigned char c:name)if(c<32||c==127||c=='"')return false;
 auto dot=name.rfind('.');if(dot==std::string::npos)return false;
 std::string ext=name.substr(dot);for(char &c:ext)if(c>='A'&&c<='Z')c+=32;
 for(const char* allowed:{".pdf",".docx",".doc",".pptx",".xlsx",".png",".jpg",".jpeg",".txt",".md"})if(ext==allowed)return true;
 return false;
}
}
