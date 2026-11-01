#include "core/collector.h"

namespace Elica::Core {

Project::Project(fs::path root_dir) : rootDir(move(root_dir)) {
    this->errors.clear();
    this->elySourceFiles.clear();
    this->nativeHeaderFiles.clear();
    this->nativeSourceFiles.clear();
    this->externalLibs.clear();
}

bool Project::loadConfig() {
    fs::path managerPath = this->rootDir / "manager.json";
    if (!fs::exists(managerPath)) {
        cerr << "[Elica][ERROR] Project manager not found in: " << this->rootDir << "\n";
        return false;
    }

    try {
        ifstream file(managerPath);
        nlohmann::json j;
        file >> j;

        if (!j.contains("project") || !j.contains("version") || !j.contains("enter")) {
            cerr << "[Elica][ERROR] manager.json must have "
                    "\"project\", \"version\", \"enter\"\n";
            return false;
        }

        this->config.name = j["project"];
        this->config.version = j["version"];
        this->config.entry = rootDir / j["enter"];

        return true;
    } catch (const exception &e) {
        cerr << "[Elica][ERROR] Manager parsing failed: " << e.what() << "\n";
        return false;
    }
}

fs::path Project::locateModule(const string& moduleName) const {
    // modules/
    fs::path local_module_dir = this->rootDir / "modules" / moduleName;
    if (fs::exists(local_module_dir) && fs::is_directory(local_module_dir)) {
        return local_module_dir;
    }

    // fallback
    fs::path parent_dir = this->rootDir.parent_path();
    if (fs::exists(parent_dir) && fs::is_directory(parent_dir)) {
        for (const auto& entry : fs::directory_iterator(parent_dir)) {
            if (entry.is_directory() && entry.path() != rootDir) {
                // Проверяем наличие папки modules/<module_name> внутри соседнего проекта
                fs::path candidate = entry.path() / "modules" / moduleName;
                if (fs::exists(candidate) && fs::is_directory(candidate)) {
                    return candidate;
                }
                // Или если сам соседний проект и есть искомый модуль (совпадает имя папки)
                if (entry.path().filename() == moduleName) {
                    return entry.path();
                }
            }
        }
    }

    return {};
}

bool Project::addLib(const string& moduleName, ModuleInfo& outInfo) {
    fs::path module_dir = this->locateModule(moduleName);
    
    if (module_dir.empty()) {
        cerr << "Error: Module '" << moduleName << "' not found in local modules/ or across workspace projects.\n";
        return false;
    }

    fs::path json_path = module_dir / "module.json";
    if (!fs::exists(json_path)) {
        cerr << "Error: Module '" << moduleName << "' is missing module.json at " << json_path << "\n";
        return false;
    }

    try {
        ifstream file(json_path);
        nlohmann::json j;
        file >> j;

        outInfo.name = j.value("name", moduleName);

        // elyc
        if (j.contains("sources") && j["sources"].is_array()) {
            for (const auto& src : j["sources"]) {
                outInfo.elycFiles.push_back(module_dir / src.get<string>());
            }
        }

        // headers
        if (j.contains("headers") && j["headers"].is_array()) {
            for (const auto& hdr : j["headers"]) {
                outInfo.headerFiles.push_back(module_dir / hdr.get<string>());
            }
        }

        // precompiled
        if (j.contains("libraries") && j["libraries"].is_array()) {
            for (const auto& lib : j["libraries"]) {
                outInfo.libraryFiles.push_back(module_dir / lib.get<string>());
            }
        }

        return true;
    } catch (const exception& e) {
        cerr << "Error parsing module.json for '" << moduleName << "': " << e.what() << "\n";
        return false;
    }
}

void Project::addFile(fs::path path, FileType type) {
    if (type == FileType::ELY || type == FileType::ELYC) {
        this->elySourceFiles.push_back(path);
    } else if (type == FileType::H || type == FileType::HPP) {
        this->nativeHeaderFiles.push_back(path);
    } else if (type == FileType::C || type == FileType ::CPP) { 
        this->nativeSourceFiles.push_back(path);
    } else {
        this->externalLibs.push_back(path);
    }
}

string Project::getRawFile(fs::path filePath) {
    if (!fs::exists(filePath) || fs::is_directory(filePath)) return string();

    string content;
    ifstream contentStream(filePath, ios::in | ios::binary);

    if (!contentStream.is_open()) {
        cerr << "[Elica][ERROR]: Failed to read file: " << filePath.string() << "\n";
    }

    ostringstream buffer;
    buffer << contentStream.rdbuf();

    return buffer.str();
}

bool Project::collectSources() {
    if (!fs::exists(rootDir) || !fs::is_directory(rootDir)) {
        cerr << "[Elica][ERROR] Root directory does not exist or is not a directory: " << rootDir << "\n";
        return false;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootDir)) {
            if (!entry.is_regular_file()) continue;

            fs::path path = entry.path();
            string ext = path.extension().string();

            fs::path relPath = fs::relative(path, rootDir);
            if (!relPath.empty()) {
                fs::path firstDirPath = *relPath.begin();
                string firstDir = firstDirPath.generic_string();
                if (firstDir == "build" || firstDir == "bin" || firstDir == "_compilation" || firstDir == ".git") {
                    continue;
                }
            }

            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") {
                addFile(path, FileType::CPP);
            } else if (ext == ".c") {
                addFile(path, FileType::C);
            };
        }

        return true;
    } catch (const exception& e) {
        cerr << "[Elica][ERROR] Failed to collect sources: " << e.what() << "\n";
        return false;
    }
}

int Project::traceErrors() {
    if (this->errors.size() == 0) {
        return 0;
    }

    for (size_t i = 0; i < this->errors.size(); i++) {
        Error error = errors[i];
        cout << "[Elica][ERROR][" << error.path
             << "][line: " << error.position.line
             << ", column: " << error.position.column
             << "]: " << error.message;
    }
    return 1;
}

}