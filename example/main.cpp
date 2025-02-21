#include "la-disassembler.h"

uint32_t input_inst() {
    std::string line;
    std::cout << "Enter instruction: ";
    std::getline(std::cin, line);
    if (line.empty()) {
        return 0;
    }
    uint32_t inst = std::stoul(line, nullptr, 16);
    return inst;
}

int main() {
    LADisassembler::Disassembler disassembler;
    disassembler.set_imm_hex(true);
    disassembler.set_reg_alias(false);
    disassembler.set_reg_prefix(true);
    while (true) {
        uint32_t inst = input_inst();
        if (inst == 0) {
            break;
        }
        std::string s = disassembler.disassemble(inst, 0x80000000);
        if (s.empty()) {
            std::cout << "Invalid instruction" << std::endl;
        } else {
            std::cout << s << std::endl;
        }   
    }
    return 0;
}
