// CCAnnotator.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include <iostream>
#include <string>
#include <locale>
#include "xmlparser.h"
#define BOOST_ALL_NO_LIB
#include <boost/program_options.hpp>

int main(int ac, char** av) {

    namespace po = boost::program_options;
    int retVal = 0;
    std::locale::global(std::locale("en_US.UTF-8"));
    try {

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("index_file", po::value<std::string>()->default_value("index.html"), "specifies the root index file")
            ("annotated_index_file", po::value<std::string>()->default_value("annotated_index.html"), "specifies the root index file")
            ("index_folder", po::value<std::string>()->default_value("./code_coverage_report"), "specifies the index folder")
            ("project_folder", po::value<std::string>()->default_value("."), "specifies the path to the project folder")
            ("no-diagnostics", po::bool_switch(), "disable Visual Studio coverage diagnostics")
            ("annotate_html", po::bool_switch(), "annotate the html file")
            ("annotated_html_folder", po::value<std::string>()->default_value("./code_coverage_report"), "specifies a relative path to the annotated files")
            ("sourcefile_ext", po::value<std::string>()->default_value(""), "specifies an extension to append to the source file");

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        if (vm.count("index_folder")) {

            std::cout << "project_folder set to "
                << vm["project_folder"].as<std::string>() << ".\n";
            std::cout << "index_folder set to "
                << vm["index_folder"].as<std::string>() << ".\n";
            std::cout << "index_file speciefied as "
                << vm["index_file"].as<std::string>() << ".\n";

            ST_Command_Line_Options parameters;
            parameters.index_file = vm["index_file"].as<std::string>();
            parameters.annotated_index_file = vm["annotated_index_file"].as<std::string>();
            parameters.index_folder = vm["index_folder"].as<std::string>();
            parameters.project_folder = vm["project_folder"].as<std::string>();
            parameters.diagnostics = !vm["no-diagnostics"].as<bool>();
            parameters.annotate_html = vm["annotate_html"].as<bool>();
            parameters.annotated_html_folder = vm["annotated_html_folder"].as<std::string>();
            parameters.sourcefile_ext = vm["sourcefile_ext"].as<std::string>();

            retVal = build_node_tree(parameters)?0:1;

        }
        else {
            std::cerr << "Source folder not set.\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        retVal = 1;
    }
    catch (...) {
        std::cerr << "Exception of unknown type!\n";
        retVal = 1;
    }
    return retVal;
}

// Programm ausführen: STRG+F5 oder Menüeintrag "Debuggen" > "Starten ohne Debuggen starten"
// Programm debuggen: F5 oder "Debuggen" > Menü "Debuggen starten"

// Tipps für den Einstieg: 
//   1. Verwenden Sie das Projektmappen-Explorer-Fenster zum Hinzufügen/Verwalten von Dateien.
//   2. Verwenden Sie das Team Explorer-Fenster zum Herstellen einer Verbindung mit der Quellcodeverwaltung.
//   3. Verwenden Sie das Ausgabefenster, um die Buildausgabe und andere Nachrichten anzuzeigen.
//   4. Verwenden Sie das Fenster "Fehlerliste", um Fehler anzuzeigen.
//   5. Wechseln Sie zu "Projekt" > "Neues Element hinzufügen", um neue Codedateien zu erstellen, bzw. zu "Projekt" > "Vorhandenes Element hinzufügen", um dem Projekt vorhandene Codedateien hinzuzufügen.
//   6. Um dieses Projekt später erneut zu öffnen, wechseln Sie zu "Datei" > "Öffnen" > "Projekt", und wählen Sie die SLN-Datei aus.
