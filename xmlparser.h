#pragma once
#include <string>
#include <filesystem>

struct ST_Command_Line_Options
{
    std::filesystem::path index_file;
    std::filesystem::path annotated_index_file;
    std::filesystem::path index_folder;
    std::filesystem::path project_folder;
    bool diagnostics{false};
    bool annotate_html{false};
    std::filesystem::path annotated_html_folder;
    std::string sourcefile_ext;
};

bool build_node_tree(ST_Command_Line_Options const& parameter);
