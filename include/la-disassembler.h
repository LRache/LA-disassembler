#ifndef __LADISASSEMBLER_DISASSEMBLER_H__
#define __LADISASSEMBLER_DISASSEMBLER_H__

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cstdint>

namespace LADisassembler {

class BitPat {
private:
    uint64_t bits;
    uint64_t mask;
    unsigned int length;

public:
    BitPat(const std::string &s) {
        uint64_t bits = 0;
        uint64_t mask = 0;
        int length = 0;
        for (int i = s.length() - 1; i >= 0; i--) {
            switch (s[i]) {
                case '0': length++; break;
                case '1': bits |= 1 << length; length++; break;
                case 'x': mask |= 1 << length; length++; break;
                case '?': mask |= 1 << length; length++; break;
                case '_': break;
                case ' ': break;
                default: std::cerr << "BitPat: invalid character in pattern" << std::endl; break;
            }
        }
        if (length > 64) {
            std::cerr << "BitPat: pattern too long" << std::endl;
        }
                
        for (std::size_t i = length; i < 64; i++) {
            mask |= 1UL << i;
        }
        
        this->bits = bits;
        this->mask = ~mask; // mask is 0 for match any bit
        this->length = length;
    }
        
    unsigned int get_length() const {
        return this->length;
    }
        
    bool match(uint64_t data) const {
        return (data & this->mask) == this->bits;
    }
};
        
template<typename entry_t>
class Decoder {
private:
    struct Entry {
        BitPat pattern;
        entry_t entry;
    };
    std::vector<Entry> patterns;

public:
    unsigned int fixedLength = 0;
        
    bool add(const std::string &pattern, const entry_t &entry) {
        if (patterns.empty()) {
            fixedLength = BitPat(pattern).get_length();
        } else {
            if (fixedLength != BitPat(pattern).get_length()) {
                std::cerr << "Decoder: pattern length mismatch" << std::endl;
                return false;
            }
        }
        patterns.push_back({BitPat(pattern), entry});
        return true;
    }
        
    bool decode(uint64_t bits, entry_t &e) const {
        for (const Entry &p : patterns) {
            if (p.pattern.match(bits)) {
                e = p.entry;
                return true;
            }
        }
        return false;
    }
        
    unsigned int count() const {
        return patterns.size();
    }
};

class Disassembler {
public:
    enum TokenType {
        NAME,   // str
        RD,     // num
        RJ,     // num
        RK,     // num
        UIMM32, // num
        SIMM32, // num
        UIMM64, // num
        SIMM64, // num
        PCOFF,  // num
        BASEREG, // num
        ADDROFF, // num
        END,
    };
    
    struct DecodeToken {
        TokenType type = END;
        union {
            const char *str;
            uint64_t num;
        };
    };
    
    struct DecodeTokenArray {
        DecodeToken tokens[4];
    };

private:
    static std::string to_hex(uint64_t n) {
        static const char *hexDigits = "0123456789abcdef";

        if (n == 0) {
            return "0";
        }

        std::string hexString;
        while (n > 0) {
            int remainder = n % 16;
            hexString += hexDigits[remainder];
            n /= 16;
        }

        std::reverse(hexString.begin(), hexString.end());

        return hexString;
    } 

    bool hexImm = false;
    bool regAlias = false;
    bool regPrefix = false;
    bool instAlias = true;
    bool mode32 = false;

    struct DecoderEntry {
        std::function<void (const Disassembler *, uint32_t, const void*, DecodeTokenArray &)> disasmFunc;
        const void* args;
    };
    
    Decoder<DecoderEntry> decoder;
    bool decode(uint32_t inst, DecodeTokenArray &tokens) const {
        DecoderEntry entry;
        bool success = decoder.decode(inst, entry);
        if (!success) {
            return false;
        }
        entry.disasmFunc(this, inst, entry.args, tokens);
        return true;
    }

    #define __BITS(hi, lo) ((inst >> (lo)) & ((1 << ((hi) - (lo) + 1)) - 1))
    #define __SEXT(bits, from) ((int64_t)(((int64_t)bits) << (64 - (from))) >> (64 - (from)))
    #define __SIMM(hi, lo) __SEXT(__BITS(hi, lo), (hi) - (lo) + 1)

    void disasm_3R(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = RJ;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = RK;
        tokens.tokens[3].num = __BITS(14, 10);
    }

    void disasm_2RI12(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = RJ;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = SIMM32;
        tokens.tokens[3].num = __SIMM(21, 10);
    }

    void disasm_2RI14(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = RJ;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = SIMM32;
        tokens.tokens[3].num = __SIMM(23, 10);
    }

    void disasm_shifti_w(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = RJ;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = UIMM32;
        tokens.tokens[3].num = __BITS(14, 10);
    }

