#include "xmlparser.h"
#include "gumbo.h"
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <queue>
#include <array>
#include <stack>
#include <span>
#include <unordered_map>
#include <cassert>
#include "VersionInfo.h"
#include "Annotation_remarks.h"

struct ST_HTMLTags
{
    std::unordered_map<std::string, bool> attributes;
    std::unordered_map<std::string, std::string> references;
};

struct ST_TextAttributes
{
    std::string text;
    std::unordered_map<std::string, bool> attributes;
    std::unordered_map<std::string, std::string> references;
    bool crlf;
};

enum class HTML_TAG : size_t { EN_HTML = 0, EN_HEADING = 1, EN_TABLE = 2, EN_TABLE_ROW = 3, EN_TABLE_COLUMN = 4, EN_ROOT };

class BaseInstanceHandler
{
private:
protected:
    virtual void IncStartRef(size_t) = 0;
    virtual void DecStartRef(size_t) = 0;
    virtual size_t GetStartRef(size_t) = 0;

    virtual void IncStopRef(size_t) = 0;
    virtual void DecStopRef(size_t) = 0;
    virtual size_t GetStopRef(size_t) = 0;
public:
    BaseInstanceHandler() {}
    virtual ~BaseInstanceHandler() {}
};

template <size_t N>
class CInstanceHandler : public BaseInstanceHandler
{
private:
    std::array<size_t, N>* pStart;
    std::array<size_t, N>* pStop;

    void IncRef(size_t instance, std::array<size_t, N>* p) {
        if (p != nullptr) {
            if (instance < N) {
                ++(*p)[instance];
            }
            else {
                throw std::runtime_error("Invalid object instance.");
            }
        }
        else {
            throw std::runtime_error("Array not allocated.");
        }
    }
    void DecRef(size_t instance, std::array<size_t, N>* p) {
        if (p != nullptr) {
            if (instance < N) {
                if ((*p)[instance] > 0) {
                    --(*p)[instance];
                }
                else {
                    throw std::runtime_error("Reference error.");
                }
            }
            else {
                throw std::runtime_error("Invalid object instance.");
            }
        }
        else {
            throw std::runtime_error("Array not allocated.");
        }
    }

    size_t GetRef(size_t instance, std::array<size_t, N>* p) {
        if (p != nullptr) {
            if (instance < N) {
                return (*p)[instance];
            }
            else {
                throw std::runtime_error("Invalid object instance.");
                return 0;
            }
        }
        else {
            throw std::runtime_error("Array not allocated.");
            return 0;
        }
    }

public:
    virtual ~CInstanceHandler() { if (pStart != nullptr) delete pStart;  if (pStop != nullptr) delete pStop; }
    CInstanceHandler() : BaseInstanceHandler(), pStart(new std::array<size_t, N>()), pStop(new std::array<size_t, N>()) {}

    virtual void IncStartRef(size_t instance) override { IncRef(instance, pStart); }
    virtual void DecStartRef(size_t instance) override { DecRef(instance, pStart); }
    virtual size_t GetStartRef(size_t instance) override { return GetRef(instance, pStart); }

    virtual void IncStopRef(size_t instance) override { IncRef(instance, pStop); }
    virtual void DecStopRef(size_t instance) override { DecRef(instance, pStop); }
    virtual size_t GetStopRef(size_t instance) override { return GetRef(instance, pStop); }
};

struct HTML_RegExBaseExpressions
{
    HTML_TAG tag;
    std::regex const* content;
    std::regex const* attributes;
};

struct HTML_RegExExpressions
{
    HTML_TAG tag;
    std::regex starting;
    std::regex ending;
    BaseInstanceHandler* pbase;
};

struct HTMLHit {
    size_t matching_pos;
    size_t terminating_pos;
    enum HTML_TAG tag;
    std::smatch match;
    bool operator <(const HTMLHit& comp) const { return this->matching_pos > comp.matching_pos; }
};

struct ST_DOMTREE
{
    HTMLHit content;
    ST_DOMTREE* pChild;
    ST_DOMTREE* pSiblings;
    ST_DOMTREE() : pSiblings(nullptr), pChild(nullptr) {}
    ~ST_DOMTREE() { if (pChild != nullptr) { ST_DOMTREE* ch = pChild;  pChild = nullptr;  delete ch; } if (pSiblings != nullptr) { ST_DOMTREE* sib = pSiblings;  pSiblings = nullptr; delete sib; } }
};

struct ST_TABLE_ELEMENT
{
    HTML_TAG tag;
    ST_TABLE_ELEMENT* pNext;
    ST_TABLE_ELEMENT* pPrev;
    ST_TABLE_ELEMENT* pSub;
    ST_TABLE_ELEMENT* pParent;
    ST_TABLE_ELEMENT() : pNext(nullptr), pPrev(nullptr), pSub(nullptr), pParent(nullptr) {}
    ~ST_TABLE_ELEMENT() {
        if (pSub != nullptr) { ST_TABLE_ELEMENT* p = pSub; pSub = nullptr; delete p; }
        if (pNext != nullptr) { ST_TABLE_ELEMENT* p = pNext; pNext = nullptr; delete p; }
        //if (pPrev != nullptr) { ST_TABLE_ELEMENT* p = pPrev; pPrev = nullptr; delete p; }
        //if (pParent != nullptr) { ST_TABLE_ELEMENT* p = pParent; pParent = nullptr; delete p; }
    }
    std::string content;
    std::unordered_map<std::string, std::string> attributes;
};

struct ST_HTMLContent
{
    std::string html;
    ST_DOMTREE* ptree;
    ST_HTMLContent() : ptree(nullptr) {}
    ~ST_HTMLContent() { ST_DOMTREE* tree = ptree; ptree = nullptr; if (tree != nullptr) { delete tree; } }
};

struct ST_DocumentCollectionContent
{
    ST_HTMLContent index_file;
    
    ST_TABLE_ELEMENT* pIndexTableStruct;
    std::vector<ST_TextAttributes> attributes;

    std::unordered_map<std::string, ST_HTMLContent> source_base;
    ST_DocumentCollectionContent() : index_file(), pIndexTableStruct(nullptr) { attributes.clear(); }
    ~ST_DocumentCollectionContent() { ST_TABLE_ELEMENT* ix = pIndexTableStruct;  pIndexTableStruct = nullptr;  if (ix != nullptr) { delete ix; } }
};

struct ST_Statistics
{
    size_t No_Overall_Lines;
    size_t No_Covered_Lines;
    size_t No_Uncovered_Lines;
    size_t No_Accepted_Uncovered_Lines;
    size_t No_Remaining_Uncovered_Lines;
    size_t No_Annotated_Lines;
    float percentange_covered_lines;
    float percentange_uncovered_lines;
    float percentange_annotated_and_covered_lines;
    std::string file_name;
    std::string file_anchor;
};

