//
// Created by Vlad on 31.07.2023.
//

#include <iostream>
#include <variant>
#include <cstdint>
#include <initializer_list>
#include <vector>

enum class StructureParsingFlag {for_each};

template<typename T>
concept ParserVariablePath = std::is_integral_v<T> ||
                             std::is_convertible_v<T, std::string> || std::is_same_v<T, char *> ||
                             std::is_same_v<T, StructureParsingFlag>;

template<typename T, ParserVariablePath... path> requires std::is_integral_v<T>
void f(T& x, path... y) {
    std::cout << x << " - " << sizeof...(y) << std::endl;
}

int main() {
    int y = 10;
    int t = 20;
    int* x = &y;
    int* p = &t;
    int** w = &x;
    (*w) = p;
    std::cout << (**w) << "\n";
    std::cout << (*x) << "\n";
}
