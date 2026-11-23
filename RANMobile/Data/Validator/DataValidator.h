#pragma once

#include <string>
#include <vector>

namespace RANMobile {

struct DataError {
    std::string resourceID;
    std::string resourceType;
    std::string file;
    std::string status = "NOT FOUND";
};

struct DataValidationReport {
    bool mapFound = false;
    bool monsterFound = false;
    bool npcFound = false;
    bool itemFound = false;
    bool skillFound = false;
    bool questFound = false;
    bool modelFound = false;
    bool textureFound = false;
    bool animationFound = false;
    std::vector<DataError> errors;

    std::string ToString() const;
};

class DataValidator {
public:
    static DataValidationReport ValidateDirectory(const std::string& dataDir);
};

} // namespace RANMobile