#ifdef RECURSIVLY_DEFINED
ST_DOMTREE* findtag(HTML_TAG tag, ST_DOMTREE* proot)
{
    ST_DOMTREE* result = nullptr;
    while ((proot != nullptr) && (result == nullptr))
    {
        if (proot->content.tag != tag)
        {
            if (proot->pChild != nullptr)
            {
                result = findtag(tag, proot->pChild);
            }
            proot = proot->pSiblings;
        }
        else
        {
            result = proot;
        }
    }
    return result;
}
#else
ST_DOMTREE* findtag(HTML_TAG tag, ST_DOMTREE* proot)
{
    ST_DOMTREE* result = nullptr;
    std::stack<ST_DOMTREE*> level;

    while ((proot != nullptr) && (result == nullptr))
    {
        if (proot->content.tag != tag)
        {
            if (proot->pChild != nullptr)
            {
                level.push(proot);
                proot = proot->pChild;
            }
            else if (proot->pSiblings != nullptr)
            {
                proot = proot->pSiblings;
            }
            else
            {
                proot = nullptr;
                while ((level.size()>0) && (proot==nullptr))
                {
                    proot = level.top();
                    level.pop();
                    proot = proot->pSiblings;
                }
            }
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#endif

#ifdef RECURSIVLY_DEFINED
ST_TABLE_ELEMENT* findtag(HTML_TAG tag, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    while ((proot != nullptr) && (result == nullptr))
    {
        if (proot->tag != tag)
        {
            if (proot->pSub != nullptr)
            {
                result = findtag(tag, proot->pSub);
            }
            proot = proot->pNext;
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#else
ST_TABLE_ELEMENT* findtag(HTML_TAG tag, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    std::stack<ST_TABLE_ELEMENT*> level;
    while ((proot != nullptr) && (result == nullptr))
    {
        if (proot->tag != tag)
        {
            if (proot->pSub != nullptr)
            {
                level.push(proot);
                proot = proot->pSub;
            }
            else if (proot->pNext != nullptr)
            {
                proot = proot->pNext;
            }
            else
            {
                proot = nullptr;
                while ((level.size() > 0) && (proot == nullptr))
                {
                    proot = level.top();
                    level.pop();
                    proot = proot->pNext;
                }
            }
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#endif

#ifdef RECURSIVLY_DEFINED
void findalltags(HTML_TAG tag, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_elements)
{
    const auto get_tags = [](ST_TABLE_ELEMENT* proot, const HTML_TAG tag, std::vector<ST_TABLE_ELEMENT*>& related_elements) ->void
    {
        if (tag == proot->tag)
        {
            related_elements.push_back(proot);
        }
    };

    while (proot != nullptr)
    {
        get_tags(proot, tag, related_elements);

        if (proot->pSub != nullptr)
        {
            findalltags(tag, proot->pSub, related_elements);
        }
        proot = proot->pNext;
    }
}
#else
void findalltags(HTML_TAG tag, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_elements)
{
    const auto get_tags = [](ST_TABLE_ELEMENT* proot, const HTML_TAG tag, std::vector<ST_TABLE_ELEMENT*>& related_elements) ->void
    {
        if (tag == proot->tag)
        {
            related_elements.push_back(proot);
        }
    };
    std::stack<ST_TABLE_ELEMENT*> level;
    while (proot != nullptr)
    {
        get_tags(proot, tag, related_elements);

        if (proot->pSub != nullptr)
        {
            level.push(proot);
            proot = proot->pSub;
        }
        else if (proot->pNext)
        {
            proot = proot->pNext;
        }
        else
        {
            proot = nullptr;
            while ((level.size() > 0) && (proot == nullptr))
            {
                proot = level.top();
                level.pop();
                proot = proot->pNext;
            }
        }
    }
}
#endif

#ifdef RECURSIVLY_DEFINED
ST_TABLE_ELEMENT* findcontent(const std::string& content, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    while ((proot != nullptr) && (result==nullptr))
    {
        if (proot->content != content)
        {
            if (proot->pSub != nullptr)
            {
                result = findcontent(content, proot->pSub);
            }
            proot = proot->pNext;
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#else
ST_TABLE_ELEMENT* findcontent(const std::string& content, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    std::stack<ST_TABLE_ELEMENT*> level;

    while ((proot != nullptr) && (result == nullptr))
    {
        if (proot->content != content)
        {
            if (proot->pSub != nullptr)
            {
                level.push(proot);
                proot = proot->pSub;
            }
            else if (proot->pNext)
            {
                proot = proot->pNext;
            }
            else
            {
                proot = nullptr;
                while ((level.size() > 0) && (proot == nullptr))
                {
                    proot = level.top();
                    level.pop();
                    proot = proot->pNext;
                }
            }
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#endif

#ifdef RECURSIVLY_DEFINED
void findallcontent(const std::string& content, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_content)
{
    const auto get_content = [](ST_TABLE_ELEMENT* proot, const std::string& content, std::vector<ST_TABLE_ELEMENT*>& related_content) ->void
    {
        if (content == proot->content)
        {
            related_content.push_back(proot);
        }
    };

    while (proot != nullptr)
    {
        get_content(proot, content, related_content);

        if (proot->pSub != nullptr)
        {
            findallcontent(content, proot->pSub, related_content);
        }
        proot = proot->pNext;
    }
}
#else
void findallcontent(const std::string& content, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_content)
{
    const auto get_content = [](ST_TABLE_ELEMENT* proot, const std::string& content, std::vector<ST_TABLE_ELEMENT*>& related_content) ->void
    {
        if (content == proot->content)
        {
            related_content.push_back(proot);
        }
    };

    std::stack<ST_TABLE_ELEMENT*> level;
    while (proot != nullptr)
    {
        get_content(proot, content, related_content);

        if (proot->pSub != nullptr)
        {
            level.push(proot);
            proot = proot->pSub;
        }
        else if (proot->pNext)
        {
            proot = proot->pNext;
        }
        else
        {
            proot = nullptr;
            while ((level.size() > 0) && (proot == nullptr))
            {
                proot = level.top();
                level.pop();
                proot = proot->pNext;
            }
        }
    }
}
#endif

#ifdef RECURSIVLY_DEFINED
ST_TABLE_ELEMENT* findattribute(const std::string& attrib_name, const std::string& attrib_value, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    const auto get_attribute = [](ST_TABLE_ELEMENT* proot, const std::string& attrib_name, const std::string& attrib_value) -> bool
    {
        bool retVal = false;
        const auto attrib = proot->attributes.find(attrib_name);
        bool key_exists = (attrib != proot->attributes.cend());
        if (key_exists)
        {
            retVal = attrib->second == attrib_value;
        }
        return retVal;
    };


    while ((proot != nullptr) && (result == nullptr))
    {
        if (get_attribute(proot, attrib_name, attrib_value))
        {
            if (proot->pSub != nullptr)
            {
                result = findattribute(attrib_name, attrib_value, proot->pSub);
            }
            proot = proot->pNext;
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#else
ST_TABLE_ELEMENT* findattribute(const std::string& attrib_name, const std::string& attrib_value, ST_TABLE_ELEMENT* proot)
{
    ST_TABLE_ELEMENT* result = nullptr;
    const auto get_attribute = [](ST_TABLE_ELEMENT* proot, const std::string& attrib_name, const std::string& attrib_value) -> bool
    {
        bool retVal = false;
        const auto attrib = proot->attributes.find(attrib_name);
        bool key_exists = (attrib != proot->attributes.cend());
        if (key_exists)
        {
            retVal = attrib->second == attrib_value;
        }
        return retVal;
    };

    std::stack<ST_TABLE_ELEMENT*> level;
    while ((proot != nullptr) && (result == nullptr))
    {
        if (get_attribute(proot, attrib_name, attrib_value))
        {
            if (proot->pSub != nullptr)
            {
                level.push(proot);
                proot = proot->pSub;
            }
            else if (proot->pNext)
            {
                proot = proot->pNext;
            }
            else
            {
                proot = nullptr;
                while ((level.size() > 0) && (proot == nullptr))
                {
                    proot = level.top();
                    level.pop();
                    proot = proot->pNext;
                }
            }
        }
        else
        {
            result = proot;
            break;
        }
    }
    return result;
}
#endif

#ifdef RECURSIVLY_DEFINED
void findallattributes(const std::string& attrib_name, const std::string& attrib_value, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_content)
{
    const auto get_attributes = [](ST_TABLE_ELEMENT* proot, const std::string& attrib_name, const std::string& attrib_value, std::vector<ST_TABLE_ELEMENT*>& related_content) ->void
    {
        const auto attrib = proot->attributes.find(attrib_name);
        bool key_exists = (attrib != proot->attributes.cend());
        if (key_exists)
        {
            key_exists = attrib->second == attrib_value;
            if (key_exists == true)
            {
                related_content.push_back(proot);
            }
        }
    };

    while (proot != nullptr)
    {
        get_attributes(proot, attrib_name, attrib_value, related_content);
        //std::cout << proot->content << std::endl;
        
        if (proot->pSub != nullptr)
        {
            findallattributes(attrib_name, attrib_value, proot->pSub, related_content);
        }

        proot = proot->pNext;
    }
}
#else
void findallattributes(const std::string& attrib_name, const std::string& attrib_value, ST_TABLE_ELEMENT* proot, std::vector<ST_TABLE_ELEMENT*>& related_content)
{
    const auto get_attributes = [](ST_TABLE_ELEMENT* proot, const std::string& attrib_name, const std::string& attrib_value, std::vector<ST_TABLE_ELEMENT*>& related_content) ->void
    {
        const auto attrib = proot->attributes.find(attrib_name);
        bool key_exists = (attrib != proot->attributes.cend());
        if (key_exists)
        {
            key_exists = attrib->second == attrib_value;
            if (key_exists == true)
            {
                related_content.push_back(proot);
            }
        }
    };

    std::stack<ST_TABLE_ELEMENT*> level;
    while (proot != nullptr)
    {
        get_attributes(proot, attrib_name, attrib_value, related_content);

        if (proot->pSub != nullptr)
        {
            level.push(proot);
            proot = proot->pSub;
        }
        else if (proot->pNext)
        {
            proot = proot->pNext;
        }
        else
        {
            proot = nullptr;
            while ((level.size() > 0) && (proot == nullptr))
            {
                proot = level.top();
                level.pop();
                proot = proot->pNext;
            }
        }
    }
}
#endif

void show_all_nodes(size_t depth, ST_TABLE_ELEMENT* proot)
{
    while (proot)
    {
        std::cout << "Depth " << depth << "\t " << proot->content << std::endl;
        if (proot->pSub)
            show_all_nodes(depth + 1, proot->pSub);

        proot = proot->pNext;
    }
}

static bool build_full_dom_tree(ST_HTMLContent& parser, ST_TABLE_ELEMENT*& ptable, std::vector<ST_TextAttributes>& attrib)
{
    const auto get_content_and_attributes = [](HTML_TAG tag, std::string& raw_content, std::string& content, std::unordered_map<std::string, std::string>& attributes) -> void
    {
        static const std::regex heading_content = std::regex(R"(<\s*h\d{1}\s*>(.*?)<\s*/h\d{1}\s*>)");
        static const std::regex table_column_content = std::regex(R"(<\s*td[^>]*>(.*?)<\s*/td\s*>)");
        static const std::regex table_column_attributes = std::regex(R"(<\s*td\s+([^>]+)>[\s\S]*?<\s*/td\s*>)");
        static const std::regex table_row_attributes = std::regex(R"(<\s*tr\s+([^>]+)>.*<\s*/tr\s*>)");

        static const HTML_RegExBaseExpressions ExpressionList[] =
        {
            { HTML_TAG::EN_HTML, nullptr, nullptr},
            { HTML_TAG::EN_HEADING, &heading_content, nullptr},
            { HTML_TAG::EN_TABLE, nullptr, nullptr},
            { HTML_TAG::EN_TABLE_ROW, nullptr, &table_row_attributes},
            { HTML_TAG::EN_TABLE_COLUMN, &table_column_content, &table_column_attributes},
        };

        content.clear();
        attributes.clear();

        std::smatch match_content;
        std::smatch match_attributes;
        std::smatch match_subattributes;
        std::string subattribute_string;

        bool hit_content = false;
        bool hit_attribute = false;
        bool sub_attributes = false;

        switch (tag)
        {
        case HTML_TAG::EN_HTML:
            content = "";
            attributes.clear();
            break;
        case HTML_TAG::EN_HEADING:
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_HEADING)].content != nullptr)
            {
                hit_content = std::regex_search(raw_content, match_content, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_HEADING)].content);
            }
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_HEADING)].attributes != nullptr)
            {
                hit_attribute = std::regex_search(raw_content, match_attributes, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_HEADING)].attributes);
            }
            break;
        case HTML_TAG::EN_TABLE:
            content = "";
            attributes.clear();
            break;
        case HTML_TAG::EN_TABLE_COLUMN:
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_COLUMN)].content != nullptr)
            {
                hit_content = std::regex_search(raw_content, match_content, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_COLUMN)].content);
                static const std::regex subattribute(R"(<\s*pre\s*>\s*<\s*a\s+([^>]+)>\s*([^<]+)\s*<\s*/a\s*>\s*<\s*/pre\s*>)");
                if (hit_content)
                {
                    subattribute_string = match_content[1];
                    sub_attributes = std::regex_search(subattribute_string, match_subattributes, subattribute);
                }
            }
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_COLUMN)].attributes != nullptr)
            {
                hit_attribute = std::regex_search(raw_content, match_attributes, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_COLUMN)].attributes);
            }
            break;
        case HTML_TAG::EN_TABLE_ROW:
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_ROW)].content != nullptr)
            {
                hit_content = std::regex_search(raw_content, match_content, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_ROW)].content);
            }
            if (ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_ROW)].attributes != nullptr)
            {
                hit_attribute = std::regex_search(raw_content, match_attributes, *ExpressionList[static_cast<size_t>(HTML_TAG::EN_TABLE_ROW)].attributes);
            }
            break;
        case HTML_TAG::EN_ROOT:
            content = "";
            attributes.clear();
            break;
        }

        static const std::regex attrib_pattern(R"(([^\s=]+)\s*=\s*(['"])(.*?)\2)");
        if (sub_attributes == true)
        {
            content = match_subattributes[2].str();
            std::string attribute = match_subattributes[1].str();
            std::smatch attr_match;

            std::string::const_iterator searchStart = attribute.cbegin();
            while (std::regex_search(searchStart, attribute.cend(), attr_match, attrib_pattern)) {
                attributes[attr_match[1].str()] = attr_match[3].str();
                searchStart = attr_match.suffix().first;
            }
        }
        else if (hit_content == true)
        {
            static const std::regex pre_removal(R"(<\s*pre\s*>\s*(.*)<\s*/pre\s*>)");
            std::smatch match_cont;
            content = match_content[1].str();
            if (std::regex_search(content, match_cont, pre_removal))
            {
                content = match_cont[1].str();
            }
            else
            {
                content = match_content[1].str();
            }
        }

        if (hit_attribute == true)
        {

            std::string attribute = match_attributes[1].str();
            std::smatch attr_match;

            std::string::const_iterator searchStart = attribute.cbegin();
            while (std::regex_search(searchStart, attribute.cend(), attr_match, attrib_pattern)) {
                std::smatch enclosure_match;
                static const std::regex enclosure_removal(R"((^[\'\"])(.*)\1$)");
                std::string enclosed_attribute = attr_match[1].str();
                std::string enclosed_attribute_value = attr_match[3].str();
                if (std::regex_search(enclosed_attribute, enclosure_match, enclosure_removal))
                {
                    enclosed_attribute = enclosure_match[2].str();
                }
                if (std::regex_search(enclosed_attribute_value, enclosure_match, enclosure_removal))
                {
                    enclosed_attribute_value = enclosure_match[2].str();
                }


                attributes[enclosed_attribute] = enclosed_attribute_value;
                searchStart = attr_match.suffix().first;
            }
        }
    };

    bool retVal = true;
    ptable = nullptr;
    if (parser.ptree != nullptr)
    {
        ST_DOMTREE* ptree = parser.ptree;
        std::unordered_map<std::string, std::vector<std::string>> dictionary;
        ST_TABLE_ELEMENT* table = nullptr;
        ST_TABLE_ELEMENT* top = nullptr;

        ST_TABLE_ELEMENT* sibling_node = nullptr;
        ST_TABLE_ELEMENT* parent_node = nullptr;

        std::stack<ST_DOMTREE*> parent;
        std::stack<ST_TABLE_ELEMENT*> parent_element;
        while (ptree)
        {
            ST_TABLE_ELEMENT* new_table = new ST_TABLE_ELEMENT;
            if (new_table != nullptr)
            {
                if (top == nullptr)
                    top = new_table;

                new_table->tag = ptree->content.tag;
                std::string cont = parser.html.substr(ptree->content.matching_pos, ptree->content.terminating_pos - ptree->content.matching_pos);
                get_content_and_attributes(new_table->tag, cont, new_table->content, new_table->attributes);
                if (ptree->pChild)
                {
                    dictionary[ptree->content.match[0].str()].push_back(parser.html.substr(ptree->content.matching_pos, ptree->content.terminating_pos - ptree->content.matching_pos));
                    parent.push(ptree);
                    ptree = ptree->pChild;

                    new_table->pParent = parent_node;
                    new_table->pPrev = sibling_node;

                    if (sibling_node == nullptr)
                    {
                        if (parent_node != nullptr)
                            parent_node->pSub = new_table;
                    }
                    else
                    {
                        sibling_node->pNext = new_table;
                    }

                    table = new_table;
                    parent_node = table;
                    sibling_node = nullptr;
                    parent_element.push(table);
                }
                else if (ptree->pSiblings)
                {
                    dictionary[ptree->content.match[0].str()].push_back(parser.html.substr(ptree->content.matching_pos, ptree->content.terminating_pos - ptree->content.matching_pos));
                    ptree = ptree->pSiblings;

                    new_table->pParent = parent_node;
                    new_table->pPrev = sibling_node;
                    if (sibling_node != nullptr)
                    {
                        sibling_node->pNext = new_table;
                    }
                    else
                    {
                        if (parent_node != nullptr)
                            parent_node->pSub = new_table;
                    }
                    //parent_node = nullptr;
                    table = new_table;
                    sibling_node = table;
                }
                else
                {
                    dictionary[ptree->content.match[0].str()].push_back(parser.html.substr(ptree->content.matching_pos, ptree->content.terminating_pos - ptree->content.matching_pos));
                    if (sibling_node == nullptr)
                    {
                        if (parent_node != nullptr)
                            parent_node->pSub = new_table;
                    }
                    else
                    {
                        sibling_node->pNext = new_table;
                    }

                    if (parent.size() == parent_element.size() && (parent.size() > 0))
                    {
                        do {
                            ptree = parent.top();
                            ptree = ptree->pSiblings;
                            sibling_node = parent_element.top();
                            parent.pop();
                            parent_element.pop();
                            if (parent_element.size())
                            {
                                parent_node = parent_element.top();
                            }
                            else
                            {
                                parent_node = nullptr;
                            }
                            new_table->pParent = sibling_node;
                            table = sibling_node;
                        } while ((parent.size() > 0) && (ptree == nullptr));
                    }
                    else
                    {
                        ptree = nullptr;
                    }
                }
            }
            else
            {
                retVal = false;
                if (top != nullptr)
                    delete top;
                top = nullptr;
            }
        }
        ptable = top;
    }
    else
    {
        retVal = false;
    }
    return retVal;
}


