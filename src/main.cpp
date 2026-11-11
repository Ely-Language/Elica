// Elica!

#include <iostream>
#include <filesystem>
#include <vector>

// Elica
#include "core/utils.h"
#include "core/collector.h"

#include "lexer/lexer.h"
#include "lexer/token.h"

#include "features/preprocessor.h"
#include "features/macro.h"
#include "features/jumper.h"

using namespace std;

namespace fs = std::filesystem;

#define DEBUG

#define VERSION "27.1 devstream"
#define BUILD "3"

int help() {
    cout << "Elica compiler help:\n"
         << " - Create new project:\n"
         << "       elica project [root path] [project name] (-v/--version) [version of project]\n"
         << " - Activate project virtual environment:\n"
         << "       elica env [manager or project folder path]\n"
         << " - Fast compilation  &run:\n"
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

int checkFile(string &pathToFile) {
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

int dev(string &pathToFile) {
    fs::path projectPath = pathToFile;
    auto project = new Elica::Core::Project(projectPath);

    // manager.json
    if (!project->loadConfig()) {
        cerr << "[Elica][ERROR] Failed to load manager.json\n";
        delete project;
        return 1;
    }

    Elica::Core::ProjectConfig config = project->getConfig();
    cout << "Project: " << config.name << " v" << config.version << "\n";
    cout << "Entry:   " << config.entry << "\n\n";

    // Phase 0: graph walk — collect reachable files (ely/elyc/c/cpp/h/hpp)
    Elica::Features::Preprocess::Jumping::Jumper jumper(*project);
    jumper.jumpAll();

    if (project->traceErrors() != 0) {
        cerr << "[Elica][ERROR] Jump phase failed\n";
        delete project;
        return 1;
    }

    const auto &elyFiles = project->getElySourceFiles();
    cout << "Reachable Ely files: " << elyFiles.size() << "\n";
    for (const auto &f : elyFiles) {
        cout << "  " << fs::relative(f, config.entry.parent_path()) << "\n";
    }
    cout << "\n";

    // Phase 1: collect all macros across the whole project (cross-file macros)
    auto preprocessor = new Elica::Features::Preprocess::Preprocessor(project);
    preprocessor->collectAll(elyFiles);

    // cout << "Macros collected: #=" << project->compilerMacros.size()
    //      << " @=" << project->textMacros.size()
    //      << " &=" << project->structMacros.size() << "\n\n";

    // Phase 2: preprocess + lex each file
    for (const auto &file : elyFiles) {
        const auto rel = fs::relative(file, config.entry.parent_path());
        cout << "── " << rel << " ──\n";

        auto tokens = preprocessor->preprocess(file,
                                               Elica::Lexer::LexerLanguage::ELY);

        for (const auto &token : tokens) {
            if (std::holds_alternative<Elica::Lexer::CommonToken>(token.type) &&
                std::get<Elica::Lexer::CommonToken>(token.type) ==
                    Elica::Lexer::CommonToken::END_OF_FILE) {
                continue;
            }
            cout << "  [" << token.position.line
                 << ":" << token.position.column
                 << "] variant=" << token.type.index()
                 << "  " << token.lexeme << "\n";
        }
        cout << "\n";
    }

    if (project->traceErrors() != 0) {
        cerr << "[Elica][ERROR] Preprocess/lex phase had errors\n";
    }

    delete preprocessor;
    delete project;
    return 0;
}

int main(int argc, char* argv[]) {
    vector<string> args(argv, argv + argc);

    // version
    if (args.size() >= 2  && (args[1] == "-v" || args[1] == "--version")) {
        return version();
    }

    // help
    if (args.size() == 1 || (args.size() >= 2  && (args[1] == "-h" || args[1] == "--help"))) {
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
                if (args[i] == "-v"  && i + 1 < args.size()) {
                    versionStr = args[++i];
                } else if (rootPath.empty()) {
                    rootPath = args[i];
                } else if (projectName.empty()) {
                    projectName = args[i];
                }
            }

            if (!rootPath.empty()  && !projectName.empty()) {
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