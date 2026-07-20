#include "lightengine/application.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        return lightengine::Application{std::cout}.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
