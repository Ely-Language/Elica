#include "core/utils.h"

bool Elica::Core::createProject(string rootPath, const string &projectName, const string &version) {
    fs::path root = rootPath;
    if (!fs::exists(root / projectName)) {
        fs::path newRoot = root / projectName;
        fs::create_directory(newRoot);
        root = newRoot;
    }
    try {
        fs::path modules = root / "modules";
        fs::path elica = root / "elica";
        fs::path bin = root / "bin";
        fs::path compilation = root / "_compilation";

        fs::create_directory(modules);
        fs::create_directory(elica);
        fs::create_directory(bin);
        fs::create_directory(compilation);

        // manager.json
        fs::path manager = root / "manager.json";
        if (!fs::exists(manager)) {
            ofstream managerFile(manager);
            managerFile << "{\n"
                        << "    \"project\": \"" << projectName << "\",\n"
                        << "    \"version\": \"" << version << "\",\n"
                        << "    \"enter\": \"enter.ely\""
                        << "}\n";
        }

        // main.ely
        fs::path main = root / "main.ely";
        if (!fs::exists(main)) {
            ofstream mainFile(main);
            mainFile << "int func main() {\n"
                     << "   console.print(\"Hello, World!\");\n"
                     << "}\n";
        }

        // TODO: environment

        return true;
    } catch (const exception& e) {
        cerr << "[Elica][ERROR] Failed to create project structure: " << e.what() << "\n";
        return false;
    }
}

vector<Elica::Core::Error> Elica::Core::checkFileForIDE(const fs::path &root) {
    // TODO IDE errors
    vector<Elica::Core::Error> result;
    return result;
}