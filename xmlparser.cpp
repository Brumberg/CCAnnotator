#include "xmlparser.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
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
};

struct ST_DocumentParser
{
    bool header_set;
    bool header_description_set;
    bool footer_generated_by_set;
    std::string html_heading;
    std::string html_description;
    std::string html_generator_name;
    size_t row;
    size_t column;
    std::vector<std::vector<ST_TextAttributes>> table;
};


void traverse_nodes(ST_DocumentParser &parser_struct, ST_HTMLTags tags, lxb_dom_node_t* node) {
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

bool xml_parse_file_content(const std::string& content) {
    bool retVal = false;

    const lxb_char_t* html = reinterpret_cast<const lxb_char_t*>(content.c_str());
    size_t html_len = content.length();

    lxb_status_t status;
    lxb_dom_node_t* body;
    lxb_html_document_t* document;
    lxb_css_parser_t* parser;
    lxb_selectors_t* selectors;
    lxb_css_selector_list_t* list;

    document = lxb_html_document_create();
    if (document != nullptr) {

        status = lxb_html_document_parse(document, html, content.length());

        if (status == 0)
        {
            ST_DocumentParser content;
            content.header_set = false;
            content.header_description_set = false;
            ST_HTMLTags tags;
            traverse_nodes(content, tags, lxb_dom_interface_node(document->body));
        }
        retVal = true;
    }
    else {
        retVal = false;
    }
    return retVal;
}

bool xml_file_content(std::string source)
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

        if (xml_parse_file_content(html)) {
            std::cout << "HTML content parsed successfully" << std::endl;
        }
        else {
            std::cout << "Failed to parse HTML content" << std::endl;
        }
    }
    
    return retVal;
}