/*------------------------------------------------------------------------------------------*//**
\brief          traverses the index file and returns the table
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool open_html_file(std::filesystem::path const& source, std::string& content)
{
    bool retVal = false;

    std::ifstream file(source);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
    }
    else
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        retVal = true;
    }

    return retVal;
}

static bool findmatch(std::string::const_iterator start, std::string::const_iterator stop, std::regex& expression, std::smatch& match)
{
    return std::regex_search(start, stop, match, expression);
}

template <size_t N>
bool build_DOM_tree_nodes(const HTML_RegExExpressions(&html_tags)[N], std::pair<std::priority_queue<HTMLHit>&, std::priority_queue<HTMLHit>&>& queue, const std::string& content, ST_DOMTREE** ppTree)
{
    bool retVal = true;
    std::priority_queue<HTMLHit>& in = queue.first;
    std::priority_queue<HTMLHit>& out = queue.second;
    std::stack<ST_DOMTREE*> top_nodes;

    ST_DOMTREE** top_node = ppTree;

    auto find_match_starting = [&](HTMLHit& top, HTMLHit& res) ->bool
    {
        bool retVal = true;
        std::smatch match;
        std::regex r1 = html_tags[static_cast<size_t>(top.tag)].starting;
        std::string::const_iterator a1 = top.match.suffix().first;
        bool start = findmatch(a1, content.cend(), r1, match);
        if (start == true)
        {
            const size_t index = match[0].first - content.cbegin();
            res.matching_pos = index;
            res.tag = top.tag;
            res.match = match;
        }
        else
        {
            retVal = false;
        }
        return retVal;
    };

    auto find_match_ending = [&](HTMLHit& top, HTMLHit& res) ->bool
    {
        bool retVal = true;
        std::smatch match;
        std::regex r1 = html_tags[static_cast<size_t>(top.tag)].ending;
        std::string::const_iterator a1 = top.match.suffix().first;
        bool start = findmatch(a1, content.cend(), r1, match);
        if (start == true)
        {
            const size_t index = match[0].first - content.cbegin();
            res.matching_pos = index;
            res.tag = top.tag;
            res.match = match;
        }
        else
        {
            retVal = false;
        }
        return retVal;
    };

    //allocates an object and determines, if it is a child or sibling
    auto PrioQueueHandler = [&](HTMLHit& on_top, HTMLHit& out_top) ->bool
    {
        bool retVal = true;
        ST_DOMTREE* new_node = new ST_DOMTREE;
        if (new_node != nullptr)
        {
            new_node->content = on_top;
            if (on_top.tag != out_top.tag) //next node must be a child node
            {
                HTMLHit res;
                *top_node = new_node;
                new_node->content = on_top;
                top_nodes.push(new_node);
                top_node = &new_node->pChild;
                bool retval = find_match_starting(on_top, res);
                if (retval)
                {
                    in.push(res);
                }
            }
            else //they are equal. Are they siblings?
            {
                HTMLHit res_in;
                bool in_stat = find_match_starting(on_top, res_in);

                if ((in_stat == false) || ((res_in.matching_pos > out_top.matching_pos) && (in_stat == true)))
                {
                    //in this case it is a sibling
                    out.pop();
                    *top_node = new_node;
                    new_node->content = on_top;
                    top_node = &new_node->pSiblings;
                    new_node->content.terminating_pos = out_top.match.suffix().first - content.cbegin();
                    if (in_stat == true)
                    {
                        in.push(res_in);
                    }

                    HTMLHit res_out;
                    bool out_stat = find_match_ending(out_top, res_out);
                    if (out_stat)
                    {
                        out.push(res_out);
                        //new_node->content.terminating_pos = res_out.match.suffix().first-content.cbegin();
                    }
                    //else
                    //{
                    //    new_node->content.terminating_pos = std::string::npos;
                    //}
                }
                else
                {
                    //it is a child
                    *top_node = new_node;
                    new_node->content = on_top;
                    top_nodes.push(new_node);
                    top_node = &new_node->pChild;
                    if (in_stat == true)//no more childs of this type
                    {
                        in.push(res_in);
                    }
                }
            }
        }
        else
        {
            retVal = false;
        }
        return retVal;
    };

    while (out.size() && retVal == true)
    {
        const auto RewindDOMTree = [&](HTMLHit& on_top, HTMLHit& out_top) -> bool
        {
            bool rewind = false;
            out_top = out.top();
            if (in.size() == 0)
            {
                rewind = true;
                on_top = out_top;// top_nodes.top()->content;
                //in.push(on_top);
                if (top_nodes.size() > 0)
                {
                    if ((*top_nodes.top()).content.tag == out_top.tag)
                    {
                        std::cout << "Resolved tags " << static_cast<size_t>(on_top.tag) << " column " << on_top.matching_pos << std::endl;
                    }
                    else
                    {
                        std::cout << "Unresolved tags " << static_cast<size_t>(on_top.tag) << " column " << on_top.matching_pos << std::endl;
                    }
                }
                else
                {
                    std::cout << "Unresolved tags " << static_cast<size_t>(on_top.tag) << " column " << on_top.matching_pos << std::endl;
                }
            }
            else
            {
                on_top = in.top();
                rewind = on_top.matching_pos > out_top.matching_pos;
            }
            return rewind;
        };

        HTMLHit out_top;
        HTMLHit on_top;

        if (RewindDOMTree(on_top, out_top))
        {
            top_node = nullptr;
            if (top_nodes.size())
            {
                top_node = &top_nodes.top();
                while ((out.size() > 0) && ((*top_node)->content.tag != out_top.tag))
                {
                    std::cerr << "HTML tags do not match. TopNode: " << (*top_node)->content.matching_pos << " Top: " << on_top.matching_pos << " Out: " << out_top.matching_pos << std::endl;
                    //pop non fitting item
                    out.pop();

                    bool stop_ret = find_match_ending(out_top, out_top);
                    if (stop_ret == true)
                    {
                        out.push(out_top);
                    }
                    if (out.size() > 0)
                        out_top = out.top();
                }

                if (out.size() > 0)
                {
                    (*top_node)->content.terminating_pos = out_top.match.suffix().first - content.cbegin();
                    out.pop();
                }
                else
                {
                    (*top_node)->content.terminating_pos = std::string::npos;
                    std::cerr << "unable to recover from error" << std::endl;
                }

                top_nodes.pop();

                bool stop_ret = find_match_ending(out_top, out_top);
                if (stop_ret == true)
                {
                    out.push(out_top);
                }
                top_node = &(*top_node)->pSiblings;
            }
        }
        else
        {
            in.pop();
            retVal = PrioQueueHandler(on_top, out_top);
        }
    }
    return retVal;
}

static bool WriteTree(std::ofstream& stream, ST_DOMTREE* pDOMTree)
{
    std::stack<ST_DOMTREE*> parent_nodes;
    /*while (pDOMTree != nullptr)
    {
        if (pDOMTree->content.match.prefix() != nullptr)
        {

        }
        pDOMTree = pDOMTree->pSiblings;
    }*/
    return true;

}

static bool FlushDOMTree(const std::string& file_name, ST_DOMTREE* pDOMTree)
{
    bool retVal;
    if (pDOMTree != nullptr)
    {
        std::ofstream out(file_name);
        if (out.is_open())
        {
            retVal = WriteTree(out, pDOMTree);
        }
        else
        {
            retVal = false;
        }
    }
    else
    {
        retVal = false;
    }
    return retVal;
}

static bool create_DOM_tree(const std::string& content, ST_DOMTREE** ppDomTree)
{
    bool retVal;
    if (ppDomTree != nullptr)
    {
        retVal = true;;
        *ppDomTree = nullptr;
        CInstanceHandler<1> HTML;
        CInstanceHandler<1> HEADING;
        CInstanceHandler<1> TABLE;
        CInstanceHandler<1> TABLE_ROW;
        CInstanceHandler<1> TABLE_COLUMN;

        static const HTML_RegExExpressions ExpressionList[] =
        {
            { HTML_TAG::EN_HTML, std::regex(R"(<\s*html\s*>)"), std::regex(R"(<\s*/html\s*>)"), &HTML},
            { HTML_TAG::EN_HEADING, std::regex(R"(<\s*h\d{1}\s*>)"), std::regex(R"(<\s*/h\d{1}\s*>)"), &HEADING},
            { HTML_TAG::EN_TABLE, std::regex(R"(<\s*table\s*>)"), std::regex(R"(<\s*/table\s*>)"), &TABLE },
            { HTML_TAG::EN_TABLE_ROW, std::regex(R"(<\s*tr\s*[^>]*\s*>)"), std::regex(R"(<\s*/tr\s*>)"), &TABLE_ROW },
            { HTML_TAG::EN_TABLE_COLUMN, std::regex(R"(<\s*td\s*[^>]*\s*>)"), std::regex(R"(<\s*/td\s*>)"), &TABLE_COLUMN },
        };

        std::priority_queue<HTMLHit> min_priority_queue_starting_quotes;
        std::priority_queue<HTMLHit> min_priority_queue_ending_quotes;
        std::pair<std::priority_queue<HTMLHit>&, std::priority_queue<HTMLHit>&> queue(min_priority_queue_starting_quotes, min_priority_queue_ending_quotes);
        for (auto& a : ExpressionList)
        {

            HTMLHit lines;
            std::smatch match;
            bool found_start = std::regex_search(content, match, a.starting);
            if (found_start == true)
            {
                lines.tag = a.tag;
                lines.match = match;
                lines.matching_pos = match[0].first - content.cbegin();
                min_priority_queue_starting_quotes.push(lines);
            }

            bool found_end = std::regex_search(content, match, a.ending);
            if (found_end == true)
            {
                lines.tag = a.tag;
                lines.match = match;
                lines.matching_pos = match[0].first - content.cbegin();
                min_priority_queue_ending_quotes.push(lines);
            }

            if (found_start != found_end)
            {
                std::cerr << "Invalid html file. Number of tags do not match" << std::endl;
                retVal = false;
            }
        }

        if (retVal == true)
        {
            retVal = build_DOM_tree_nodes(ExpressionList, queue, content, ppDomTree);
        }
    }
    else
    {
        retVal = false;
    }
    return retVal;
}

