#include <iostream>
#include <fstream>
#include <memory>
#include <cassert>
#include <map>
#include <vector>

#include "lexer.h"
#include "json_parser.h"
#include "stream_wrapper.h"

int main() {
    std::ifstream inp("test_1.txt", std::ios::binary);
    IStreamWrapper wr(inp);
    JsonParser par(std::move(wr));
    auto tmp = par.base_parser_unit();
    int constant;
    std::string first_signal_type;
    std::string comparator;
    std::string output_signal_type;
    std::string output_signal_name;
    tmp->add_variable_listener(constant, {"decider_conditions", "constant"});
    tmp->add_variable_listener(first_signal_type, {"decider_conditions", "first_signal", "type"});
    tmp->add_variable_listener(comparator, {"decider_conditions", "comparator"});
    tmp->add_variable_listener(output_signal_type, {"decider_conditions", "output_signal", "type"});
    tmp->add_variable_listener(output_signal_name, {"decider_conditions", "output_signal", "name"});

    tmp->scan();
    std::cout << constant << " " << first_signal_type << " " << comparator << " " << output_signal_type << " " << output_signal_name << std::endl;
}
