// Elica!

#include <iostream>
#include <filesystem>
#include <vector>
#include <iomanip>
#include <cstdio>

// Elica
#include "core/utils.h"
#include "core/collector.h"

#include "lexer/lexer.h"
#include "lexer/token.h"

#include "features/preprocessor.h"
#include "features/macro.h"
#include "features/jumper.h"
#include "parser/ast_maker.h"
#include "parser/grammars.h"
#include "semantic/analyzer.h"

using namespace std;

namespace fs = std::filesystem;

// DEBUG MODE
#define ALL_DEBUG

#ifdef ALL_DEBUG
#define DEBUG
#define PREPOC_DEBUG
#define LEX_DEBUG
#define PARSE_DEBUG
#define SEMA_DEBUG
#endif


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
    cout << "JUMPING..." << endl;
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

    // Helper: escape control characters so the dump stays one token per line.
    const auto escapeLexeme = [](const std::string &s) {
        std::string out;
        for (const char ch : s) {
            switch (ch) {
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(ch));
                        out += buf;
                    } else {
                        out += ch;
                    }
            }
        }
        return out;
    };

    // Helper: print a token stream in a human-readable form.
    const auto printTokens = [&](const std::vector<Elica::Lexer::Token> &tokens) {
        for (const auto &token : tokens) {
            if (std::holds_alternative<Elica::Lexer::CommonToken>(token.type) &&
                std::get<Elica::Lexer::CommonToken>(token.type) ==
                    Elica::Lexer::CommonToken::END_OF_FILE) {
                continue;
            }
            cout << "  [" << token.position.line
                 << ":" << token.position.column << "] "
                 << std::left << std::setw(26)
                 << Elica::Lexer::tokenTypeName(token)
                 << escapeLexeme(token.lexeme) << "\n";
        }
    };

    // Helper: parse tokens into an AST and print it (or report the failure).
    const auto parseAndDump = [&](const std::vector<Elica::Lexer::Token> &tokens,
                                  Elica::Lexer::LexerLanguage lang,
                                  const std::string &rel) {
        auto tree = Elica::Parsing::parseTokens(tokens, lang);
        if (tree) {
            auto prog = Elica::Parsing::buildProgram(*tree, tokens, rel);
            Elica::Parsing::printProgram(*prog, cout);
        } else {
            cout << "  [PARSE FAILED: " << rel << "]\n";
        }
    };

    // Phase 2: preprocess + lex each Ely file, then parse into an AST.
    std::vector<std::shared_ptr<Elica::Parsing::Program>> elyPrograms;
    for (const auto &file : elyFiles) {
        const auto rel = fs::relative(file, config.entry.parent_path());
        cout << "── " << rel << " ──\n";

        auto tokens = preprocessor->preprocess(file,
                                               Elica::Lexer::LexerLanguage::ELY);

        printTokens(tokens);
        cout << "\n";

        // Collapse cCode/cppCode blocks into leaf nodes, then parse.
        std::vector<std::string> blockLangs;
        auto clean = Elica::Parsing::collapseNativeBlocks(tokens, blockLangs);
        auto tree = Elica::Parsing::parseTokens(clean, Elica::Lexer::LexerLanguage::ELY);
        if (tree) {
            auto prog = Elica::Parsing::buildProgram(*tree, clean, rel.string());
            elyPrograms.push_back(prog);
            Elica::Parsing::printProgram(*prog, cout);
        } else {
            cout << "  [PARSE FAILED: " << rel << "]\n";
        }
        cout << "\n";
    }

    // Phase 3: lex + parse native C/C++ files. Raw source — no macro
    // preprocessing (C/C++ preprocessing is intentionally out of scope).
    std::vector<std::shared_ptr<Elica::Parsing::Program>> nativePrograms;
    const auto lexNative = [&](const fs::path &file, Elica::Lexer::LexerLanguage lang) {
        const auto rel = fs::relative(file, config.entry.parent_path());
        cout << "── " << rel << " ["
             << (lang == Elica::Lexer::LexerLanguage::C ? "C" : "C++") << "] ──\n";

        string source = project->getRawFile(file);
        Elica::Lexer::Lexer rawLexer(source, lang);
        auto tokens = rawLexer.tokenize();
        printTokens(tokens);
        cout << "\n";

        // Pull preprocessor directives out, then parse the rest.
        std::vector<std::string> directives;
        auto clean = Elica::Parsing::extractDirectives(tokens, directives);
        if (!directives.empty()) {
            cout << "  directives:\n";
            for (const auto &d : directives) cout << "    " << d << "\n";
            cout << "\n";
        }
        auto tree = Elica::Parsing::parseTokens(clean, lang);
        if (tree) {
            auto prog = Elica::Parsing::buildProgram(*tree, clean, rel.string());
            nativePrograms.push_back(prog);
            Elica::Parsing::printProgram(*prog, cout);
        } else {
            cout << "  [PARSE FAILED: " << rel << "]\n";
        }
        cout << "\n";
    };

    for (const auto &file : project->getNativeSourceFiles()) {
        const auto ext = file.extension().string();
        lexNative(file, (ext == ".c") ? Elica::Lexer::LexerLanguage::C
                                      : Elica::Lexer::LexerLanguage::CPP);
    }
    for (const auto &file : project->getNativeHeaderFiles()) {
        lexNative(file, Elica::Lexer::LexerLanguage::CPP);
    }

    // Phase 3.5: semantic analysis with the full picture — Ely AND native
    // declarations share one code space (direct C++ calls, no extern).
    if (!elyPrograms.empty() || !nativePrograms.empty()) {
        cout << "── SEMANTIC ANALYSIS ──\n";
        auto sem = Elica::Semantic::analyzeEly(elyPrograms, nativePrograms);
        Elica::Semantic::printAnalysis(sem, cout);
        cout << "\n";
    }

    // Stdlib resolution report (system <...> includes found via the toolchain).
    if (!project->getExternalIncludes().empty()) {
        const auto &incs = project->getExternalIncludes();
        cout << "Resolved system includes (" << incs.size() << "):\n";
        for (const auto &inc : incs) {
            cout << "  " << inc.string() << "\n";
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