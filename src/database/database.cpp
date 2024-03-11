#include "database.hpp"

#include <filesystem>

namespace fs = std::filesystem;

// check if current is in the primary or child directory
bool in_same_directory(const fs::path &current, const fs::path &directory) {
    if (current.empty() || directory.empty()) {
        return false;
    }
    if (current.root_name().compare(directory.root_name()) != 0) { // it's used in windows to compare disk names
        return false;
    }
    const fs::path relative = current.lexically_relative(directory);
    return !relative.empty() && relative.begin()->compare("..") != 0;
}

namespace apptime {
bool database::is_ignored(const fs::path &path) const {
    for (const auto &[type, value]: ignores()) {
        switch (type) {
        case ignore_file:
            if (!path.compare(value)) {
                return true;
            }
            break;
        case ignore_path:
            if (in_same_directory(path, value)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}
} // namespace apptime