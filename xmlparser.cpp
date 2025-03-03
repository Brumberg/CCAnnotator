#include "xmlparser.h"
#include "gumbo.h"

#include <unordered_set>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <queue>
#include <array>
#include <stack>
#include <unordered_map>
#include <cassert>

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
    ~ST_DOMTREE() { if (pSiblings != nullptr) { delete pSiblings; pSiblings = nullptr; } if (pChild != nullptr) { delete pChild; pChild = nullptr; } }
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
        if (pNext != nullptr) { ST_TABLE_ELEMENT* p = pNext; pNext = nullptr; delete p; }
        if (pPrev != nullptr) { ST_TABLE_ELEMENT* p = pPrev; pPrev = nullptr; delete p; }
        if (pSub != nullptr) { ST_TABLE_ELEMENT* p = pSub; pSub = nullptr; delete p; }
        if (pParent != nullptr) { ST_TABLE_ELEMENT* p = pParent; pParent = nullptr; delete p; }
    }
    std::string content;
    std::unordered_map<std::string, std::string> attributes;
};

struct ST_HTMLContent
{
    std::string html;
    ST_DOMTREE* ptree;
};

struct ST_DocumentCollectionContent
{
    ST_HTMLContent index_file;
    std::unordered_map<std::string, ST_HTMLContent> source_base;
};

struct ST_Statistics
{
    size_t No_Uncovered_Lines;
    size_t No_Covered_Lines;
    size_t No_Annotated_Lines;
    float percentange_covered_lines;
    float percentange_uncovered_lines;
    float percentange_annotated_and_covered_lines;
};


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
        std::cout << proot->content << std::endl;
        
        if (proot->pSub != nullptr)
        {
            findallattributes(attrib_name, attrib_value, proot->pSub, related_content);
        }

        proot = proot->pNext;
    }
}

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
        static const std::regex table_column_attributes = std::regex(R"(<\s*td\s+([^>]+).*<\s*/td\s*>)");
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

        static const std::regex attrib_pattern(R"(([^\s]+)\s*=\s*([^\s]+))");
        if (sub_attributes == true)
        {
            content = match_subattributes[2].str();
            std::string attribute = match_subattributes[1].str();
            std::smatch attr_match;

            std::string::const_iterator searchStart = attribute.cbegin();
            while (std::regex_search(searchStart, attribute.cend(), attr_match, attrib_pattern)) {
                attributes[attr_match[1].str()] = attr_match[2].str();
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
                static const std::regex enclosure_removal(R"(^[\'|\"](.*)[\'|\"]$)");
                std::string enclosed_attribute = attr_match[1].str();
                std::string enclosed_attribute_value = attr_match[2].str();
                if (std::regex_search(enclosed_attribute, enclosure_match, enclosure_removal))
                {
                    enclosed_attribute = enclosure_match[1].str();
                }
                if (std::regex_search(enclosed_attribute_value, enclosure_match, enclosure_removal))
                {
                    enclosed_attribute_value = enclosure_match[1].str();
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
static bool open_html_file(std::string source, std::string& content)
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
    std::unordered_map<size_t, size_t> dict_covered_lines;
    std::unordered_map<size_t, size_t> dict_uncovered_lines;
    std::unordered_map<size_t, size_t> dict_accepted_uncovered_lines;

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
            static const std::regex pattern(R"(//.*?&lt;cov\s+class=&apos;(A\d)&apos;&gt;(.*?)&lt;\s*/cov\s*&gt;)");
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
                const std::string exp = content.substr(tree->content.matching_pos, length);
                bool hit = std::regex_search(exp, match, pattern);
                if (hit == true)
                {
                    annotation_string = match[1].str();
                    additional_remarks = match[2].str();
                    annotation = annotation_string;
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
                    static const std::regex exception_code(R"(A[0-7])");
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
                        }
                    }

#ifdef APPENDCOLUMN //needs additional tweaking -> deleting remarks
                    found_start = std::regex_search(exp, match, expr_code);
                    if (found_start)
                    {
                        static const std::string additional_column_remarks_start = "<td class='remark'><pre>";
                        static const std::string additional_column_remarks_stop = "</pre></td>";
                        std::string dummy;
                        dummy.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        new_content.append(content, latest_hit, tree->content.terminating_pos - latest_hit);
                        latest_hit = tree->content.terminating_pos;
                        new_content.append(additional_column_remarks_start + additional_remarks + additional_column_remarks_stop);
                        additional_remarks = "";
                    }
#else
                    found_start = std::regex_search(exp, match, expr_code);
                    if (found_start)
                    {
                        if (annotated_code)
                        {
                            static const std::regex class_red(R"(<\s*span\s*class\s*=\s*'red')");
                            static const std::string class_cyan = "<span class='cyan'";
                            std::string dummy = content.substr(latest_hit, tree->content.terminating_pos - latest_hit);

                            dummy = std::regex_replace(dummy, class_red, class_cyan);
                            latest_hit = tree->content.terminating_pos;

                            std::smatch match;
                            static const std::regex pattern(R"(&lt;cov\s+class=&apos;(A\d)&apos;&gt;(.*?)&lt;\s*/cov\s*&gt;)");
                            const bool match_annotation = std::regex_search(dummy, match, pattern);
                            if (match_annotation)
                            {
                                std::string prefix_str(match.prefix().first, match.prefix().second);
                                std::string suffix_str(match.suffix().first, match.suffix().second);
                                dummy = prefix_str + suffix_str;
                                static const std::regex del_comment(R"(\s*//\s*(?=(<\s*/span\s*>)*<\s*/pre\s*><\s*/td\s*>))");
                                const bool del_comment_result = std::regex_search(dummy, match, del_comment);
                                if (del_comment_result)
                                {
                                    dummy = std::string(match.prefix().first, match.prefix().second);
                                    latest_hit = tree->content.terminating_pos;
                                }
                            }
                            new_content.append(dummy);
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
                                            ++dict_accepted_uncovered_lines[currently_processed_line];
                                    }
                                }
                            }
                        }
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

    stats.No_Covered_Lines = dict_covered_lines.size();
    stats.No_Annotated_Lines = dict_accepted_uncovered_lines.size();
    stats.No_Uncovered_Lines = dict_uncovered_lines.size();
    const size_t overall_number_of_lines = stats.No_Covered_Lines + stats.No_Uncovered_Lines;
    if (overall_number_of_lines)
    {
        stats.percentange_uncovered_lines = static_cast<float>(stats.No_Uncovered_Lines) / static_cast<float>(overall_number_of_lines);
        stats.percentange_covered_lines = static_cast<float>(stats.No_Covered_Lines) / static_cast<float>(overall_number_of_lines);
        stats.percentange_annotated_and_covered_lines = static_cast<float>(stats.No_Uncovered_Lines - stats.No_Annotated_Lines) / static_cast<float>(overall_number_of_lines);
    }
    return true;
}