    void disasm_12UI(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = UIMM32;
        tokens.tokens[2].num = __BITS(24, 5) << 12;
    }

    void disasm_branch(uint32_t inst, const void *args, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)args;
        tokens.tokens[1].type = RJ;
        tokens.tokens[1].num = __BITS(9, 5);
        tokens.tokens[2].type = RK;
        tokens.tokens[2].num = __BITS(4, 0);
        tokens.tokens[3].type = PCOFF;
        tokens.tokens[3].num = __SIMM(25, 10) << 2;
    }

    void disasm_jirl(uint32_t inst, const void *, DecodeTokenArray &tokens) const {
        if (inst == 0x4c000020 && instAlias) {
            tokens.tokens[0].type = NAME;
            tokens.tokens[0].str = "ret";
            tokens.tokens[1].type = END;
        } else {
            tokens.tokens[0].type = NAME;
            tokens.tokens[0].str = "jirl";
            tokens.tokens[1].type = RD;
            tokens.tokens[1].num = __BITS(4, 0);
            tokens.tokens[2].type = RJ;
            tokens.tokens[2].num = __BITS(9, 5);
            tokens.tokens[3].type = PCOFF;
            tokens.tokens[3].num = __SIMM(24, 10);
        }
    }

    void disasm_b(uint32_t inst, const void *, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = "b";
        tokens.tokens[1].type = PCOFF;
        tokens.tokens[1].num = __SEXT(__BITS(9, 0) << 16 | __BITS(24, 10), 26);
    }

    void disasm_bl(uint32_t inst, const void *, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = instAlias ? "call" : "bl";
        tokens.tokens[1].type = PCOFF;
        tokens.tokens[1].num = __SEXT(__BITS(9, 0) << 16 | __BITS(24, 10), 26);
    }

    void disasm_load(uint32_t inst, const void *name, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)name;
        tokens.tokens[1].type = RD;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = BASEREG;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = ADDROFF;
        tokens.tokens[3].num = __SIMM(21, 10);
    }

    void disasm_store(uint32_t inst, const void *name, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = (const char*)name;
        tokens.tokens[1].type = RJ;
        tokens.tokens[1].num = __BITS(4, 0);
        tokens.tokens[2].type = BASEREG;
        tokens.tokens[2].num = __BITS(9, 5);
        tokens.tokens[3].type = ADDROFF;
        tokens.tokens[3].num = __SIMM(21, 10);
    }

    void disasm_break(uint32_t inst, const void *, DecodeTokenArray &tokens) const {
        tokens.tokens[0].type = NAME;
        tokens.tokens[0].str = "break";
        tokens.tokens[1].type = UIMM32;
        tokens.tokens[1].num = __BITS(14, 0);
    }

    #undef __SIMM
    #undef __SEXT
    #undef __BITS

