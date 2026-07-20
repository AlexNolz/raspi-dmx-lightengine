#pragma once

#include <iosfwd>

namespace lightengine {

class Application final {
public:
    explicit Application(std::ostream& output) noexcept;

    [[nodiscard]] int run() const;

private:
    std::ostream& output_;
};

}  // namespace lightengine
