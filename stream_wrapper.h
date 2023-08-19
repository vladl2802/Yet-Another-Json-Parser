//
// Created by Vlad on 12.07.2023.
//

#ifndef JSONPARSER_STREAM_WRAPPER_H
#define JSONPARSER_STREAM_WRAPPER_H

#include <iostream>

class IStreamWrapper {
    using char_int_t = std::char_traits<char>::int_type;

    std::istream* stream;

public:
    explicit IStreamWrapper(std::istream& is) noexcept;
    explicit IStreamWrapper(const std::string& s) noexcept;
    IStreamWrapper(IStreamWrapper&& other) noexcept = default;
    IStreamWrapper(const IStreamWrapper& other) = delete;

    IStreamWrapper operator=(IStreamWrapper&& other) = delete;
    IStreamWrapper operator=(const IStreamWrapper& other) = delete;

    ~IStreamWrapper() = default;

    char_int_t get();
};

#endif //JSONPARSER_STREAM_WRAPPER_H
