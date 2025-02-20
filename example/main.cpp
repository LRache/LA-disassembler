#include "la-disassembler.h"

int main() {
    LADisassembler::Disassembler disassembler;
    disassembler.set_imm_hex(true);
    disassembler.set_reg_alias(false);
    disassembler.set_reg_prefix(true);
    std::cout << disassembler.disassemble(0x57ff93ff, 0x80000080) << std::endl;
    return 0;
}
