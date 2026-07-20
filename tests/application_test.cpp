#include "lightengine/application.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::ostringstream output;
    const lightengine::Application application{output};

    if (application.run() != 0) {
        std::cerr << "Application returned a non-zero exit code\n";
        return 1;
    }

    const std::string text = output.str();
    if (text.find("DMX Light Engine") == std::string::npos) {
        std::cerr << "Application did not print its identity\n";
        return 1;
    }

    return 0;
}
