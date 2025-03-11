#ifndef VERSIONINFO_H
#define VERSIONINFO_H
#include <string>

class CClangOneVersion
{
private:
	static const std::string m_c8Version;
public:
	static const std::string& GetVersion() { return m_c8Version; }
};
#endif
