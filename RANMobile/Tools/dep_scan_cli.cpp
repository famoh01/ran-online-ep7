#include "../Data/Importer/EP7DependencyScanner.h"
#include <iostream>
#include <map>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: dep_scan_cli <client_dir> <file1> [file2 ...]\n";
        return 1;
    }
    RANMobile::EP7DependencyScanner scanner;
    scanner.IndexClientDirectory(argv[1]);

    for (int i = 2; i < argc; ++i) {
        std::cout << "=== DEPENDENCY SCAN: " << argv[i] << " ===\n";
        auto assets = scanner.ScanRecursive(argv[i], 3);
        std::map<std::string, std::pair<int, uint64_t>> extStats;
        int missingCount = 0;
        for (const auto& a : assets) {
            std::cout << (a.found ? "[FOUND] " : "[MISSING] ") << a.name
                      << " (" << a.sizeBytes << " bytes)\n";
            if (a.found) {
                extStats[a.extension].first++;
                extStats[a.extension].second += a.sizeBytes;
            } else {
                missingCount++;
            }
        }
        std::cout << "--- SUMMARY (" << argv[i] << ") ---\n";
        std::cout << "Total Referenced: " << assets.size() << " | Missing: " << missingCount << "\n";
        for (const auto& [ext, st] : extStats) {
            std::cout << "  " << ext << ": " << st.first << " files, " << st.second << " bytes\n";
        }
    }
    return 0;
}
