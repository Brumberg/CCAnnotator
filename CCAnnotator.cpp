// CCAnnotator.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include <iostream>
#include <string>
#include <locale>
#include "xmlparser.h"
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
            ("index_folder", po::value<std::string>()->default_value("./code_coverage_report"), "specifies the index folder")
            ("source_folder", po::value<std::string>(), "specifies the corresponding source folder");

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        if (vm.count("source_folder")) {

            std::cout << "sourcefolder set to "
                << vm["source_folder"].as<std::string>() << ".\n";
            std::cout << "index_folder set to "
                << vm["index_folder"].as<std::string>() << ".\n";
            std::cout << "index_file speciefied as "
                << vm["index_file"].as<std::string>() << ".\n";

            std::unordered_map<std::string, std::string> parameters;
            parameters["index_file"] = vm["index_file"].as<std::string>();
            parameters["index_folder"] = vm["index_folder"].as<std::string>();
            parameters["source_folder"] = vm["source_folder"].as<std::string>();
            const std::string source = vm["index_file"].as<std::string>();
            retVal = build_node_tree(source, parameters);

        }
        else {
            std::cerr << "source folder not set.\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
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
