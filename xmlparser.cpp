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
extern "C"
{
#include "lexbor/html/interfaces/element.h"
#include "lexbor/dom/interfaces/comment.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/dom/interfaces/text.h"
#include "lexbor/html/serialize.h"
#include "lexbor/dom/interface.h"
#include "lexbor/html/parser.h"
#include "lexbor/html/html.h"
#include "lexbor/core/fs.h"
}
#include <unordered_map>
#include <cassert>

#if 0
static const char* find_title(const GumboNode* root) {
    assert(root->type == GUMBO_NODE_ELEMENT);
    assert(root->v.element.children.length >= 2);

    const GumboVector* root_children = &root->v.element.children;
    GumboNode* head = NULL;
    for (size_t i = 0; i < root_children->length; ++i) {
        GumboNode* child = reinterpret_cast<GumboNode*>(root_children->data[i]);
        if (child->type == GUMBO_NODE_ELEMENT &&
            child->v.element.tag == GUMBO_TAG_HEAD) {
            head = child;
            break;
        }
    }
    assert(head != NULL);

    GumboVector* head_children = &head->v.element.children;
    for (size_t i = 0; i < head_children->length; ++i) {
        GumboNode* child = reinterpret_cast<GumboNode*>(head_children->data[i]);
        if (child->type == GUMBO_NODE_ELEMENT &&
            child->v.element.tag == GUMBO_TAG_TITLE) {
            if (child->v.element.children.length != 1) {
                return "<empty title>";
            }
            GumboNode* title_text = reinterpret_cast<GumboNode*>(child->v.element.children.data[0]);
            assert(title_text->type == GUMBO_NODE_TEXT || title_text->type == GUMBO_NODE_WHITESPACE);
            return title_text->v.text.text;
        }
    }
    return "<no title found>";
}
#endif


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

struct ST_HTMLParser
{
    bool header_set;
    bool header_description_set;
    bool footer_generated_by_set;
    std::string html_heading;
    std::string html_description;
    std::string html_generator_name;
    size_t row;
    size_t column;
    size_t file_column;
    std::vector<std::vector<ST_TextAttributes>> table;
    std::unordered_map<std::string, std::unordered_set<std::string>> code_metric;
    lxb_html_document_t* dom_document;
};

struct ST_DocumentContent
{
    ST_HTMLParser html_index_file;
    std::unordered_map<std::string, ST_HTMLParser> html_files;
    std::unordered_map<std::string, std::vector<std::string>> source_files;
};

