#include "xmlparser.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "gumbo.h"
extern "C"
{
#include "lexbor/html/interfaces/element.h"
#include "lexbor/dom/interfaces/comment.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/dom/interfaces/text.h"
#include "lexbor/dom/interface.h"
#include "lexbor/html/serialize.h"
#include "lexbor/html/parser.h"
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
static void traverse_nodes(ST_HTMLParser &parser_struct, ST_HTMLTags tags, lxb_dom_node_t* node) {
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
                const lxb_char_t* tag_name = lxb_dom_element_qualified_name(element, nullptr);
                if (tag_name != nullptr)
                {
                    std::string val = reinterpret_cast<const char*>(tag_name);
                    ltags.attributes[val] = true;
                    if (val == "table")
                    {
                        parser_struct.column = parser_struct.row = 0;
                        parser_struct.table.clear();
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
                        parser_struct.table.rbegin()->push_back(a);
                        ++parser_struct.column;
                    }
                }
            }
            break;
        case LXB_DOM_NODE_TYPE_COMMENT:
            break;
        }

        traverse_nodes(parser_struct, ltags, child);
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
            traverse_nodes(parser_struct, tags, lxb_dom_interface_node(document->body));
        }
        retVal = true;
    }
    else {
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
    bool retVal = false;
    if (parameter.find("index_file") != parameter.cend() && parameter.find("index_folder") != parameter.cend() && parameter.find("source_folder") != parameter.cend())
    {
        ST_HTMLParser index_file;
        const std::string root = parameter["index_folder"] + "/";
        retVal = get_html_content(root + source, index_file);
        if (retVal == true)
        {
            retVal = check_index_file(index_file);
            if (retVal == true)
            {
                build_relative_paths(index_file);
                if (retVal == true)
                {
                    retVal = dive_into_folder(parameter["source_folder"], index_file);
                    if (retVal == true)
                    {
                        std::unordered_map<std::string, std::vector<std::string>> source_code;
                        retVal = load_source_code(index_file, source_code);
                        if (retVal == true)
                        {
                            std::unordered_map<std::string, ST_HTMLParser> html_content;
                            retVal = load_html_code(root, index_file, html_content);
                            if (retVal == true)
                            {
                            }
                        }
                    }
                }
            }
        }
    }
    return retVal;
}