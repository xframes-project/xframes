#include <string>
#include <optional>
#include <variant>
#include <functional>

#pragma once

#ifndef SHARED_STUFF
#define SHARED_STUFF

// https://www.cppstories.com/2021/heterogeneous-access-cpp20/
struct StringHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(const char *txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(const std::string &txt) const {
        return std::hash<std::string>{}(txt);
    }
};

using OnInitCallback = void (*)();
using OnTextChangedCallback = void (*)(int id, const std::string& value);
using OnComboChangedCallback = void (*)(int id, int value);
using OnNumericValueChangedCallback = void (*)(int id, float value);
using OnBooleanValueChangedCallback = void (*)(int id, bool value);
using OnMultipleNumericValuesChangedCallback = void (*)(int id, const float* values, int size);
using OnClickCallback = void (*)(int id);
using RequestTextureFn = void (*)(int id, std::string resourceLocation);

std::optional<float> charPercentageToFloat(const char* input);

// Base class for type-erased setters
struct IVariadicFn {
    virtual ~IVariadicFn() = default;
    virtual void call(void* args[]) = 0;
};

// Templated derived class for variadic setters
template <typename... Args>
class VariadicFnImpl : public IVariadicFn {
    std::function<void(Args...)> setter;

public:
    explicit VariadicFnImpl(std::function<void(Args...)> s) : setter(std::move(s)) {}

    void call(void* args[]) override {
        // Use std::apply to call the setter with unpacked arguments
        callImpl(std::index_sequence_for<Args...>{}, args);
    }

private:
    template <std::size_t... I>
    void callImpl(std::index_sequence<I...>, void* args[]) {
        setter(*static_cast<Args*>(args[I])...);
    }
};


#endif
