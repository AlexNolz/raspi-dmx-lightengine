#include "lightengine/application.hpp"

#include "lightengine/version.hpp"

#include <ostream>

namespace lightengine {

Application::Application(std::ostream& output) noexcept : output_{output} {}

int Application::run() const {
    output_ << "Raspberry Pi DMX Light Engine " << version << '\n';
    output_ << "C++ project is ready.\n";
    return 0;
}

}  // namespace lightengine