public:
    Disassembler() {
        #define __INSTPAT_NAME(pattern, func, name) this->decoder.add(pattern, {&Disassembler::func, #name});
        #define __INSTPAT_NONE(pattern, func) this->decoder.add(pattern, {&Disassembler::func, nullptr});

        __INSTPAT_NAME("0000000000 0100000 ????? ????? ?????", disasm_3R, add.w  );
        __INSTPAT_NAME("0000000000 0100001 ????? ????? ?????", disasm_3R, add.d  );
        __INSTPAT_NAME("0000000000 0100010 ????? ????? ?????", disasm_3R, sub.w  );
        __INSTPAT_NAME("0000000000 0100011 ????? ????? ?????", disasm_3R, sub.d  );
        __INSTPAT_NAME("0000000000 0100100 ????? ????? ?????", disasm_3R, slt    );
        __INSTPAT_NAME("0000000000 0100101 ????? ????? ?????", disasm_3R, sltu   );
        __INSTPAT_NAME("0000000000 0100110 ????? ????? ?????", disasm_3R, maskeqz);
        __INSTPAT_NAME("0000000000 0100111 ????? ????? ?????", disasm_3R, masknez);
        __INSTPAT_NAME("0000000000 0101000 ????? ????? ?????", disasm_3R, nor    );
        __INSTPAT_NAME("0000000000 0101001 ????? ????? ?????", disasm_3R, and    );
        __INSTPAT_NAME("0000000000 0101010 ????? ????? ?????", disasm_3R, or     );
        __INSTPAT_NAME("0000000000 0101011 ????? ????? ?????", disasm_3R, xor    );
        __INSTPAT_NAME("0000000000 0101100 ????? ????? ?????", disasm_3R, orn    );
        __INSTPAT_NAME("0000000000 0101101 ????? ????? ?????", disasm_3R, andn   );
        __INSTPAT_NAME("0000000000 0101110 ????? ????? ?????", disasm_3R, sll.w  );
        __INSTPAT_NAME("0000000000 0101111 ????? ????? ?????", disasm_3R, srl.w  );
        __INSTPAT_NAME("0000000000 0110000 ????? ????? ?????", disasm_3R, sra.w  );
        __INSTPAT_NAME("0000000000 0110001 ????? ????? ?????", disasm_3R, sll.d  );
        __INSTPAT_NAME("0000000000 0110010 ????? ????? ?????", disasm_3R, srl.d  );
        __INSTPAT_NAME("0000000000 0110011 ????? ????? ?????", disasm_3R, sra.d  );
        
        __INSTPAT_NAME("0000000000 0111000 ????? ????? ?????", disasm_3R, mul.w  );
        __INSTPAT_NAME("0000000000 0111001 ????? ????? ?????", disasm_3R, mulh.w );
        __INSTPAT_NAME("0000000000 0111010 ????? ????? ?????", disasm_3R, mulhu.w);
        __INSTPAT_NAME("0000000000 1000000 ????? ????? ?????", disasm_3R, div.w  );
        __INSTPAT_NAME("00000000001 000001 ????? ????? ?????", disasm_3R, mod.w  );
        __INSTPAT_NAME("0000000000 1000010 ????? ????? ?????", disasm_3R, div.wu );
        __INSTPAT_NAME("0000000000 1000011 ????? ????? ?????", disasm_3R, mod.wu );

        __INSTPAT_NAME("000000 1010 ???????????? ????? ?????", disasm_2RI12, addi.w);
        __INSTPAT_NAME("000000 1000 ???????????? ????? ?????", disasm_2RI12, slti  );
        __INSTPAT_NAME("000000 1001 ???????????? ????? ?????", disasm_2RI12, sltiu );
        __INSTPAT_NAME("000000 1101 ???????????? ????? ?????", disasm_2RI12, andi  );
        __INSTPAT_NAME("000000 1110 ???????????? ????? ?????", disasm_2RI12, ori   );
        __INSTPAT_NAME("000000 1111 ???????????? ????? ?????", disasm_2RI12, xori  );

        __INSTPAT_NAME("00000000010000 001 ????? ????? ?????", disasm_shifti_w, slli.w);
        __INSTPAT_NAME("00000000010001 001 ????? ????? ?????", disasm_shifti_w, srli.w);
        __INSTPAT_NAME("00000000010010 001 ????? ????? ?????", disasm_shifti_w, srai.w);

        __INSTPAT_NAME("0001010 ???????????????????? ?????", disasm_12UI, lu12i.w  );
        __INSTPAT_NAME("0001110 ???????????????????? ?????", disasm_12UI, pcaddu12i);

        __INSTPAT_NAME("010110 ???????????????? ????? ?????", disasm_branch, beq );
        __INSTPAT_NAME("010111 ???????????????? ????? ?????", disasm_branch, bne );
        __INSTPAT_NAME("011000 ???????????????? ????? ?????", disasm_branch, blt );
        __INSTPAT_NAME("011001 ???????????????? ????? ?????", disasm_branch, bge );
        __INSTPAT_NAME("011010 ???????????????? ????? ?????", disasm_branch, bltu);
        __INSTPAT_NAME("011011 ???????????????? ????? ?????", disasm_branch, bgeu);

        __INSTPAT_NONE("010011 ???????????????? ????? ?????", disasm_jirl);
        __INSTPAT_NONE("010100 ???????????????? ????? ?????", disasm_b   );
        __INSTPAT_NONE("010101 ???????????????? ????? ?????", disasm_bl  );

        __INSTPAT_NAME("00101 00000 ???????????? ????? ?????", disasm_load , ld.b );
        __INSTPAT_NAME("00101 00001 ???????????? ????? ?????", disasm_load , ld.h );
        __INSTPAT_NAME("00101 00010 ???????????? ????? ?????", disasm_load , ld.w );
        __INSTPAT_NAME("00101 01000 ???????????? ????? ?????", disasm_load , ld.bu);
        __INSTPAT_NAME("00101 01001 ???????????? ????? ?????", disasm_load , ld.hu);
        __INSTPAT_NAME("00101 00100 ???????????? ????? ?????", disasm_store, st.b );
        __INSTPAT_NAME("00101 00101 ???????????? ????? ?????", disasm_store, st.h );
        __INSTPAT_NAME("00101 00110 ???????????? ????? ?????", disasm_store, st.w );

        __INSTPAT_NAME("00000000001010100 ???????????????", disasm_break, break);

        #undef __INSTPAT_NONE
        #undef __INSTPAT_NAME
    }

    void set_imm_hex(bool hex) {
        hexImm = hex;
    }

    void set_reg_alias(bool alias) {
        regAlias = alias;
    }

    void set_reg_prefix(bool prefix) {
        regPrefix = prefix;
    }

    std::string fmt_gpr(unsigned int index) const {
        static const char* gprAlias[] = {
            "zero", "ra", "tp", "sp",
            "a0", "a1", "a2", "a3",
            "a4", "a5", "a6", "a7",
            "t0", "t1", "t2", "t3",
            "t4", "t5", "t6", "t7",
            "t8", "u0", "fp", "s0",
            "s1", "s2", "s3", "s4",
            "s5", "s6", "s7", "s8",
        };

        if (index >= 32) {
            return "";
        }
    
        if (regAlias) {
            if (regPrefix) {
                return std::string("$") + gprAlias[index];
            } else {
                return gprAlias[index];
            }
        } else {
            if (regPrefix) {
                return "$r" + std::to_string(index);
            } else {
                return "r" + std::to_string(index);
            }
        }
    }

    std::string fmt_gpr(const DecodeToken &token) const {
        return fmt_gpr(token.num);
    }

    std::string fmt_imm(uint64_t imm, TokenType type) const {
        switch (type) {
            case UIMM32:
                if (hexImm) {
                    return "0x" + to_hex((uint32_t)imm);
                } else {
                    return std::to_string((uint32_t)imm);
                }
            case SIMM32:
                if (hexImm) {
                    int32_t imm32 = imm;
                    if (imm32 < 0) return "-0x" + to_hex((uint32_t)-imm32);
                    else return "0x" + to_hex((uint32_t)imm32);
                } else {
                    return std::to_string((int32_t)imm);
                }
            case UIMM64:
                if (hexImm) {
                    return "0x" + to_hex((uint64_t)imm);
                } else {
                    return std::to_string((uint64_t)imm);
                }
            case SIMM64:
                if (hexImm) {
                    int64_t imm64 = imm;
                    if (imm64 < 0) return "-0x" + to_hex((uint64_t)-imm64);
                    else return "0x" + to_hex((uint64_t)imm64);
                } else {
                    return std::to_string((int64_t)imm);
                }
            default:
                return "";
        }
    }

    std::string fmt_imm(const DecodeToken &token) const {
        return fmt_imm(token.num, token.type);
    }

    std::string fmt_pc(uint64_t pc, uint64_t off) const {
        if (mode32) {
            return fmt_imm((uint32_t)pc + (uint32_t)off, UIMM32);
        } else {
            return fmt_imm(pc + off, UIMM64);
        }
    }

    std::string fmt_pc(uint64_t pc, const DecodeToken &token) const {
        return fmt_pc(pc, token.num);
    }

    std::string fmt_base_off(const DecodeToken &base, const DecodeToken &off) const {
        if (mode32) {
            return fmt_imm(off.num, SIMM32) + "(" + fmt_gpr(base) + ")";
        }
        else {
            return fmt_imm(off.num, SIMM64) + "(" + fmt_gpr(base) + ")";
        }
    }

    std::string fmt_tokens(uint64_t pc, const DecodeTokenArray &tokens) const {
        std::string result;
        for (int i = 0; i < 4; i++) {
            if (tokens.tokens[i].type == END) {
                break;
            }

            switch (tokens.tokens[i].type) {
                case NAME:
                    result += tokens.tokens[i].str;
                    break;
                case RD:
                case RJ:
                case RK:
                    result += fmt_gpr(tokens.tokens[i]);
                    break;
                case UIMM32:
                case SIMM32:
                case UIMM64:
                case SIMM64:
                    result += fmt_imm(tokens.tokens[i]);
                    break;
                case PCOFF:
                    result += fmt_pc(pc, tokens.tokens[i]);
                    break;
                case BASEREG:
                    if (i != 3 && tokens.tokens[i + 1].type == ADDROFF) {
                        result += fmt_base_off(tokens.tokens[i], tokens.tokens[i + 1]);
                        i++;
                    } else {
                        result += fmt_gpr(tokens.tokens[i]);
                    }
                default:
                    break;
            }
            if (tokens.tokens[i].type != NAME && i < 3 && tokens.tokens[i + 1].type != END) {
                result += ",";
            }
            if (i < 3) {
                result += " ";
            }
        }
        return result;
    }

    bool disassemble_to_tokens(uint32_t inst, DecodeTokenArray &tokens) const {
        return decode(inst, tokens);
    }
    
    std::string disassemble(uint32_t inst, uint64_t pc) {
        DecodeTokenArray tokens;
        bool s = disassemble_to_tokens(inst, tokens);
        if (!s) {
            return "";
        }

        return fmt_tokens(pc, tokens);
    }
};

}

#endif
