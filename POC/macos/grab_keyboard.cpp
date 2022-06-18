#include <string>
#include "keyio_mac.hpp"

int main() {
    std::string device = "Moonlander Mark I";
    monitor_kb((char*)device.c_str());
    return 0;
}
