#pragma once
#include <luisa/core/basic_types.h>
namespace vstd {
class StringBuilder;
}// namespace vstd
namespace lc::spirv {
using namespace luisa;
class Register {
    uint64_t _id{~0ull};
    explicit Register(uint64_t id);

public:
    Register();
    explicit Register(std::nullptr_t) : Register() {}
    static void reset_register();
    static Register new_register();
    [[nodiscard]] luisa::string to_str() const;
    bool operator==(uint64_t id) const { return _id == id; }
    bool operator==(std::nullptr_t) const { return _id == ~0ull; }
};
}// namespace lc::spirv