/*------------------------------------------------------------------------------------------*//**
\brief          traverses the index file and returns the table
\
\param[out]     ST_HTMLParser &parser_struct, -> structure to fill in
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static void traverse_nodes(ST_HTMLParser &parser_struct, ST_HTMLTags tags, lxb_dom_node_t* node, bool &initial_table, bool enforce_table_regeneration) {
    lxb_dom_node_t* child = node->first_child;
    while (child != nullptr) {

        ST_HTMLTags ltags = tags;
        switch (child->type)
        {
        case LXB_DOM_NODE_TYPE_ELEMENT:
        {
            lxb_dom_element_t* element = lxb_dom_interface_element(child);
            if (element != nullptr)
            {
                size_t len;
                const lxb_char_t* class_attr = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"class", 5, &len);
                if (class_attr != nullptr)
                {
                    const std::string class_type = std::string(reinterpret_cast<const char*>(class_attr));
                    ltags.attributes[class_type] = true;
                }

                const lxb_char_t* tag_name = lxb_dom_element_qualified_name(element, nullptr);
                if (tag_name != nullptr)
                {
                    std::string val = reinterpret_cast<const char*>(tag_name);
                    ltags.attributes[val] = true;
                    if (val == "table")
                    {
                        if (initial_table == false || enforce_table_regeneration == true)
                        {
                            initial_table = true;
                            parser_struct.column = parser_struct.row = 0;
                            parser_struct.table.clear();
                        }
                    }
                    else if (val == "tr")
                    {
                        if (parser_struct.column != 0)
                        {
                            ++parser_struct.row;
                        }
                        
                        parser_struct.table.push_back(std::vector<ST_TextAttributes>());
                        parser_struct.column = 0;
                        
                    }
                    else if (val == "a")
                    {
                        size_t len;
                        const lxb_char_t* href = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"href", 4, &len);
                        ltags.references["href"] = std::string(reinterpret_cast<const char*>(href));
                    }
                    else if (ltags.attributes.find("table") != ltags.attributes.cend() && val == "td" && child->first_child == nullptr)
                    {
                        static const std::string empty_string = "";
                        ST_TextAttributes a;
                        a.text = std::string(reinterpret_cast<const char*>(""));
                        a.attributes = ltags.attributes;
                        a.references = ltags.references;
                        parser_struct.table.rbegin()->push_back(a);
                        ++parser_struct.column;
                    }
                    /*else if (val == "td")
                    {
                        ++parser_struct.column;
                        parser_struct.table.push_back(std::vector<std::string>());
                    }*/
                }
            }
        }
        break;
        case LXB_DOM_NODE_TYPE_TEXT:
            if ((parser_struct.header_set == false) && (ltags.attributes.find("h2") != ltags.attributes.cend()) && (ltags.attributes.size() == 1))
            {
                lxb_dom_text_t* text_node = lxb_dom_interface_text(child);
                if (text_node != nullptr)
                {
                    //lxb_dom_text_t* text = lxb_dom_interface_text(text_node);
                    const lxb_char_t* text_content = lxb_dom_node_text_content(node, NULL);
                    //std::cout << "Text: " << text_content << std::endl;
                    if (text_content != nullptr)
                    {
                        parser_struct.html_heading = reinterpret_cast<const char*>(text_content);
                        parser_struct.header_set = true;
                    }
                }
            }
            else if ((parser_struct.header_description_set == false) && (ltags.attributes.find("h4") != ltags.attributes.cend()) && (ltags.attributes.size() == 1))
            {
                const lxb_char_t* text_content = lxb_dom_node_text_content(node, NULL);
                if (text_content != nullptr)
                {
                    parser_struct.html_description = reinterpret_cast<const char*>(text_content);
                    parser_struct.header_description_set = true;
                }
            }
            else if ((parser_struct.footer_generated_by_set == false) && (ltags.attributes.find("h5") != ltags.attributes.cend()) && (ltags.attributes.size() == 1))
            {
                const lxb_char_t* text_content = lxb_dom_node_text_content(node, NULL);
                if (text_content != nullptr)
                {
                    parser_struct.html_generator_name = reinterpret_cast<const char*>(text_content);
                    parser_struct.footer_generated_by_set = true;
                }
            }
            else if (ltags.attributes.find("table") != ltags.attributes.cend() && ltags.attributes.find("td") != ltags.attributes.cend())
            {
                lxb_dom_text_t* text_node = lxb_dom_interface_text(child);
                if (text_node != nullptr)
                {
                    //lxb_dom_text_t* text = lxb_dom_interface_text(text_node);
                    const lxb_char_t* text_content = lxb_dom_node_text_content(node, NULL);
                    //std::cout << "Text: " << text_content << std::endl;
                    if (text_content != nullptr)
                    {
                        ST_TextAttributes a;
                        a.text = std::string(reinterpret_cast<const char*>(text_content));
                        a.attributes = ltags.attributes;
                        a.references = ltags.references;
 
                        //line_out number exist
                        std::vector<ST_TextAttributes> &iter = *parser_struct.table.rbegin();
                        if (iter.size() > 0)
                        {
                            if (a.attributes.find("uncovered-line") != a.attributes.cend())
                            {
                                parser_struct.code_metric["uncovered-line"].insert(iter[0].text);
                            }
                            else if (a.attributes.find("covered-line") != a.attributes.cend())
                            {
                                parser_struct.code_metric["covered-line"].insert(iter[0].text);
                            }

                            if (a.attributes.find("red") != a.attributes.cend())
                            {
                                if (parser_struct.code_metric["uncovered-line"].find(iter[0].text) == parser_struct.code_metric["uncovered-line"].cend())
                                {
                                    parser_struct.code_metric["uncovered-branch"].insert(iter[0].text);
                                }
                            }
                        }

                        parser_struct.table.rbegin()->push_back(a);
                        ++parser_struct.column;
                    }
                }
            }
            break;
        case LXB_DOM_NODE_TYPE_COMMENT:
            break;
        }

        traverse_nodes(parser_struct, ltags, child, initial_table, enforce_table_regeneration);
        child = child->next;
    }
}


