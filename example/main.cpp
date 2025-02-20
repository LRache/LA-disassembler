#include "la-disassembler.h"

int main() {
    LADisassembler::Disassembler disassembler;
    disassembler.set_imm_hex(false);
    disassembler.set_reg_alias(false);
    disassembler.set_reg_prefix(true);
    std::cout << disassembler.disassemble(0x002a0000, 0x80000154) << std::endl;
    return 0;
}