static bool create_annotated_html(const std::string& content, std::string& new_content, ST_Statistics& stats, ST_DOMTREE* pDomTree)
{
    const auto check_if_bracket = [](std::string& code) -> bool
    {
        //static const std::regex no_content(R"(<\s*span\s*class\s*=\s*'red'[^>]*>\s*(?:\{|\}|else|case\s+\w+\s*:|break\s*;|break\s*<\s*/span\s*>\s*;|default\s*:){1}\s*(//)?|<\s*pre\s*>\s*else\s*(//)?|<\s*pre\s*><span\s+class\s*=\s*'red'\s*>\s*#ifndef\s*(//)?|<\s*pre\s*><span\s+class\s*=\s*'red'\s*>\s*#ifdef\s*(//)?|<\s*pre\s*><span\s+class\s*=\s*'red'\s*>\s*#if\s*(//)?|<\s*pre\s*><span\s+class\s*=\s*'red'\s*>\s*#endif\s*(//)?|<\s*pre\s*><span\s+class\s*=\s*'red'\s*>\s*typedef\s*(//)?)");//
        static const std::regex no_content(
            R"(<\s*span\s*class\s*=\s*'[^']*\bred\b[^']*'[^>]*>\s*(?:\{|\}|else|case\s+\w+\s*:|break\s*;|break\s*<\s*/span\s*>\s*;|default\s*:)\s*(?://)?|<\s*pre\s*>\s*else\s*(?://)?|<\s*pre\s*><span\s+class\s*=\s*'[^']*\bred\b[^']*'\s*>\s*#ifndef\s*(?://)?|<\s*pre\s*><span\s+class\s*=\s*'[^']*\bred\b[^']*'\s*>\s*#ifdef\s*(?://)?|<\s*pre\s*><span\s+class\s*=\s*'[^']*\bred\b[^']*'\s*>\s*#if\s*(?://)?|<\s*pre\s*><span\s+class\s*=\s*'[^']*\bred\b[^']*'\s*>\s*#endif\s*(?://)?|<\s*pre\s*><span\s+class\s*=\s*'[^']*\bred\b[^']*'\s*>\s*typedef\s*(?://)?)"
        );

        std::smatch match;
        return std::regex_search(code, match, no_content);
    };

    ST_DOMTREE* tree = pDomTree;
    std::stack<ST_DOMTREE*> parent;
    std::stack<bool> table_match;

    std::unordered_map<HTML_TAG, size_t> flags;
    flags[HTML_TAG::EN_HEADING] = 0;
    flags[HTML_TAG::EN_HTML] = 0;
    flags[HTML_TAG::EN_TABLE] = 0;
    flags[HTML_TAG::EN_TABLE_COLUMN] = 0;
    flags[HTML_TAG::EN_TABLE_ROW] = 0;
    flags[HTML_TAG::EN_ROOT] = 0;

    bool line_hit = false;
    bool count_hit = false;
    bool source_hit = false;
    bool annotated_code = false;
    size_t covered_lines = 0u;
    size_t uncovered_lines = 0u;
    size_t strictly_remaining_uncovered_lines = 0u;
    size_t remaining_uncovered_lines = 0u;
    size_t currently_processed_line = 0u;


    size_t latest_hit = 0u;
    size_t row_count = 0;
    ST_DOMTREE* row_match = nullptr;
    std::string annotation;
    std::string annotation_string;
    std::string additional_remarks;
    std::unordered_map<size_t, size_t> overall_number_of_lines;
    std::unordered_map<size_t, size_t> dict_covered_lines;
    std::unordered_map<size_t, size_t> dict_uncovered_lines;
    std::unordered_map<size_t, size_t> dict_accepted_uncovered_lines;
    std::unordered_map<size_t, size_t> dict_remaining_uncovered_lines;

    //static const std::regex annotation_pattern(R"(//.*?&lt;cov\s+class=&apos;(A\d)&apos;&gt;(.*?)&lt;\s*/cov\s*&gt;)");
    static const std::regex annotation_pattern(R"(//.*?ccov:\s+(A[\d{1}E])(?:\b.*&lt;\s*([^&]+)\s*&gt;)*)");

    while (tree != nullptr)
    {
        std::string sub_content;
        flags[tree->content.tag] += 1u;
        sub_content = tree->content.terminating_pos == std::string::npos ? content.substr(tree->content.matching_pos) : content.substr(tree->content.matching_pos, tree->content.terminating_pos - tree->content.matching_pos);

        if (tree->content.tag == HTML_TAG::EN_TABLE)
        {
            line_hit = false;
            count_hit = false;
            source_hit = false;
            annotated_code = false;
            currently_processed_line = 0;
            annotation_string.clear();
            additional_remarks.clear();
            annotation.clear();
        }
        else if (tree->content.tag == HTML_TAG::EN_TABLE_ROW)
        {
            annotation_string.clear();
            additional_remarks.clear();
            annotation.clear();
            annotation_string.clear();
            currently_processed_line = 0;
            annotated_code = false;
            row_match = tree;
            row_count = 0;
            std::smatch match;
            static const std::regex table_pattern(R"(<\s*table\s*>)");
            //const size_t endpos = (tree->pChild != nullptr) ? tree->pChild->content.matching_pos : tree->content.terminating_pos;
            size_t length = tree->content.terminating_pos == std::string::npos ? 0u : tree->content.terminating_pos - tree->content.matching_pos;
            bool table_hit = std::regex_search(sub_content, match, table_pattern);
            if (table_hit)
            {
                if (static_cast<size_t>(match.position()) < length)
                {
                    length = match.position();
                }
            }

            if (length)
            {
                bool is_plain_line = false;
                
                if (tree->pChild != nullptr)
                {
                    bool expr_line_result = false;
                    bool potentially_unexecutable_code = false;
                    bool covered_line = false;
                    ST_DOMTREE* lock_ahead = tree->pChild;
                    while (lock_ahead)
                    {
                        static const std::regex expr_line(R"(class\s*=\s*'covered-line')");
                        const size_t length = lock_ahead->content.terminating_pos == std::string::npos ? std::string::npos : lock_ahead->content.terminating_pos - lock_ahead->content.matching_pos;
                        std::string block = content.substr(lock_ahead->content.matching_pos, length);
                        expr_line_result = std::regex_search(block, match, expr_line);
                        if (expr_line_result == true)
                        {
                            covered_line = true;
                            break;
                        }
                        else
                        {
							static const std::regex skipped_line(R"(class\s*=\s*'skipped-line')");
							bool skipped_line_found = std::regex_search(block, match, skipped_line);
                            if (skipped_line_found)
                            {
                                covered_line = true;
                                break;
							}
                            else
                            {
                                static const std::regex empty_uncovered_line(R"(<td class\s*=\s*'uncovered-line'[^>]*>\s*<\s*/td\s*>)");
                                bool empty_uncovered_line_found = std::regex_search(block, match, empty_uncovered_line);
                                if (empty_uncovered_line_found)//uncovered line but not touched -> means it is not zero -> maybe better to check for color?
                                {
                                    covered_line = true;
                                    break;
                                }
                                else
                                {
                                    static const std::regex expr_code(R"(class\s*=\s*'code')");
                                    bool start_found = std::regex_search(block, match, expr_code);
                                    if (start_found == true)
                                    {
                                        if (check_if_bracket(block))
                                        {
                                            annotation_string = "E0";
                                            additional_remarks = "not executable";
                                            annotation = annotation_string;
                                            is_plain_line = true;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        lock_ahead = lock_ahead->pSiblings;
                    }
                }
                if (is_plain_line==false)
                {
                    const std::string exp = content.substr(tree->content.matching_pos, length);
                    bool hit = std::regex_search(exp, match, annotation_pattern);
                    if (hit == true)
                    {
                        annotation_string = match[1].str();
                        additional_remarks = match[2].str();
                        annotation = annotation_string;
                    }
                    else
                    {

                    }
                }
            }
        }
        else if (tree->content.tag == HTML_TAG::EN_TABLE_COLUMN)
        {
            if (flags[HTML_TAG::EN_TABLE_COLUMN] > 0u)
            {
                size_t line_hit_count = sub_content.find("Line");
                size_t count_hit_count = sub_content.find("Count");
                size_t source_hit_count = sub_content.find("Source");
                if (line_hit_count != std::string::npos)
                {
                    line_hit = true;
                    count_hit = false;
                    source_hit = false;
                    annotated_code = false;
                    currently_processed_line = 0;
                }
                else if ((line_hit == true) && (count_hit_count != std::string::npos))
                {
                    line_hit = true;
                    count_hit = true;
                    source_hit = false;
                    annotated_code = false;
                    currently_processed_line = 0;
                }
                else if ((line_hit == true) && (count_hit == true) && (source_hit_count != std::string::npos))
                {
                    source_hit = true;
                    annotated_code = false;
                    currently_processed_line = 0;
#ifdef APPENDCOLUMN
                    static const std::string modified_header = "<tr><td><pre>Line</pre></td><td><pre>Annotation</pre></td><td><pre>Count</pre></td><td><pre>Source</pre></td><td><pre>Remarks</pre></td>";//the last </tr> comes from the existing tr node.
#else
                    static const std::string modified_header = "<tr><td><pre>Line</pre></td><td><pre>Annotation</pre></td><td><pre>Count</pre></td><td><pre>Source</pre></td>";//the last </tr> comes from the existing tr node.
#endif
                    if (row_match->content.matching_pos != std::string::npos && latest_hit != std::string::npos)
                    {
                        new_content.append(content, latest_hit, row_match->content.matching_pos - latest_hit);
                        new_content.append(modified_header);
                        latest_hit = tree->content.terminating_pos;
                    }
                }
                else
                {
                    //match OK
                    std::smatch match;
                    static const std::regex expr_line_no(R"(class='line-number')");
                    static const std::regex expr_covered_line(R"(class='covered-line')");
                    static const std::regex expr_uncovered_line(R"(class='uncovered-line')");
                    static const std::regex expr_real_uncovered_line(R"(<\s*?td\s*?class\s*?=\s*?'uncovered-line'><pre>(0)</pre><\s*?/td\s*?>)");
                    static const std::regex expr_code(R"(class='code')");
                    static const std::regex expr_skipped_line(R"(class='skipped-line')");
                    static const std::regex exception_code(R"(([AE]?[0-7])|AE)");
                    std::string exp = tree->content.match[0].str();
                    bool found_start = std::regex_search(exp, match, expr_line_no);
                    if (found_start)
                    {
                        static const std::string additional_column_annotation_start = "<td class='annotation'><pre>";
                        static const std::string additional_column_annotation_stop = "</pre></td>";
                        std::string dummy;
                        dummy.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        new_content.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        latest_hit = tree->content.terminating_pos;
                        new_content.append(additional_column_annotation_start + annotation_string + additional_column_annotation_stop);
                        if (annotation_string.length())
                        {
                            annotated_code = true;
                            annotation_string = "";
                        }
                        static const std::regex line_match_no(R"(name\s*=\s*'L(\d+)')");
                        /*size_t rest_size = tree->pSiblings != nullptr ? tree->pSiblings->content.matching_pos : std::string::npos;
                        rest_size = tree->pChild != nullptr ? tree->pChild->content.matching_pos : rest_size;
                        rest_size = rest_size == std::string::npos ? rest_size : rest_size - tree->content.terminating_pos;*/
                        std::string line = content.substr(tree->content.matching_pos, tree->content.terminating_pos - tree->content.matching_pos);
                        bool line_found = std::regex_search(line, match, line_match_no);
                        if (line_found)
                        {
                            std::string str = match[1].str();
                            try {
                                size_t line_no = stoul(str);
                                currently_processed_line = line_no;
                            }
                            catch (const std::invalid_argument& ia)
                            {
                                currently_processed_line = 0u;
                                std::cerr << "Invalid argument: " << ia.what() << std::endl;
                            }
                            catch (const std::out_of_range& ia)
                            {
                                currently_processed_line = 0u;
                                std::cerr << "Invalid argument: " << ia.what() << std::endl;
                            }
                            ++overall_number_of_lines[currently_processed_line];//is the error bin
                        }
                    }

#ifdef APPENDCOLUMN //needs additional tweaking -> deleting remarks
                    found_start = std::regex_search(exp, match, expr_code);
                    if (found_start)
                    {
                        static const std::string additional_column_remarks_start = "<td class='remark'><pre>";
                        static const std::string additional_column_remarks_stop = "</pre></td>";
                        std::string code_row;
                        code_row.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        new_content.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        latest_hit = tree->content.terminating_pos;
                        new_content.append(additional_column_remarks_start + additional_remarks + additional_column_remarks_stop);
                        additional_remarks = "";
                    }
#else
                    const bool skip_line = std::regex_search(exp, match, expr_skipped_line);
                    found_start = std::regex_search(exp, match, expr_code) && !skip_line;

                    if (found_start)
                    {
                        const size_t length = tree->content.terminating_pos == std::string::npos ? std::string::npos : tree->content.terminating_pos - latest_hit;
                        std::string code_row = content.substr(latest_hit, tree->content.terminating_pos - latest_hit);

                        bool bracket_check = check_if_bracket(code_row);
                        if (annotated_code || bracket_check)
                        {
                            static const std::regex class_red(
                                R"((<\s*span\s+class\s*=\s*'[^']*)\bred\b([^']*'[^>]*>))"
                            );
                            if (bracket_check==false)
                            {
                                if (annotated_code)
                                {
                                    static const std::regex ccov_only(
                                        R"(//\s*ccov:\s*A(?:\d|E)(?:\s*&lt;.*?&gt;)?\s*(?=(?:<span[^>]*>\s*</span>\s*)*</pre>\s*</td>\s*$))"
                                    );

                                    static const std::regex ccov_suffix(
                                        R"(\s+ccov:\s*A(?:\d|E)(?:\s*&lt;.*?&gt;)?\s*(?=(?:<span[^>]*>\s*</span>\s*)*</pre>\s*</td>\s*$))"
                                    );

                                    code_row = std::regex_replace(code_row, ccov_only, "");
                                    code_row = std::regex_replace(code_row, ccov_suffix, "");
                                }
                                static const std::string class_cyan = "$1cyan$2";
                                if (std::regex_search(code_row, class_red))
                                {
                                    code_row = std::regex_replace(code_row, class_red, class_cyan);
                                }
                            }
                            else
                            {
                                static const std::regex no_content(R"(<\s*span\s+class\s*=\s*'[^']*\bred\b[^']*'[^>]*>(.*?)<\s*/span\s*>)");
                                std::smatch match;
                                if (std::regex_search(code_row, match, no_content))
                                {
                                    std::string prefix_str(match.prefix().first, match.prefix().second);
                                    std::string suffix_str(match.suffix().first, match.suffix().second);
                                    code_row = prefix_str + match[1].str() + suffix_str;
                                }
                            }

                            latest_hit = tree->content.terminating_pos;
                            if (bracket_check == false)
                            {
                                std::smatch match;
                                const bool match_annotation = std::regex_search(code_row, match, annotation_pattern);
                                if (match_annotation)
                                {
                                    std::string prefix_str(match.prefix().first, match.prefix().second);
                                    std::string suffix_str(match.suffix().first, match.suffix().second);
                                    code_row = prefix_str + suffix_str;
                                    static const std::regex del_comment(R"(\s*//\s*(?=(<\s*/span\s*>)*<\s*/pre\s*><\s*/td\s*>))");
                                    const bool del_comment_result = std::regex_search(code_row, match, del_comment);
                                    if (del_comment_result)
                                    {
                                        code_row = std::string(match.prefix().first, match.prefix().second);
                                        latest_hit = tree->content.terminating_pos;
                                    }
                                }
                            }
                            else
                            {
                            }
                            new_content.append(code_row);
                        }
                        annotation.clear();
                        annotation_string.clear();
                        annotated_code = false;
                    }
#endif
                    std::string node_content = tree->content.match[0].str();
                    bool line_covered = std::regex_search(exp, match, expr_covered_line);
                    if (line_covered)
                    {
                        ++covered_lines;
                        annotation.clear();
                        annotation_string.clear();
                        if (currently_processed_line != 0u)
                        {
                            ++dict_covered_lines[currently_processed_line];
                        }
                    }

                    bool line_uncovered = std::regex_search(exp, match, expr_uncovered_line);
                    if (line_uncovered)
                    {
                        const size_t length = (tree->content.terminating_pos == std::string::npos) ? 0 : (tree->content.terminating_pos - tree->content.matching_pos);
                        ++uncovered_lines;

                        if (length > 0)
                        {
                            const std::string node_content = content.substr(tree->content.matching_pos, length);

                            bool real_line_uncovered = std::regex_search(node_content, match, expr_real_uncovered_line);
                            if (real_line_uncovered)
                            {
                                ++strictly_remaining_uncovered_lines;
                                if (currently_processed_line != 0u)
                                {
                                    ++dict_uncovered_lines[currently_processed_line];
                                }
                            }

                            bool covered_by_exception = std::regex_search(annotation, match, exception_code);
                            if (!covered_by_exception && real_line_uncovered)
                            {
                                ++remaining_uncovered_lines;
                                if (currently_processed_line>0)
                                    ++dict_remaining_uncovered_lines[currently_processed_line];
                            }
                            else
                            {
                                //it is covered. Therefore, we can treat the string explicitly
                                const size_t length = tree->content.terminating_pos != std::string::npos ? (tree->content.terminating_pos - latest_hit) : 0u;
                                if (length > 0u)
                                {
                                    std::string dummy = content.substr(latest_hit, tree->content.terminating_pos - latest_hit);
                                    //static const std::string uncovered_line = "class=\'uncovered-line\'";
                                    static const std::string covered_line = "class=\'covered-line\'";
                                    static const std::regex uncovered_line_ex(R"(class\s*=\s*\'uncovered-line\')");
                                    //static const std::regex covered_line_ex(R"(class\s*=\s*\'covered-line\')");
                                    dummy = std::regex_replace(dummy, uncovered_line_ex, covered_line);
                                    new_content.append(dummy);
                                    latest_hit = tree->content.terminating_pos;
                                    if (currently_processed_line != 0u)
                                    {
                                        if (covered_by_exception)
                                        {
                                            ++dict_accepted_uncovered_lines[currently_processed_line];
                                        }
                                        else
                                        {
                                            if (real_line_uncovered && currently_processed_line>0)
                                                ++dict_remaining_uncovered_lines[currently_processed_line];
                                        }
                                    }
                                }
                            }
                        }
                        annotation.clear();
                        annotation_string.clear();
                    }

                    if (skip_line)
                    {
                        annotation.clear();
                        annotation_string.clear();
                    }

                    ++row_count;
                    line_hit = false;
                    count_hit = false;
                    source_hit = false;
                }
            }
        }

        if (tree->pChild != nullptr)
        {
            parent.push(tree);
            tree = tree->pChild;
            table_match.push(source_hit);
        }
        else if (tree->pSiblings != nullptr)
        {
            tree = tree->pSiblings;
        }
        else
        {
            tree = parent.top();
            while ((tree->pSiblings == nullptr) && (parent.size() > 0))
            {
                table_match.pop();
                parent.pop();
                if (parent.size() > 0)
                {
                    tree = parent.top();
                }
                else
                {
                    tree = nullptr;
                    break;
                }
            }

            if (tree == nullptr)
            {
                if (table_match.size() > 0)
                {
                    std::cerr << "invalid DOM tree (table)" << std::endl;
                    table_match.pop();
                }
                if (parent.size() > 0)
                {
                    std::cerr << "invalid DOM tree (table)" << std::endl;
                    parent.pop();
                }
                break;
            }
            else
            {
                tree = tree->pSiblings;
                source_hit = table_match.top();

                if (table_match.size() > 0)
                {
                    table_match.pop();
                }
                else
                {
                    std::cerr << "invalid DOM tree (table)" << std::endl;
                    break;
                }

                if (parent.size() > 0)
                {
                    parent.pop();
                }
                else
                {
                    std::cerr << "invalid DOM tree (parent)" << std::endl;
                    break;
                }
            }
        }
        flags[tree->content.tag] -= flags[tree->content.tag] > 0 ? 1u : 0u;
    }
    if (latest_hit != std::string::npos)
        new_content.append(content, latest_hit);

    const auto a = overall_number_of_lines.find(0);
    const size_t number_of_processed_lines = (a != overall_number_of_lines.cend()) ? overall_number_of_lines.size() - a->second : overall_number_of_lines.size();

    stats.No_Covered_Lines = dict_covered_lines.size();
    stats.No_Uncovered_Lines = dict_uncovered_lines.size();
    stats.No_Accepted_Uncovered_Lines = dict_accepted_uncovered_lines.size();
    stats.No_Remaining_Uncovered_Lines = dict_remaining_uncovered_lines.size();
    stats.No_Annotated_Lines = dict_accepted_uncovered_lines.size();
    stats.No_Overall_Lines = stats.No_Covered_Lines + stats.No_Uncovered_Lines;

    if (number_of_processed_lines)
    {
        stats.percentange_uncovered_lines = static_cast<float>(stats.No_Uncovered_Lines) / static_cast<float>(number_of_processed_lines);
        stats.percentange_covered_lines = static_cast<float>(stats.No_Covered_Lines) / static_cast<float>(number_of_processed_lines);
        stats.percentange_annotated_and_covered_lines = static_cast<float>(stats.No_Uncovered_Lines - stats.No_Annotated_Lines) / static_cast<float>(number_of_processed_lines);
    }
    return true;
}

static bool write_html_file(std::filesystem::path const& file_name, const std::string& content)
{
    bool retVal = true;
    std::ofstream out(file_name);
    if (out.is_open())
    {
        try
        {
            out.write(&content[0], content.length());
        }
        catch (const std::exception& ex) {
            std::cerr << "Could not create file. "
                << ex.what();
            retVal = false;
        }
    }
    else
    {
        std::cerr << "Unable to create HTML file." << std::endl;
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          visit every file and extract content
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool build_relative_paths(const std::filesystem::path& root, std::vector<ST_TextAttributes>& attributes)
{
    //const std::filesystem::path path = std::filesystem::current_path();
    //const std::string rel_path = path.string();
    const std::filesystem::path& rel_path = root;
    for (auto& i : attributes)
    {
        if (i.references.find("href") != i.references.cend())
        {
            const std::filesystem::path path = std::filesystem::path(rel_path / i.references["href"]).lexically_normal();
            std::cout << path << std::endl;
            if (std::filesystem::is_regular_file(path))
            {
                i.references["rel_file_path"] = path.generic_string();
                i.crlf = false;
            }
            else
            {
                i.crlf = true;
            }
        }
        else
        {
            i.crlf = true;
        }
    }
    attributes.erase(std::remove_if(attributes.begin(), attributes.end(), [](ST_TextAttributes& att)->bool {return att.crlf==true; }), attributes.end());
    return true;
}

/*------------------------------------------------------------------------------------------*//**
\brief          visit every file and extract content
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool dive_into_folder(const std::filesystem::path& root_folder, std::vector<ST_TextAttributes>& attributes)
{
    bool retVal = false;
    
    const auto normalize_path = [](const std::string& value) -> const std::filesystem::path
        {
            return std::filesystem::path(value).lexically_normal().generic_string();
        };

    try {
        for (auto& i : attributes)
        {
            if (i.references.find("rel_file_path") != i.references.cend() && i.references["rel_file_path"] != "")
            {
                std::filesystem::path rel_path = std::filesystem::path(i.references["rel_file_path"]);
                std::string file_name = rel_path.filename().string();
                
                for (const auto& entry : std::filesystem::recursive_directory_iterator(root_folder)) {
                    std::filesystem::path filename = normalize_path(entry.path().filename().string());
                    if (entry.is_regular_file() && entry.path().filename() == file_name) {
                        std::string file_id = std::filesystem::relative(entry.path()).string();

                        const std::string html_ref = rel_path.relative_path().generic_string();
                        const std::string file_ref = std::filesystem::path(entry.path()).generic_string();
                        if (file_ref.find(html_ref) != std::string::npos)
                        {
                            std::cout << "Found: " << file_ref << std::endl;
                            i.references["src_file"] = file_ref;
                        }
                    }
                }
            }
        }
        retVal = true;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          visit every file and extract content
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool load_source_code(std::vector<ST_TextAttributes>& attributes, std::unordered_map<std::string, std::vector<std::string>>& src_code)
{
    bool retVal = true;
    src_code.clear();
    for (auto& i : attributes)
    {
        if (i.references.find("src_file") != i.references.cend())
        {
            std::string& src_file = i.references["src_file"];
            try {
                std::ifstream file(src_file, std::ios::binary);
                if (file.good())
                {
                    std::string content{
                        std::istreambuf_iterator<char>{file},
                        std::istreambuf_iterator<char>{}
                    };

                    std::vector<std::string> source_code;
                    static const std::string line_start("<tr>");
                    static const std::string line_terminator("</tr>");
                    static const size_t preflen = line_start.size();
                    static const size_t postlen = line_terminator.size();
                    
                    size_t prefix = content.find(line_start, 0);
                    size_t postfix = content.find(line_terminator, prefix);
                    size_t appendix = 0u;
                    if (prefix != std::string::npos)
                    {
                        source_code.push_back(content.substr(0, prefix));
                        while (postfix != std::string::npos)
                        {
                            appendix = postfix + postlen;
                            source_code.push_back(content.substr(prefix, appendix-prefix));
                            prefix = content.find(line_start, postfix);
                            postfix = content.find(line_terminator, prefix);
                        }
                        if (prefix == std::string::npos)
                        {
                            source_code.push_back(content.substr(appendix));
                        }
                        else
                        {
                            retVal = false;
                            std::cerr << "Invalid file format." << std::endl;
                        }
                    }
                    else
                    {
                        retVal = false;
                        std::cerr << "Invalid file content" << std::endl;
                    }
                    file.close();
                    if (retVal)
                    {
                        src_code[src_file]=source_code;
                    }
                }
                else
                {
                    retVal = false;
                    std::cerr << "Error while loading " << src_file << std::endl;
                }
            }
            catch (const std::ifstream::failure& e) {
                std::cerr << "Exception opening/reading file. " << e.what() << " Error code: " << e.code() << std::endl;
                retVal = false;
            }
        }
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          traverses the index file and returns the table
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool load_html_code(const std::filesystem::path& root, std::vector<ST_TextAttributes>& attributes, std::unordered_map<std::string, ST_HTMLContent>& source_tree)
{
    bool retVal = true;
    source_tree.clear();
    for (auto& i : attributes)
    {
        if (!i.references.empty() && i.references.find("rel_file_path") != i.references.cend())
        {
            std::smatch match;
            std::regex quotationmarks_removal(R"(^[\'|\"](.*)[\'|\"]$)");
            std::string abs_file_path = i.references["href"];
            bool adapted_content = std::regex_search(abs_file_path, match, quotationmarks_removal);
            if (adapted_content == true)
            {
                abs_file_path = match[1].str();
            }
            const std::filesystem::path root_file = (root / abs_file_path).lexically_normal();
            std::cout << "\033[1;32mProcessing: \033[0m" << root_file << std::endl;
            std::string content;
            retVal &= open_html_file(root_file, content);
            if (retVal)
            {
                std::string file_id = i.references["src_file"];
                source_tree[file_id].html = std::move(content);
                retVal &= create_DOM_tree(source_tree[file_id].html, &source_tree[file_id].ptree);
            }
        }
    }

    return retVal;
}

static std::unordered_set<std::string> intersect(std::unordered_set<std::string>& first, std::unordered_set<std::string>& second)
{
    std::unordered_set<std::string> retVal;
    for (const auto& i : first)
    {
        if (second.find(i) != second.cend())
        {
            retVal.insert(i);
        }
    }
    return retVal;
}

static std::unordered_set<std::string> unite(std::unordered_set<std::string>& first, std::unordered_set<std::string>& second)
{
    std::unordered_set<std::string> retVal = second;
    //retVal.insert(first);
    for (const auto& i : first)
    {
        retVal.insert(i);
    }
    return retVal;
}


static bool save_file(std::filesystem::path const& file_name, std::string& content)
{
    bool retVal = false;
    std::ofstream file(file_name);
    try {
        if (file.good())
        {
            file << content;
            retVal = true;
        }
    }
    catch (const std::ios_base::failure& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return retVal;
}

static bool save_file(std::string file_name, std::vector<std::string> lines)
{
    bool retVal = false;
    std::ofstream file(file_name);
    try {
        if (file.good())
        {
            for (const auto& i : lines)
            {
                file << i << std::endl;
            }
            retVal = true;
        }
    }
    catch (const std::ios_base::failure& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          dive into the project folder and retreive all files with the given file endings
\
\param[in]      root folder of the project (typically where the vcxproj is located),
\param[in]      file endings
\param[out]     list of all files with the given file endings
\return         true/false
*//*-------------------------------------------------------------------------------------------*/
bool retrieve_list_of_project_files(const std::filesystem::path& root_folder, std::span<const std::string_view> const extensions, std::span<const std::string_view> const exclusions, std::vector<std::filesystem::path>& file_list)
{
	bool retVal = true;
    std::error_code ec;
	try {
        std::filesystem::directory_iterator iterator{ root_folder, std::filesystem::directory_options::skip_permission_denied, ec };
        std::filesystem::directory_iterator end{};
		
		while(iterator != end) 
        {
			if (iterator->is_regular_file()) {
                std::filesystem::path file_extension = iterator->path().extension();
                //std::cout << "Filename " << iterator->path() << std::endl;
                for (const auto& ext : extensions)
                {
                    if (file_extension == ext)
                    {
                        file_list.push_back(iterator->path().lexically_normal());
                        break;
                    }
				}
			}
            else
            {
                if (iterator->is_directory())
                {
                    bool exclude = false;
                    const auto path = iterator->path().filename();
                    for (const auto& ex : exclusions)
                    {
                        if (ex == path)
                        {
                            exclude = true;
                            break;
                        }
                    }
                    if (exclude == false)
                    {
                        retVal = retrieve_list_of_project_files(iterator->path(), extensions, exclusions, file_list) && retVal;
                    }
                }
            }
            ec.clear();
            iterator.increment(ec);
		}
	}

	catch (const std::filesystem::filesystem_error& e) {
        retVal = false;
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          Find the best matching source file for a given file reference.
\
\param[in]      reference   File reference to match.
\param[in]      list        List of possible source files.
\param[out]     match       Best matching source file, if at least one candidate is found.
\return         EN_exact if the source file can be uniquely identified,
                EN_ambiguous if a fallback candidate is returned,
                EN_not_found if no candidate is found.
*//*-------------------------------------------------------------------------------------------*/

enum class EN_SourceFileMatch
{
    EN_not_found,  // No candidate with matching html_ref_filename
    EN_ambiguous,  // Best candidate returned, but not uniquely resolved
    EN_exact       // Candidate uniquely resolved
};

EN_SourceFileMatch get_source_file(std::filesystem::path const& reference, std::vector<std::filesystem::path> const& list, std::filesystem::path& match)
{
    EN_SourceFileMatch retVal;
	match.clear();
	const auto file_name_to_match = reference.filename();
	std::vector<std::pair<std::filesystem::path, std::filesystem::path>> condensed_list;
	for (const auto &i : list)
	{
		const std::filesystem::path normalized_path = i.lexically_normal();
		if (normalized_path.filename() == file_name_to_match)
			condensed_list.emplace_back(normalized_path, normalized_path);
	}
	if (condensed_list.empty())
	{
		retVal = EN_SourceFileMatch::EN_not_found;
	}
	else
	{
		if (condensed_list.size() > 1u)
		{
			std::filesystem::path ref_counter = reference.lexically_normal();
            std::filesystem::path previous_ref_counter = ref_counter;
			bool break_loop = false;
			do
			{
				match = condensed_list.front().first;
				for (auto it = condensed_list.cbegin(); it != condensed_list.cend(); )
				{
					if (it->second.filename() != ref_counter.filename())
					{
						it = condensed_list.erase(it);
					}
					else
					{
						++it;
					}
				}
				if (condensed_list.size() > 1u)
				{
					for (auto& i : condensed_list)
					{
						i.second = i.second.parent_path();
					}
					ref_counter = ref_counter.parent_path();
				}
				else
				{
					break_loop = true;
				}
				break_loop = break_loop || (previous_ref_counter == ref_counter);
                previous_ref_counter = ref_counter;
			} while (!break_loop);
            
            if (!condensed_list.empty())
            {
                match = condensed_list.front().first;
                retVal = condensed_list.size() > 1u
                    ? EN_SourceFileMatch::EN_ambiguous
                    : EN_SourceFileMatch::EN_exact;
            }
            else
            {
				retVal = EN_SourceFileMatch::EN_ambiguous;
            }
		}
        else
        {
            match = condensed_list.front().first;
            retVal = EN_SourceFileMatch::EN_exact;
        }
	}
	return retVal;
}

static bool save_and_annotate_source_code(ST_Command_Line_Options const& parameter, std::unordered_map<std::string, std::vector<std::string>>& source, std::unordered_map<std::string, ST_HTMLContent>& tree)
{
	bool retVal = true;
	const bool save_code_as_different_file = parameter.sourcefile_ext.size() != 0u;
	const std::string& src_extension = parameter.sourcefile_ext;

    std::vector<std::filesystem::path> file_list;
	constexpr std::string_view extensions[] = { ".cpp", ".c", ".h", ".hpp" };
    constexpr std::string_view exclusions[] = { ".github", ".visuallint", ".vs", "gmock", "gtest", "gnuplot-iostream" };
    retVal = retrieve_list_of_project_files(parameter.project_folder, extensions, exclusions, file_list);

	for (auto& tree_pos : tree)
	{
		ST_TABLE_ELEMENT* ptable;
		std::vector<ST_TextAttributes> attrib;
		retVal &= build_full_dom_tree(tree[tree_pos.first], ptable, attrib);
		//show_all_nodes(0, ptable);
		if (retVal == true)
		{
			ST_TABLE_ELEMENT* table_node = findtag(HTML_TAG::EN_TABLE, ptable);
			if (table_node)
			{
				std::vector<ST_TABLE_ELEMENT*> line_numbers;
				std::vector<ST_TABLE_ELEMENT*> uncovered_code;
				std::vector<ST_TABLE_ELEMENT*> covered_code;
				std::vector<ST_TABLE_ELEMENT*> count;
				std::vector<ST_TABLE_ELEMENT*> code;
				std::vector<ST_TABLE_ELEMENT*> uncovered_branch;
				findallattributes("class", "line-number", ptable, line_numbers);
				findallattributes("class", "covered-line", ptable, covered_code);
				findallattributes("class", "uncovered-line", ptable, uncovered_code);
				//findallattributes("class", "red", ptable, uncovered_branch);
				findallattributes("class", "code", ptable, code);

				std::unordered_set<std::string> pure_uncovered_lines;
				for (const auto& i : uncovered_code)
				{
					//static const std::regex zeropattern = std::regex(R"0");
					//std::smatch is_zero;
					if (i->content == "0")
					{
						if (i->pNext)
						{
							const auto extract = [](std::string& content) -> std::string
								{
									static const std::regex extract(R"(\s*<\s*span\s+class\s*=\s*(['"])(.*?)\1\s*>(.*?)<\s*/span\s*>)");
									std::smatch match;
									bool found = std::regex_search(content, match, extract);
									if (found)
									{
										return match[3].str();
									}
									return content;
								};

							std::string code_content = extract(i->pNext->content);
							static const std::regex reject_as_uncovered(R"(^\s*(?:else\s*[{}]*\s*(?://.*)?|do\s*[{}]*\s*(?://.*)?|break\s*;\s*(?://.*)?|[{}]\s*(?://.*)?|.*//\s*ccov:\s*A[\dE].*)?$)");
							if (!std::regex_match(code_content, reject_as_uncovered))
							{
								//std::cout << "Uncovered line: " << tree_pos->pPrev->content << std::endl;
								pure_uncovered_lines.insert(i->pPrev->content);
							}
						}
					}
				}
				const auto src_iterator = source.find(tree_pos.first);
				if (src_iterator != source.cend())
				{
					std::string sourcfile_name = save_code_as_different_file == false ? src_iterator->first : src_iterator->first + src_extension;
					//retVal &= save_file(sourcfile_name, src_iterator->second);
					std::vector<size_t> output;
					output.reserve(pure_uncovered_lines.size());
					std::transform(pure_uncovered_lines.begin(), pure_uncovered_lines.end(), std::back_inserter(output), [](const std::string& s) -> size_t
						{
							size_t value{};
							const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
							if ((ec == std::errc{}) && (ptr == s.data() + s.size()))
							{
								return value;
							}
							else
							{
								return 0;
							}
						});

					std::sort(output.begin(), output.end());
					output.erase(
						std::unique(output.begin(), output.end()),
						output.end());

					if (output.size() > 0)
					{
						const auto cpp_filename = [](const std::string& file_name) -> std::string
							{
								std::smatch match;
								static const std::regex cpp_absolute_filepath_and_name(
									R"((.*)\.html?$)"
								);
								if (std::regex_match(file_name, match, cpp_absolute_filepath_and_name))
								{
									return match[1].str();
								}
								else
								{
									return file_name;
								}
							};

						const std::string html_ref_filename = cpp_filename(tree_pos.first);
						std::filesystem::path related_project_file;
						const auto file_match_found = get_source_file(html_ref_filename, file_list, related_project_file);
						const auto assign_file_name = [](EN_SourceFileMatch match, const std::string& html_reference, const std::filesystem::path& file_path) -> std::string
							{
								switch (match)
								{
								case EN_SourceFileMatch::EN_exact:
                                case EN_SourceFileMatch::EN_ambiguous:
									return file_path.generic_string();
								case EN_SourceFileMatch::EN_not_found:
								default:
									return html_reference;
								}
							};
                        const auto filename = assign_file_name(file_match_found, html_ref_filename, related_project_file);
                        if (output.front() == 0u)
                        {
                            std::cout << "Line/parsing error occurred. Affected source locations:" << std::endl;

                            for (auto uncovered_lines = output.cbegin() + 1u;
                                uncovered_lines != output.cend();
                                ++uncovered_lines)
                            {
                                std::cout
                                    << "\t" << filename << "(" << *uncovered_lines << ") : "
                                    << "warning CCOV002: uncovered line; "
                                    << "annotation candidate: // ccov: Ax"
                                    << std::endl;
                            }
                        }
						else
						{
							for (const auto uncovered_lines : output)
							{
								std::cout
									<< filename << "(" << uncovered_lines << ") : "
									<< "warning CCOV002: uncovered line; "
									<< "annotation candidate: // ccov: Ax" << std::endl;
							}
						}
					}
				}
			}
			if (retVal == false)
				break;
		}
	}
	return retVal;
}

static bool patch_html_file(std::filesystem::path const& filename, const std::string& content, ST_Statistics& stats)
{
    ST_DOMTREE* pDomTree = nullptr;
    std::string new_content;
    
    //std::regex stylesheet(R"(href='[./\\]*style.css')");
    //std::string adapted_content = std::regex_replace(content, stylesheet, "href='./style.css'");
    std::string adapted_content = content;
    bool retVal = create_DOM_tree(adapted_content, &pDomTree);
    if (retVal == true)
    {
        create_annotated_html(adapted_content, new_content, stats, pDomTree);
        retVal = write_html_file(filename, new_content);
        if (pDomTree != nullptr)
        {
            delete pDomTree;
        }
    }
    return retVal;
}

static bool save_and_annotate_html_files(std::filesystem::path const &root, ST_Command_Line_Options const& parameter, ST_DocumentCollectionContent& source)
{
    bool retVal = true;
    const bool save_code_as_different_file = parameter.sourcefile_ext.size() > 0u;
    const std::string src_extension = parameter.sourcefile_ext;
    const std::string annotated_index_file = parameter.annotated_index_file.string();
    const std::string annotated_html_folder = parameter.annotated_html_folder.string();
    size_t file_no = 0;
    //unordered_map<std::string, > statistics;
    std::unordered_map<std::string, ST_Statistics> statistics;
    for (auto& tree_pos : source.attributes)
    {
        const auto iterator = tree_pos.references.find("href");
        if (iterator != tree_pos.references.cend())
        {
            std::smatch match;
            std::regex quotationmarks_removal(R"(^[\'\"](.*)[\'\"]$)");

            std::string abs_file_path = iterator->second;
            bool adapted_content = std::regex_search(abs_file_path, match, quotationmarks_removal);
            if (adapted_content == true)
            {
                abs_file_path = match[1].str();
            }

            if (auto file_location = tree_pos.references.find("src_file"); file_location != tree_pos.references.cend())
            {
                std::string content;

                retVal = open_html_file(file_location->second, content);
                if (retVal == true)
                {
                    if (auto html_file_reference = tree_pos.references.find("href"); html_file_reference != tree_pos.references.cend())
                    {
                        ST_Statistics stats;
                        std::filesystem::path file_name = file_location->second;// std::filesystem::path(html_file_reference->second);
                        //const auto filename_iterator = tree_pos.references.find("src_file");
                        const std::string file_type = file_name.stem().extension().string();
                        if (file_type == ".cpp" || file_type == ".h" || file_type == ".c")
                        {
                            const bool is_header = file_type == ".h";
                            std::string annotated_filename;
                            if (is_header)
                            {
                                annotated_filename = file_name.stem().stem().string() + "_anno_h.html";
                            }
                            else if (file_type == ".cpp")
                            {
                                annotated_filename = file_name.stem().stem().string() + "_anno_cpp.html";
                            }
                            else
                            {
                                annotated_filename = file_name.stem().stem().string() + "_anno_c.html";
                            }
                            stats.file_name = annotated_filename;
                            const std::filesystem::path href_file_path = html_file_reference->second;
                            stats.file_anchor = (href_file_path.parent_path() / annotated_filename).lexically_normal().string();
                        }
                        else
                        {
                            std::stringstream alternative_file_name;
                            alternative_file_name << "undefined_anno_" << fileno << ".html";
                            tree_pos.references["annotated_src"] = alternative_file_name.str();
                            stats.file_name = alternative_file_name.str();
                            const std::filesystem::path href_file_path = html_file_reference->second;
                            stats.file_anchor = (href_file_path.parent_path() / file_name).lexically_normal().string();
                        }

                        const std::filesystem::path physical_file_location = file_location->second;
                        retVal = patch_html_file((physical_file_location.parent_path() / file_name).lexically_normal(), content, stats);
                        if (retVal == true)
                        {
                            statistics[tree_pos.references["href"]] = stats;
                        }
                        else
                        {
                            retVal = false;
                            break;
                        }
                    }
                    else
                    {
                        retVal = false;
                        break;
                    }
                }
                else
                {
                    std::cerr << "Unable to resolve href in html file: " << tree_pos.text << std::endl;
                }
            }
            else
            {
                std::cerr << "Unable to resolve source file: " << tree_pos.text << std::endl;
            }
        }
        file_no++;
    }

    if (retVal == true)
    {
        ST_DOMTREE* tree = findtag(HTML_TAG::EN_TABLE, source.index_file.ptree);
        if (tree != nullptr)
        {
            ST_DOMTREE* base_node = findtag(HTML_TAG::EN_TABLE_ROW, tree);
            if (base_node != nullptr)
            {
                ST_DOMTREE* table_root_node = base_node;//header of table
                size_t file_column_no = 0;
                std::string new_index_file;
                size_t last_pos = 0u;
                if (table_root_node != nullptr)
                {
                    ST_DOMTREE* reference_file_node = table_root_node->pChild;
                    ST_DOMTREE* column_node = reference_file_node;
                   
                    size_t file_column_no_counter = 0;
                    while (column_node->pSiblings != nullptr) { 
                        static const std::string column_name = std::string("Filename");
                        if (column_node->content.tag == HTML_TAG::EN_TABLE_COLUMN)
                        {
                            static const std::regex subattribute(R"(<td[^>]+>Filename</td>)");
                            std::smatch match_subattributes;
                            size_t length = column_node->content.terminating_pos == std::string::npos ? std::string::npos : column_node->content.terminating_pos - column_node->content.matching_pos;
                            std::string attrib = source.index_file.html.substr(column_node->content.matching_pos, length);
                            bool sub_attributes = std::regex_match(attrib, subattribute);
                            if (sub_attributes)
                            {
                                file_column_no = file_column_no_counter;
                                break;
                            }
                            ++file_column_no_counter;
                        }
                        column_node = column_node->pSiblings; 
                    }
                    //column_node = reference_file_node;
                    while (column_node->pSiblings != nullptr) {column_node = column_node->pSiblings;}
                    if (column_node)
                    {
                        static const std::string new_column = "<td class=\"column-entry-bold\">Annotated Line Coverage</td><td class=\"column-entry-bold\">Annotated file</td>";
                        new_index_file = source.index_file.html.substr(0, column_node->content.terminating_pos) + new_column;
                        ST_DOMTREE* base_node_next = base_node->pSiblings;
                        last_pos = column_node->content.terminating_pos;
                        table_root_node = table_root_node->pSiblings;
                    }
                }
                else
                {
                    retVal = false;
                }
                ST_Statistics overall_stats;
                overall_stats.No_Overall_Lines = 0;
                overall_stats.No_Covered_Lines = 0;
                overall_stats.No_Uncovered_Lines = 0;
                overall_stats.No_Accepted_Uncovered_Lines = 0;
                overall_stats.No_Remaining_Uncovered_Lines = 0;
                overall_stats.No_Annotated_Lines = 0;
                overall_stats.percentange_annotated_and_covered_lines = 0.f;
                overall_stats.percentange_covered_lines = 0.f;
                overall_stats.percentange_uncovered_lines = 0.f;

                const auto flush_statistics = [](const ST_Statistics& stats)->std::string
                {
                    const size_t overall_lines = stats.No_Overall_Lines;
                    const size_t No_Remaining_Uncovered_Lines = stats.No_Remaining_Uncovered_Lines;
                    const size_t covered_lines = overall_lines - No_Remaining_Uncovered_Lines;
                    std::string color = (No_Remaining_Uncovered_Lines == 0) ? "green" : (static_cast<float>(covered_lines) / static_cast<float>(overall_lines)) > 0.5 ? "yellow" : "red";
                    std::stringstream ss;
                    const auto old_precision = ss.precision();
                    ss << "<td class=\"column-entry-" << color << "\"><pre>" << std::fixed << std::setprecision(2) << (static_cast<float>(covered_lines) / static_cast<float>(overall_lines)) * 100 << "% (" << covered_lines << "/" << overall_lines << ")</pre></td>" << std::setprecision(old_precision);
                    return ss.str();
                };

                while (table_root_node != nullptr)
                {
                    bool totaling_row = false;
                    std::unordered_map<std::string, std::string> attributes;
                    ST_DOMTREE* column_node = table_root_node->pChild;
                    if (column_node != nullptr)
                    {
                        for (size_t tree_pos = 0; tree_pos < file_column_no; ++tree_pos)
                        {
                            if (column_node->pSiblings != nullptr)
                            {
                                column_node = column_node->pSiblings;
                            }
                        }
                        if (column_node->pSiblings != nullptr)
                        {
                            static const std::regex subattribute(R"(<\s*pre\s*>\s*<\s*a\s+([^>]+)>\s*([^<]+)\s*<\s*/a\s*>\s*<\s*/pre\s*>)");
                            std::smatch match_subattributes;

                            size_t length = column_node->content.terminating_pos == std::string::npos ? std::string::npos : column_node->content.terminating_pos - column_node->content.matching_pos;
                            std::string attrib = source.index_file.html.substr(column_node->content.matching_pos, length);
                            bool sub_attributes = std::regex_search(attrib, match_subattributes, subattribute);
                            if (sub_attributes)
                            {
                                static const std::regex attrib_pattern(
                                    R"(([^\s=]+)\s*=\s*(['"])(.*?)\2)"
                                );
                                if (sub_attributes == true)
                                {
                                    std::string content = match_subattributes[2].str();
                                    std::string attribute = match_subattributes[1].str();
                                    std::smatch attr_match;

                                    std::string::const_iterator searchStart = attribute.cbegin();
                                    while (std::regex_search(searchStart, attribute.cend(), attr_match, attrib_pattern)) {
                                        attributes[attr_match[1].str()] = attr_match[3].str();
                                        searchStart = attr_match.suffix().first;
                                    }
                                }
                            }
                            else
                            {
                                static const std::regex totals_match(R"(<\s*td\s*>\s*<\s*pre\s*>\s*Totals\s*<\s*/pre\s*>\s*<\s*/td\s*>)");
                                bool totals = std::regex_match(attrib, totals_match);
                                if (totals == true)
                                {
                                    attributes["href"] = "Totals"; 
                                    totaling_row = true;
                                }
                                else
                                {
                                    retVal = false;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            retVal = false;
                            break;
                        }
                        if (totaling_row == false)
                        {
                            while (column_node->pSiblings != nullptr) column_node = column_node->pSiblings;
                            new_index_file += source.index_file.html.substr(last_pos, column_node->content.terminating_pos - last_pos);
                            last_pos = column_node->content.terminating_pos;
                            
                            const auto href = attributes.find("href");
                            if (href != attributes.cend())
                            {
                                const auto statref = statistics.find(href->second);
                                if (statref != statistics.cend())
                                {
                                    size_t overall_lines = statref->second.No_Overall_Lines;
                                    size_t Covered_Lines = statref->second.No_Covered_Lines;
                                    size_t Uncovered_Lines = statref->second.No_Uncovered_Lines;
                                    size_t Accepted_Uncovered_Lines = statref->second.No_Accepted_Uncovered_Lines;
                                    size_t Remaining_Uncovered_Lines = statref->second.No_Remaining_Uncovered_Lines;
                                    size_t Annotated_Lines = statref->second.No_Annotated_Lines;
                                    
                                    overall_stats.No_Overall_Lines += overall_lines;
                                    overall_stats.No_Covered_Lines += Covered_Lines;
                                    overall_stats.No_Uncovered_Lines += Uncovered_Lines;
                                    overall_stats.No_Accepted_Uncovered_Lines += Accepted_Uncovered_Lines;
                                    overall_stats.No_Remaining_Uncovered_Lines += Remaining_Uncovered_Lines;
                                    overall_stats.No_Annotated_Lines += Annotated_Lines;
                                    
                                    new_index_file += flush_statistics(statref->second);
                                    std::stringstream file_linkage;
                                    //const auto file_identifier = source.attributes.;
                                    //if (file_identifier)

                                    file_linkage << "<td><pre><a href=\"" << statref->second.file_anchor << "\">" << statref->second.file_name << "</a></pre></td>";
                                    new_index_file += file_linkage.str();
                                }
                            }
                        }
                        else
                        {
                            while (column_node->pSiblings != nullptr) column_node = column_node->pSiblings;
                            new_index_file += source.index_file.html.substr(last_pos, column_node->content.terminating_pos - last_pos);
                            last_pos = column_node->content.terminating_pos;
                            new_index_file += flush_statistics(overall_stats);
                            new_index_file += "<td><pre></pre></td>";
                        }
                    }
                    else
                    {
                        retVal = false;
                        break;
                    }
                    table_root_node = table_root_node->pSiblings;
                }
                if (retVal == true)
                {
                    static const std::string annotation_remarks = ST_HTML_ANNOTATION::GetContent();
                    tree = findtag(HTML_TAG::EN_HEADING, tree);
                    if (tree != nullptr)
                    {
                        const size_t length = (tree->content.matching_pos != std::string::npos) && (tree->content.matching_pos > last_pos)? (tree->content.matching_pos - last_pos) : std::string::npos;
                        if (length != std::string::npos)
                        {
                            new_index_file += source.index_file.html.substr(last_pos, length);
                            new_index_file += annotation_remarks;
                            const size_t length = (tree->content.terminating_pos != std::string::npos) && (tree->content.terminating_pos > tree->content.matching_pos) ? (tree->content.terminating_pos - last_pos) : std::string::npos;
                            new_index_file += source.index_file.html.substr(tree->content.matching_pos, length);
                            if (length != std::string::npos)
                            {
                                new_index_file += CClangOneVersion::GetVersion();
                                new_index_file += source.index_file.html.substr(tree->content.terminating_pos);
                            }
                            
                        }
                        else
                        {
                            new_index_file += source.index_file.html.substr(last_pos);
                        }
                    }
                    else
                    {
                        new_index_file += source.index_file.html.substr(last_pos);
                    }
                   
                   
                    std::filesystem::path dest_file;
                    if (annotated_index_file.size())
                    {
                        dest_file = root / annotated_index_file;
                    }
                    else
                    {
                        dest_file = root / "annotated_index.html";
                    }
                    
                    retVal = save_file(dest_file, new_index_file);
                }
            }
        }
    }
    return retVal;
}

static bool load_index_file(std::filesystem::path file_name, ST_HTMLContent& tree_content)
{
    bool retVal = false;

    std::ifstream file(file_name);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
    }
    else
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        tree_content.html = buffer.str();

        if (create_DOM_tree(tree_content.html, &tree_content.ptree)) {
            //std::cout << "HTML content parsed successfully" << std::endl;
            retVal = true;
        }
        else {
            std::cerr << "Failed to parse HTML content" << std::endl;
        }
    }
    return retVal;
}

static bool create_index_table(ST_HTMLContent& parser, ST_TABLE_ELEMENT *proot, std::vector<ST_TextAttributes>& attrib)
{
    bool retVal = false;
    attrib.clear();

    static const std::string column_name = std::string("Filename");
    ST_TABLE_ELEMENT* table = findcontent(column_name, proot);
    if (table != nullptr)
    {
        if (table->pParent)
        {
            size_t column_number = 0;
            ST_TABLE_ELEMENT* row = table->pParent;
            if (row->pSub != nullptr)
            {
                ST_TABLE_ELEMENT* column_seeker = row->pSub;
                while ((column_seeker != nullptr) && (column_seeker != table))
                {
                    column_number++;
                    column_seeker = column_seeker->pNext;
                }
                if (column_seeker != nullptr)
                {
                    ST_TABLE_ELEMENT* table_line = table->pParent->pNext;
                    
                    while (table_line)
                    {
                        size_t x = column_number;
                        ST_TABLE_ELEMENT* col = table_line->pSub;
                        while (col)
                        {
                            if (x--==0)
                            {
                                ST_TextAttributes at;
                                at.text = col->content;
                                at.references = col->attributes;
                                at.crlf = false;
                                attrib.push_back(at);
                                break;
                            }
                            col = col->pNext;
                        }
                        table_line = table_line->pNext;
                    }
                    retVal = true;
                }
            }
        }
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          visit every file and extract content
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool check_index_file(ST_HTMLContent& parser, ST_TABLE_ELEMENT *&ptable, std::vector<ST_TextAttributes>& attrib)
{
    bool retVal = true;
    attrib.clear();
    if (true/*ptable != nullptr*/)
    {
        retVal = build_full_dom_tree(parser, ptable, attrib);
        if (retVal == true)
        {
            retVal = false;
            ST_TABLE_ELEMENT* top = ptable;
            if ((top != nullptr) && (top->tag == HTML_TAG::EN_HTML))
            {
                static const std::string IdentStrings[] =
                {
                    "Filename", "Function Coverage", "Line Coverage" /*, "Region Coverage", "Branch Coverage"*/
                };
                if (top->pSub != nullptr)
                {
                    if (top->pSub->content == "Coverage Report")
                    {
                        if (top->pSub->pNext != nullptr)
                        {
                            if (top->pSub->pNext->content.find("Created:") != std::string::npos)
                            {
                                ST_TABLE_ELEMENT* table = findtag(HTML_TAG::EN_TABLE, top);
                                if (table != nullptr)
                                {
                                    ST_TABLE_ELEMENT* row = findtag(HTML_TAG::EN_TABLE_ROW, table);
                                    if (row != nullptr)
                                    {
                                        ST_TABLE_ELEMENT* column = findtag(HTML_TAG::EN_TABLE_COLUMN, row);
                                        if (column != nullptr)
                                        {
                                            retVal = true;
                                            for (const auto& tree_pos : IdentStrings)
                                            {
                                                bool check = false;
                                                ST_TABLE_ELEMENT* find_table_headers = column;
                                                while (find_table_headers)
                                                {
                                                    if (tree_pos == find_table_headers->content)
                                                    {
                                                        check = true;
                                                        break;
                                                    }
                                                    find_table_headers = find_table_headers->pNext;
                                                }
                                                retVal = retVal && check;
                                            }

                                            if (retVal == true)
                                            {
                                                retVal = create_index_table(parser, row, attrib);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        retVal = false;
    }
    return retVal;
}

/*------------------------------------------------------------------------------------------*//**
\brief          traverses the index file and returns the table
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
bool build_node_tree(ST_Command_Line_Options const& parameter)
{
	std::string output;
	bool retVal = false;

	ST_DocumentCollectionContent content;
	std::unordered_map<std::string, std::vector<std::string>> source_code;

	const std::filesystem::path root = parameter.index_folder;
	retVal = load_index_file(root / parameter.index_file, content.index_file);
	if (retVal == true)
	{
		retVal = check_index_file(content.index_file, content.pIndexTableStruct, content.attributes);
		if (retVal == true)
		{
			build_relative_paths(root, content.attributes);
			retVal = content.attributes.size() > 0u;
			if (retVal == true)
			{
				retVal = dive_into_folder(parameter.index_folder.string(), content.attributes);
				if (retVal == true)
				{
					std::unordered_map<std::string, std::vector<std::string>> source_code;
					retVal = load_source_code(content.attributes, source_code);
					if (retVal == true)
					{
						std::unordered_map<std::string, ST_HTMLContent> source_tree;
						retVal = load_html_code(root, content.attributes, source_tree);
						if (retVal == true)
						{
							if (parameter.diagnostics == true)
							{
								retVal = save_and_annotate_source_code(parameter, source_code, source_tree);
								if (retVal == true)
								{
									std::cout << "Source files successfully annotated" << std::endl;
								}
							}

							if (parameter.annotate_html == true)
							{
								retVal = save_and_annotate_html_files(root, parameter, content);
								if (retVal == true)
								{
									std::cout << "HTML files successfully annotated" << std::endl;
								}
							}
						}
					}
				}
			}
			else
			{
				std::cerr << "Unable to annotate files. Files not found." << std::endl;
			}
		}
	}
	return retVal;
}