static bool write_html_file(const std::string& file_name, const std::string& content)
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
static bool build_relative_paths(std::vector<ST_TextAttributes>& attributes)
{
    const std::filesystem::path path = std::filesystem::current_path();
    const std::string rel_path(reinterpret_cast<const char*>(path.c_str()));

    for (auto& i : attributes)
    {
        if (i.references.find("href") != i.references.cend())
        {
            std::string dummy = i.references["href"];
            size_t ix = dummy.find(rel_path);
            if (ix != std::string::npos)
            {
                static const std::string ending = ".html";
                size_t ending_ = dummy.find(ending);
                ending_ = ending_ == std::string::npos ? dummy.size() : ending_;
                dummy = dummy.substr(ix, ending_ - ix);
                i.references["rel_file_path"] = dummy;
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
static bool dive_into_folder(const std::string& root_folder, std::vector<ST_TextAttributes>& attributes)
{
    bool retVal = false;
   
    try {
        for (auto& i : attributes)
        {
            if (i.references.find("rel_file_path") != i.references.cend() && i.references["rel_file_path"] != "")
            {
                std::string rel_path = i.references["rel_file_path"];
                size_t ix = rel_path.find_last_of("/\\");
                std::string file_name = rel_path;
                if (ix != std::string::npos)
                { 
                    file_name = file_name.substr(ix+1u);
                }
                
                for (const auto& entry : std::filesystem::recursive_directory_iterator(root_folder)) {
                    std::string filename = entry.path().filename().string();
                    if (entry.is_regular_file() && entry.path().filename() == file_name) {
                        std::string file_id = std::filesystem::relative(entry.path()).string();

                        if (file_id.find(rel_path) != std::string::npos)
                        {
                            std::cout << "Found: " << entry.path().string() << std::endl;
                            i.references["src_file"] = entry.path().string();
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
                std::ifstream file(src_file);
                if (file.good())
                {
                    std::string line;
                    std::vector<std::string> source_code;
                    std::istream& ret = std::getline(file, line);
                    if (ret.good())
                    {
                        if (!line.empty() && line.back() == '\r') {
                            i.crlf = true;
                            //std::cout << "Line ends with \\r\\n" << std::endl;
                        }
                        else {
                            i.crlf = false;
                            //std::cout << "Line ends with \\n" << std::endl;
                        }
                        source_code.push_back(line);
                        while (std::getline(file, line)) {
                            source_code.push_back(line);
                        }
                    }
                    else
                    {
                        std::cerr << "Souce file is empty" << std::endl;
                    }
                    src_code[src_file] = source_code;
                    file.close();
                    retVal &= true;
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
static bool load_html_code(const std::string& root, std::vector<ST_TextAttributes>& attributes, std::unordered_map<std::string, ST_HTMLContent>& source_tree)
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
            const std::string root_file = root + abs_file_path;
            std::string content;
            retVal &= open_html_file(root_file, content);
            if (retVal)
            {
                std::string file_id = i.references["src_file"];
                source_tree[file_id].html = content;
                retVal &= create_DOM_tree(content, &source_tree[file_id].ptree);
            }
        }
    }

    return retVal;
}


static bool extract_metric(std::unordered_map<std::string, std::string> parameter, std::string exception_comment, std::unordered_set<std::string>& code_metric, std::vector<std::string>& code_container)
{
    bool retVal = true;
    const std::string exclude_line_from_code_coverage = parameter["cc_ignore"];

    for (auto& j : code_metric)
    {
        size_t line_no;
        try {
            std::stringstream a(j.c_str());
            a >> line_no;
            if (line_no > 0)
            {
                --line_no;
            }
            else
            {
                std::ios_base::failure e("Number of lines have to be larger 0.");
                throw e;
            }
        }
        catch (std::ios_base::failure& e)
        {
            std::cerr << e.what() << std::endl;
            retVal = false;
            break;
        }

        if (line_no < code_container.size())
        {
            std::string& code = code_container[line_no];
            struct EN_SupportedComments {
                std::string comment;
            };

            static const EN_SupportedComments comments[] =
            {
                "/*",
                "//"
            };

            bool comment_found = false;
            for (auto& comment_type : comments)
            {
                const size_t comment = code.find(comment_type.comment);
                if (comment != std::string::npos)
                {
                    comment_found = true;
                    if (code.find(exception_comment) == std::string::npos)
                    {
                        code.insert(comment + comment_type.comment.size(), "\t" + exclude_line_from_code_coverage + "\t" + exception_comment + "\t");
                    }
                    break;
                }
            }
            if (comment_found == false)
            {
                code += "\t//" + exclude_line_from_code_coverage + "\t" + exception_comment;
            }
        }
        else
        {
            std::cerr << "Invalid relation between html and source file" << std::endl;
            retVal = false;
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
        }
    }
    catch (const std::ios_base::failure& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return retVal;
}

static bool save_and_annotate_source_code(std::unordered_map<std::string, std::string> parameter, std::unordered_map<std::string, std::vector<std::string>>& source, std::unordered_map<std::string, ST_HTMLContent>& tree)
{
    bool retVal = true;
    const std::string exclude_line_from_code_coverage = parameter["cc_ignore"];
    const bool save_code_as_different_file = parameter["create_new_sourcefiles"] == "true";
    const std::string src_extension = parameter["sourcefile_modifier"];
    
    for (auto& i : tree)
    {
        ST_TABLE_ELEMENT* ptable;
        std::vector<ST_TextAttributes> attrib;
        retVal &= build_full_dom_tree(tree[i.first], ptable, attrib);
        show_all_nodes(0, ptable);
        if (retVal == true)
        {
            ST_TABLE_ELEMENT* table_node = findtag(HTML_TAG::EN_TABLE, ptable);
            if (table_node)
            {
                std::vector<ST_TABLE_ELEMENT*> line_numbers;
                std::vector<ST_TABLE_ELEMENT*> related_class;
                std::vector<ST_TABLE_ELEMENT*> related_code;
                findallattributes("class", "line-number", ptable, line_numbers);
                findallattributes("class", "uncovered-line", ptable, related_class);
                findallattributes("class", "code", ptable, related_code);
                //std::cout << "Line is not covered "  << uncovered_line_node->content << std::endl;
            }

        }
#if 0
        findattribute("", const std::string & attrib_value, ST_TABLE_ELEMENT * proot)
        for (auto& src : source[i.first])
        {

            if (src.first.find(i.first.c_str()) != std::string::npos)
            {

                std::unordered_set<std::string>& uncovered_line = i.second.code_metric["uncovered-line"];
                std::unordered_set<std::string>& uncovered_branch = i.second.code_metric["uncovered-branch"];
                std::unordered_set<std::string>& covered_line = i.second.code_metric["covered-line"];
                std::unordered_set<std::string> pure_uncovered_branch = intersect(covered_line, uncovered_branch);
                std::unordered_set<std::string> covered_branch = intersect(uncovered_line, uncovered_branch);
                               

                bool retVal = extract_metric(parameter, parameter["uncovered_line"], uncovered_line, src.second);
                retVal &= extract_metric(parameter, parameter["uncovered_branch"], pure_uncovered_branch, src.second);
                
                std::string sourcfile_name = save_code_as_different_file == false ? src.first : src.first + src_extension;
                retVal &= save_file(sourcfile_name, src.second);

            }
        }
        if (retVal == false)
            break;
#endif
    }

    return retVal;
}

static bool patch_html_file(const std::string& content, std::string& out)
{
    ST_DOMTREE* pDomTree = nullptr;
    std::string new_content;
    
    std::regex stylesheet(R"(href='[./\\]*style.css')");
    std::string adapted_content = std::regex_replace(content, stylesheet, "href='./style.css'");

    bool retVal = create_DOM_tree(adapted_content, &pDomTree);
    if (retVal == true)
    {
        ST_Statistics stats;
        create_annotated_html(adapted_content, new_content, stats, pDomTree);
        retVal = write_html_file("Test.html", new_content);
        if (pDomTree != nullptr)
        {
            delete pDomTree;
        }
    }
    return retVal;
}
#if 0
static bool save_and_annotate_html_files(const std::string &root, std::unordered_map<std::string, std::string> parameter, ST_DocumentContent& source)
{
    bool retVal = true;
    const std::string exclude_line_from_code_coverage = parameter["cc_ignore"];
    const bool save_code_as_different_file = parameter["create_new_sourcefiles"] == "true";
    const std::string src_extension = parameter["sourcefile_modifier"];
    const size_t file_column = source.html_index_file.file_column;
    for (auto& i : source.html_index_file.table)
    {
        const auto iterator = i[file_column].references.find("href");
        if (iterator != i[file_column].references.cend())
        {
            size_t column = 0;
            size_t row = 0;
            std::string file_name = root + iterator->second;
            std::string content;

            const std::string root_file = root + i[file_column].references["href"];
            retVal = open_html_file(file_name, content);
            if (retVal == true)
            {
                std::string output;
                retVal = patch_html_file(content, output);
            }
            else
            {
                retVal = false;
                break;
            }
        }
    }
    return retVal;
}
#endif

static bool load_index_file(std::string file_name, ST_HTMLContent& tree_content)
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
    if (ptable != nullptr)
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
                                            for (const auto& i : IdentStrings)
                                            {
                                                bool check = false;
                                                ST_TABLE_ELEMENT* find_table_headers = column;
                                                while (find_table_headers)
                                                {
                                                    if (i == find_table_headers->content)
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
bool build_node_tree(std::string source, std::unordered_map<std::string, std::string> parameter)
{
    std::string output;
    bool retVal = false;
    if (parameter.find("index_file") != parameter.cend() && parameter.find("index_folder") != parameter.cend() && parameter.find("source_folder") != parameter.cend())
    {
        ST_TABLE_ELEMENT* pIndexTableStruct;
        ST_DocumentCollectionContent content;
        std::vector<ST_TextAttributes> attributes;
        std::unordered_map<std::string, std::vector<std::string>> source_code;

        const std::string root = parameter["index_folder"] + "/";
        retVal = load_index_file(root + source, content.index_file);
        if (retVal == true)
        {
            retVal = check_index_file(content.index_file, pIndexTableStruct, attributes);
            if (retVal == true)
            {
                build_relative_paths(attributes);
                if (retVal == true)
                {
                    retVal = dive_into_folder(parameter["source_folder"], attributes);
                    if (retVal == true)
                    {
                        std::unordered_map<std::string, std::vector<std::string>> source_code;
                        retVal = load_source_code(attributes, source_code);
                        if (retVal == true)
                        {
                            std::unordered_map<std::string, ST_HTMLContent> source_tree;
                            retVal = load_html_code(root, attributes, source_tree);
                            if (retVal == true)
                            {
                                if (parameter["patch_sourcefile"] == "true")
                                {
                                    retVal = save_and_annotate_source_code(parameter, source_code, source_tree);
                                    if (retVal == true)
                                    {
                                        std::cout << "Source files successfully annotated" << std::endl;
                                    }
                                }

                                /*if (parameter["annotate_html"] == "true")
                                {
                                    retVal = save_and_annotate_html_files(root, parameter, content);
                                    if (retVal == true)
                                    {
                                        std::cout << "HTML files successfully annotated" << std::endl;
                                    }
                                }*/
                            }
                        }
                    }
                }
            }
        }
    }
    return retVal;
}