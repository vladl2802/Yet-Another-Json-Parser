//
// Created by Vlad on 12.07.2023.
//
#include "stream_wrapper.h"
#include <sstream>

IStreamWrapper::IStreamWrapper(std::istream& is) noexcept : stream(&is)  {
}

IStreamWrapper::IStreamWrapper(const std::string &s) noexcept : stream(new std::istringstream(s)) {
}

IStreamWrapper::char_int_t IStreamWrapper::get() {
    char t;
    (*stream) >> t;
    return t;
}


