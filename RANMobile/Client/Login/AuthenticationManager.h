#pragma once

#include <string>

namespace RANMobile {

class NetworkClient;

class AuthenticationManager {
public:
    static bool Login(NetworkClient* net, const std::string& username, const std::string& password);
    static const std::string& GetServerDisplayName() { return s_serverDisplayName; }

private:
    static inline std::string s_serverDisplayName = "FAM HAWAII";
};

} // namespace RANMobile
