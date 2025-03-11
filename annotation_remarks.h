#ifndef ANNOTATION_REMARKS
#define ANNOTATION_REMARKS
#include <string>

class ST_HTML_ANNOTATION
{
private:
	static const std::string m_c8Annotation;
public:
	static const std::string& GetContent() {return m_c8Annotation;}
};
#endif