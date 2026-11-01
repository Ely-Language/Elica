#include <iostream>
#include <filesystem>
#include <vector>

#include "core/utils.h"
#include "core/collector.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "features/preprocessor.h"
#include "features/macro.h"

using namespace std;
namespace fs = filesystem;

#define DEBUG

#define VERSION "27.1 devstream"
#define BUILD "unnumered"

int help() {
    cout << "Elica compiler help:\n"
         << " - Create new project:\n"
         << "       elica project [root path] [project name] (-v/--version) [version of project]\n"
         << " - Activate project virtual environment:\n"
         << "       elica env [manager or project folder path]\n"
         << " - Fast compilation & run:\n"
         << "       elica run [manager or project folder path]\n"
         << " - Release compilation (build):\n"
         << "       elica build [manager or project folder path]\n"
         << " - Get version:\n"
         << "       elica --version\n"
         << " - Check file for the errors:\n"
         << "       elica check [path to file]\n";
    return 0;
}

int project(string rootPath, string projectName, string version = "0.0.1") {
    bool success = Elica::Core::createProject(rootPath, projectName, version);
    return (success) ? 0 : 1;
}

int activateEnv(string &projectPath) {
    return 0;
}

int run(string &projectPath) {
    return 0;
}

int build(string &projectPath) {
    return 0;
}

int version() {
    cout << VERSION << ", build " << BUILD << endl; 
    return 0;
}

int checkFile(string& pathToFile) {
    fs::path filePath = pathToFile;
    if (!fs::exists(pathToFile)) {
        cerr << "[Elica][ERROR]: File nor found!" << filePath << "\n";
        return 1;
    } 

    vector<Elica::Core::Error> errorList = Elica::Core::checkFileForIDE(filePath);

    if (errorList.size() == 0) {
        cout << "[Ely][FILECHECK]: Success!";
    }

    for (size_t i = 0; i < errorList.size(); i++) {
        cerr 
        << "[Ely][ERROR][" 
        << errorList[i].path << "]["
        << errorList[i].position.line << ":"
        << errorList[i].position.column << "]:" 
        << errorList[i].message << "\n";
    }
    return 0;
}

int dev(string& pathToFile) {
    fs::path projectPath = pathToFile;
    Elica::Core::Project* project = new Elica::Core::Project(projectPath);
    
    // manager.json
    project->loadConfig();
    Elica::Core::ProjectConfig config = project->getConfig();

    cout << config.name << '\n';
    cout << config.version << '\n';
    cout << config.entry << '\n';

    // entry point
    string entry = project->getRawFile(config.entry);
    cout << entry << "\n";

    // Lexing & Preprocess
    Elica::Features::Preprocess::Preprocessor* preprocessor = new Elica::Features::Preprocess::Preprocessor(project); 
    vector<Elica::Lexer::Token> tokens = preprocessor->process(config.entry, Elica::Lexer::LexerLanguage::ELY);

    for (size_t i = 0; i < tokens.size(); i++) {
        auto token = tokens[i];
        cout << "[Elica][Token][" << token.position.line
             << ":" << token.position.column
             << "]:" << token.type.index()
             << " = " << token.lexeme << "\n"; 
    }

	delete project;
	delete preprocessor;

    return 0;
}

int main(int argc, char* argv[]) {
    vector<string> args(argv, argv + argc);

    // version
    if (args.size() >= 2 && (args[1] == "-v" || args[1] == "--version")) {
        return version();
    }

    // help
    if (args.size() == 1 || (args.size() >= 2 && (args[1] == "-h" || args[1] == "--help"))) {
        return help();
    }

    // other commands
    if (args.size() > 1) {
        string command = args[1];

        if (command == "project") {
            string rootPath = "";
            string projectName = "";
            string versionStr = "";

            for (size_t i = 2; i < args.size(); i++) {
                if (args[i] == "-v" && i + 1 < args.size()) {
                    versionStr = args[++i];
                } else if (rootPath.empty()) {
                    rootPath = args[i];
                } else if (projectName.empty()) {
                    projectName = args[i];
                }
            }

            if (!rootPath.empty() && !projectName.empty()) {
                if (!versionStr.empty()) {
                    return project(rootPath, projectName, versionStr);
                } else {
                    return project(rootPath, projectName);
                }
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"project\","
                     << " try elica --help or elica -h to get more info.\n";
                return 1;
            }
        } 
        else if (command == "env") {
            if (args.size() == 3) {
                return activateEnv(args[2]);
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"env\","
                     << " try elica --help or elica -h to get more info.\n";  
                return 1;             
            }
        } 
        else if (command == "run") {
            if (args.size() == 3) {
                return run(args[2]);
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"run\","
                     << " try elica --help or elica -h to get more info.\n";
                return 1;       
            }
        } 
        else if (command == "build") {
            if (args.size() == 3) {
                return build(args[2]);
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"build\","
                     << " try elica --help or elica -h to get more info.\n";
                return 1;          
            }
        } 
        else if (command == "check") {
            if (args.size() == 3) {
                return checkFile(args[2]);
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"check\","
                     << " try elica --help or elica -h to get more info.\n";
                return 1; 
            }
        } else if (command == "dev") {
            if (args.size() == 3) {
                return dev(args[2]);
            } else {
                cerr << "[Elica][ERROR]: Invalid arguments for command \"dev\","
                     << " try elica --help or elica -h to get more info.\n";
                return 1; 
            }
        }
    }

    cerr << "[Elica][ERROR]: Unknown command. Try elica --help.\n";
    return 1;
}