/*------------------------------------------------------------------------------------------*//**
\brief          traverses the index file and returns the table
\
\param[in]      ST_HTMLTags tags,                 ->intermediate tags/attributes
\param[in]      lxb_dom_node_t* node              ->node to process
\param[out]     void
\return         -
*//*-------------------------------------------------------------------------------------------*/
static bool parse_html_file(std::string& content, ST_HTMLParser& parser_struct) {
    bool retVal = false;

    const lxb_char_t* html = reinterpret_cast<const lxb_char_t*>(content.c_str());
    size_t html_len = content.length();

    lxb_status_t status;
    lxb_html_document_t* document;

    document = lxb_html_document_create();
    if (document != nullptr) {

        status = lxb_html_document_parse(document, html, content.length());

        if (status == 0)
        {
            //ST_HTMLParser content;
            parser_struct.header_set = false;
            parser_struct.header_description_set = false;
            ST_HTMLTags tags;
            bool table_initialitzed = false;
            traverse_nodes(parser_struct, tags, lxb_dom_interface_node(document->body), table_initialitzed, false);
        }
        parser_struct.dom_document = document;
        retVal = true;
    }
    else {
        parser_struct.dom_document = nullptr;
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
static bool get_html_content(std::string source, ST_HTMLParser &content)
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
        std::string html = buffer.str();

        if (parse_html_file(html, content)) {
            //std::cout << "HTML content parsed successfully" << std::endl;
            retVal = true;
        }
        else {
            std::cerr << "Failed to parse HTML content" << std::endl;
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
static bool check_index_file(const ST_HTMLParser& parser)
{
    static const std::string IdentStrings[] =
    {
        "Filename", "Function Coverage", "Line Coverage" /*, "Region Coverage", "Branch Coverage"*/
    };

    bool retVal = true;
    for (const auto& id : IdentStrings)
    {
        bool hit = false;
        for (const auto& all_cells : parser.table[0])
        {
            hit |= id == all_cells.text;
        }
        hit &= retVal;
    }
    retVal &= parser.html_heading == "Coverage Report";
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
static bool build_relative_paths(ST_HTMLParser& parser)
{
    static const std::string IdentStrings[] =
    {
        "Filename"// , "Function Coverage", "Line Coverage" /*, "Region Coverage", "Branch Coverage"*/
    };

    size_t column = std::string::npos;
    size_t counter = 0;
    for (const auto& all_cells : parser.table[0])
    {
        if (all_cells.text == IdentStrings[0])
        {
            column = counter;
            parser.file_column = column;
        }
        ++counter;
    }
    
    const std::filesystem::path path = std::filesystem::current_path();
    const std::string rel_path(reinterpret_cast<const char*>(path.c_str()));
    if (column != std::string::npos)
    {
        for (auto& i : parser.table)
        {
            if (i[column].references.find("href") != i[column].references.cend())
            {
                std::string dummy = i[column].references["href"];
                size_t ix = dummy.find(rel_path);
                if (ix != std::string::npos)
                {
                    static const std::string ending = ".html";
                    size_t ending_ = dummy.find(ending);
                    ending_ = ending_ == std::string::npos ? dummy.size() : ending_;
                    dummy = dummy.substr(ix, ending_ - ix);
                    i[column].references["rel_file_path"] = dummy;
                }
            }
        }
    }
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
static bool dive_into_folder(const std::string& root_folder, ST_HTMLParser& parser)
{
    bool retVal = false;
   
    try {
        for (auto& i : parser.table)
        {
            if (i[parser.file_column].references.find("rel_file_path") != i[parser.file_column].references.cend() && i[parser.file_column].references["rel_file_path"] != "")
            {
                std::string rel_path = i[parser.file_column].references["rel_file_path"];
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
                            i[parser.file_column].references["src_file"] = entry.path().string();
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
static bool load_source_code(ST_HTMLParser& parser, std::unordered_map<std::string, std::vector<std::string>>& src_code)
{
    bool retVal = true;
    src_code.clear();
    for (auto& i : parser.table)
    {
        if (i[parser.file_column].references.find("src_file") != i[parser.file_column].references.cend())
        {
            std::string& src_file = i[parser.file_column].references["src_file"];
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
                            i[parser.file_column].crlf = true;
                            //std::cout << "Line ends with \\r\\n" << std::endl;
                        }
                        else {
                            i[parser.file_column].crlf = false;
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
static bool load_html_code(const std::string& root, ST_HTMLParser& parser, std::unordered_map<std::string, ST_HTMLParser>& src_code)
{
    bool retVal = true;
    src_code.clear();
    for (auto& i : parser.table)
    {
        ST_HTMLParser content;
        if (!i[parser.file_column].references.empty() && i[parser.file_column].references.find("rel_file_path") != i[parser.file_column].references.cend())
        {
            const std::string root_file = root + i[parser.file_column].references["href"];
            retVal &= get_html_content(root_file, content);
            src_code[i[parser.file_column].references["rel_file_path"]] = content;
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

static bool save_and_annotate_source_code(std::unordered_map<std::string, std::string> parameter, ST_DocumentContent& source)
{
    bool retVal = true;
    const std::string exclude_line_from_code_coverage = parameter["cc_ignore"];
    const bool save_code_as_different_file = parameter["create_new_sourcefiles"] == "true";
    const std::string src_extension = parameter["sourcefile_modifier"];
    for (auto& i : source.html_files)
    {
        for (auto& src : source.source_files)
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
    }
    return retVal;
}

static bool save_dom_tree(std::string file_name, ST_HTMLParser html)
{
    bool retVal = false;
    if (html.dom_document != nullptr)
    {
        lexbor_str_t html_output = {0};

        lxb_status_t serialized_html = lxb_html_serialize_str(lxb_dom_interface_node(html.dom_document), &html_output);
        if (serialized_html != LXB_STATUS_OK) {
            retVal = false;
        }
        else
        {
            
            if (html_output.data != nullptr)
            {
                std::ofstream out(file_name);
                if (out.is_open())
                {
                    out.write(reinterpret_cast<const char*>(html_output.data), html_output.length);
                }
                
                lexbor_str_destroy(&html_output, nullptr, false);
            }
            retVal = true;
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

enum class HTML_TAG : size_t { EN_HTML=0, EN_HEADING=1, EN_TABLE=2, EN_TABLE_ROW=3, EN_TABLE_COLUMN=4, EN_ROOT};

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
    void DecRef(size_t instance, std::array<size_t, N> *p) {
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

struct HTML_RegExExpressions
{
    HTML_TAG tag;
    std::regex starting;
    std::regex ending;
    BaseInstanceHandler* pbase;
};

struct HTMLHit {
    size_t matching_line;
    enum HTML_TAG tag;
    std::smatch match;
    bool operator <(const HTMLHit& comp) const { return this->matching_line > comp.matching_line; }
};

struct ST_DOMTREE
{
    HTMLHit content;
    ST_DOMTREE* pChild;
    ST_DOMTREE* pSiblings;
    ST_DOMTREE() : pSiblings(nullptr), pChild(nullptr){}
    ~ST_DOMTREE() { if (pSiblings != nullptr) { delete pSiblings; pSiblings = nullptr; } if (pChild != nullptr) { delete pChild; pChild = nullptr; } }
};

static bool findmatch(std::string::const_iterator start, std::string::const_iterator stop, std::regex& expression, std::smatch& match)
{
    return std::regex_search(start, stop, match, expression);
}

template <size_t N>
bool parse_htmlfile(const HTML_RegExExpressions (& html_tags)[N], std::pair<std::priority_queue<HTMLHit>&, std::priority_queue<HTMLHit>&> &queue, const std::string& content, ST_DOMTREE **ppTree)
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
            res.matching_line = index;
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
            res.matching_line = index;
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

                if ((in_stat == false) || ((res_in.matching_line > out_top.matching_line) && (in_stat == true)))
                {
                    //in this case it is a sibling
                    out.pop();
                    *top_node = new_node;
                    new_node->content = on_top;
                    top_node = &new_node->pSiblings;

                    if (in_stat == true)
                    {
                        HTMLHit res_out;
                        bool out_stat = find_match_ending(out_top, res_out);
                        if (in_stat == true)
                        {
                            in.push(res_in);
                        }
                        if (out_stat)
                        {
                            out.push(res_out);
                        }
                    }
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
            }
            else
            {
                on_top = in.top();
                rewind = on_top.matching_line > out_top.matching_line;
            }
            return rewind;
        };
        
        HTMLHit out_top;
        HTMLHit on_top;
        
        if (RewindDOMTree(on_top, out_top))
        {
            top_node = &top_nodes.top();
            if ((*top_node)->content.tag != out_top.tag)
            {
                std::cerr << "HTML tags do not match" << std::endl;
            }
            //in.pop();
            out.pop();
            top_nodes.pop();

            bool stop_ret = find_match_ending(on_top, out_top);
            if (stop_ret == true)
            {
                out.push(out_top);
            }
            top_node = &(*top_node)->pSiblings;
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

static bool patch_html_file(const std::string &content, std::string& out)
{
    bool retVal = true;
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
        { HTML_TAG::EN_TABLE_ROW, std::regex(R"(<\s*tr\s*>)"), std::regex(R"(<\s*/tr\s*>)"), &TABLE_ROW },
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
            lines.matching_line = match[0].first - content.cbegin();
            min_priority_queue_starting_quotes.push(lines);
        }

        bool found_end = std::regex_search(content, match, a.ending);
        if (found_end == true)
        {
            lines.tag = a.tag;
            lines.match = match;
            lines.matching_line = match[0].first - content.cbegin();
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
        ST_DOMTREE* pDomTree = nullptr;
        retVal = parse_htmlfile(ExpressionList, queue, content, &pDomTree);
    }
    return retVal;
}

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
        ST_DocumentContent index_file;
        
        index_file.html_index_file.dom_document = nullptr;

        const std::string root = parameter["index_folder"] + "/";
        retVal = get_html_content(root + source, index_file.html_index_file);
        if (retVal == true)
        {
            retVal = check_index_file(index_file.html_index_file);
            if (retVal == true)
            {
                build_relative_paths(index_file.html_index_file);
                if (retVal == true)
                {
                    retVal = dive_into_folder(parameter["source_folder"], index_file.html_index_file);
                    if (retVal == true)
                    {
                        std::unordered_map<std::string, std::vector<std::string>> source_code;
                        retVal = load_source_code(index_file.html_index_file, index_file.source_files);
                        if (retVal == true)
                        {
                            retVal = load_html_code(root, index_file.html_index_file, index_file.html_files);
                            if (retVal == true)
                            {
                                if (parameter["patch_sourcefile"] == "true")
                                {
                                    retVal = save_and_annotate_source_code(parameter, index_file);
                                    if (retVal == true)
                                    {
                                        std::cout << "Source files successfully annotated" << std::endl;
                                    }
                                }

                                if (parameter["annotate_html"] == "true")
                                {
                                    retVal = save_and_annotate_html_files(root, parameter, index_file);
                                    if (retVal == true)
                                    {
                                        std::cout << "HTML files successfully annotated" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return retVal;
}