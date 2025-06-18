// vm/translator.cpp
// Полная трансляция байткода SnailVM в текстовый ARM64-ассемблер.
// Это прототип, покрывает все опкоды спецификации 2025-06-17.
// Генерирует .S, пригодный для сборки clang на Apple Silicon.

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <set>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "vm.hpp"

#include <vector>
#include <set>

// bring vm types into current namespace
using namespace vm;
using namespace vm::code;
using namespace vm::runtime;
using namespace vm::memory;

// ----------------------------- util -----------------------------------------
// global pointer to constant int32 values (populated by main)
static const std::vector<uint32_t>* gConstInts = nullptr;
static const std::vector<std::string>* gConstStrLabels = nullptr;
static uint16_t be16(const uint8_t *p) { return uint16_t(p[0] << 8 | p[1]); }
static int16_t  be16s(const uint8_t *p) { return int16_t(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t *p) {
    return uint32_t(p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
}

// ----------------------- VM константы --------------------------------------
namespace Op {
    enum : uint8_t {
        NOP            = 0x00,
        PUSH_CONST     = 0x01,
        PUSH_LOCAL     = 0x02,
        PUSH_GLOBAL    = 0x03,
        STORE_LOCAL    = 0x04,
        STORE_GLOBAL   = 0x05,
        POP            = 0x06,
        DUP            = 0x07,

        ADD            = 0x10,
        SUB            = 0x11,
        MUL            = 0x12,
        DIV            = 0x13,
        MOD            = 0x14,
        EQ             = 0x20,
        NEQ            = 0x21,
        LT             = 0x22,
        LE             = 0x23,
        GT             = 0x24,
        GTE            = 0x25,
        AND            = 0x26,
        OR             = 0x27,
        NOT            = 0x28,

        JMP            = 0x30,
        JMP_IF_FALSE   = 0x31,
        CALL           = 0x32,
        RET            = 0x33,
        HALT           = 0x34,
        JMP_IF_TRUE    = 0x35,

        NEW_ARRAY      = 0x40,
        GET_ARRAY      = 0x41,
        SET_ARRAY      = 0x42,
        INIT_ARRAY     = 0x43,
        INTRINSIC_CALL  = 0x50
    }; // enum Op
} // namespace Op

namespace Command = Op;

// ---------------- intrinsics -------------------------


namespace snail {

class Translator {
    std::unordered_set<std::string> referencedLabels;
    std::unordered_set<std::string> definedLabels;
public:
    Translator(const std::vector<uint8_t> &bytecode,
              const std::vector<uint32_t> *constInts = nullptr,
              const std::vector<std::string>* strLabels=nullptr,
              const std::vector<std::string>* funcNames=nullptr)
        : code(bytecode), constInts(constInts), strLabels(strLabels), functionNames(funcNames),
          idx(0), cond(""), labelCounter(0) {
        collectFunctionNames();
        collectLabels();
    }

    std::string translate();
private:
    std::vector<uint8_t> code;
    const std::vector<uint32_t> *constInts = nullptr;
    const std::vector<std::string> *const strLabels = nullptr;
    const std::vector<std::string> *const functionNames = nullptr;
    std::unordered_map<size_t, std::string> labelAtPc;
    int labelCounter = 0;
    uint16_t idx;
    std::string cond;
    std::ostringstream asmText;
    void collectFunctionNames();
    // helper to ensure we switch back to text section when needed
    void switchToText(){ this->asmText << "\t.section __TEXT,__text\n"; }
    void collectLabels();
    void emitPrologue();
    void emitEpilogue();
    void emitLabel(size_t pc);
    void emitUndefinedLabels();

    void pushReg(const char *reg);
    void popReg(const char *reg);
    std::string makeLabel(uint32_t targetPc) {
        std::string lbl = "L" + std::to_string(targetPc);
        referencedLabels.insert(lbl);
        return lbl;
    }
    void emitLabel(const std::string &name) {
        definedLabels.insert(name);
    }
};

// implementation
void snail::Translator::collectLabels() {
    this->idx = 0;
    while (this->idx < this->code.size()) {
        uint8_t opcode = this->code[this->idx];
        if (opcode == 0x00) { // FUNC
            this->labelAtPc[this->idx] = "func_" + std::to_string(this->labelCounter++);
        }
        this->idx++;
    }
}

void snail::Translator::emitPrologue() {
    this->asmText << "\t.section __TEXT,__text\n";
    this->asmText << "\t.globl _init_globals\n";
    this->asmText << "_init_globals:\n";
    this->asmText << "\tret\n\n";
    
    // Emit function prologues
    if (this->functionNames) {
        for (size_t i = 0; i < this->functionNames->size(); ++i) {
            this->asmText << "\t.section __TEXT,__text\n";
            this->asmText << "\t.globl _fun_" << i << "\n";
            this->asmText << "_fun_" << i << ":\n";
            this->asmText << "\tstp x29, x30, [sp, #-16]!\n";
            this->asmText << "\tmov x29, sp\n";
            this->asmText << "\tsub sp, sp, #4096\n";
            this->asmText << "\tmov x20, sp\n";
            this->asmText << "\tstr x0, [sp, #-8]!\n"; // Save x0
            this->asmText << "\tstr x1, [sp, #-8]!\n"; // Save x1
            this->asmText << "\tstr x2, [sp, #-8]!\n"; // Save x2
            this->asmText << "\tstr x3, [sp, #-8]!\n"; // Save x3
        }
    }
    
    this->asmText << "\t.section __TEXT,__text\n";
    this->asmText << "\t.globl _main\n";
    this->asmText << "_main:\n";
    this->asmText << "\t// Main function prologue\n";
    this->asmText << "\tstp x29, x30, [sp, #-16]!\n";
    this->asmText << "\tmov x29, sp\n";
    this->asmText << "\t// Save x0-x3\n";
    this->asmText << "\tstp x0, x1, [sp, #-16]!\n";
    this->asmText << "\tstp x2, x3, [sp, #-16]!\n";
}

void snail::Translator::emitEpilogue() {
    emitUndefinedLabels();
    this->asmText << "\t// Main function epilogue\n";
    this->asmText << "\tldr w0, [x20], #4\n";  // get return value
    this->asmText << "\t// Restore x2-x3 then x0-x1 (paired loads)\n";
    this->asmText << "\tldp x2, x3, [sp], #16\n";
    this->asmText << "\tldp x0, x1, [sp], #16\n";   // Restore x0
    this->asmText << "\tldp x29, x30, [sp], #16\n"; // Restore frame
    this->asmText << "\tret\n\n";
    
    // Emit function epilogues
    if (this->functionNames) {
        for (size_t i = 0; i < this->functionNames->size(); ++i) {
            this->asmText << "\t.section __TEXT,__text\n";
            this->asmText << "\t.globl _fun_" << i << "_epilogue\n";
            this->asmText << "_fun_" << i << "_epilogue:\n";
            this->asmText << "\t// Function epilogue\n";
            this->asmText << "\tldr w0, [x20], #4\n";  // get return value
            this->asmText << "\tmov sp, x29\n";
            this->asmText << "\tldp x29, x30, [sp], #16\n";
            this->asmText << "\tldr x0, [sp], #8\n"; // Restore x0
            this->asmText << "\tldr x1, [sp], #8\n"; // Restore x1
            this->asmText << "\tldr x2, [sp], #8\n"; // Restore x2
            this->asmText << "\tldr x3, [sp], #8\n"; // Restore x3
            this->asmText << "\tret\n\n";
        }
    }
}

void snail::Translator::emitUndefinedLabels() {
    for (const auto &label : referencedLabels) {
        if (definedLabels.find(label) == definedLabels.end()) {
            this->asmText << label << ":\n";
        }
    }
}

void snail::Translator::emitLabel(size_t pc) {
    if (this->labelAtPc.count(pc)) {
        const std::string &lbl = this->labelAtPc[pc];
        this->asmText << lbl << "\n";
        definedLabels.insert(lbl);
    } else {
        std::string label = "L" + std::to_string(this->labelCounter++);
        this->labelAtPc[pc] = label;
        definedLabels.insert(label);
        this->asmText << label << ":\n";
    }
}

void snail::Translator::pushReg(const char *reg) {
    // Push register value onto VM stack (always 8-byte slot for alignment)

    this->asmText << "// Pushing " << reg << " to stack\n";
    bool is64 = (reg[0] == 'x');
    // Make space for 8 bytes (slot)
    this->asmText << "sub sp, sp, #8\n";
    if(is64){
        this->asmText << "str " << reg << ", [sp]\n";
    } else {
        // store 32-bit value at upper 4 bytes to keep endianness simple
        this->asmText << "str " << reg << ", [sp, #4]\n";
    }
}

void snail::Translator::popReg(const char *reg) {
    // Pop value from VM stack, assuming 8-byte slot alignment

    this->asmText << "// Popping " << reg << " from stack\n";
    bool is64 = (reg[0] == 'x');
    if(is64){
        this->asmText << "ldr " << reg << ", [sp]\n";
    } else {
        this->asmText << "ldr " << reg << ", [sp, #4]\n";
    }
    // release full 8-byte slot
    this->asmText << "add sp, sp, #8\n";
}

void snail::Translator::collectFunctionNames() {
    if (this->functionNames) {
        for (size_t i = 0; i < this->functionNames->size(); ++i) {
            if (this->functionNames->at(i).empty()) {
                std::string& name = const_cast<std::string&>(this->functionNames->at(i));
                name = "_fun_" + std::to_string(i);
            }
        }
    }
}

std::string snail::Translator::translate() {
    bool debugBrk = std::getenv("SNAIL_DEBUG_BRK") != nullptr;
    emitPrologue();
    this->asmText << "\tsub sp, sp, #4096\n";
    this->asmText << "\tmov x20, sp\n";
    this->asmText << "\t// Save x0-x3 for VM main frame\n";
    this->asmText << "\tstp x0, x1, [sp, #-16]!\n";
    this->asmText << "\tstp x2, x3, [sp, #-16]!\n";

    size_t pc = 0;
        // Helper to create/retrieve a label for a given PC
        auto ensureLabel = [this](size_t pcTarget) -> std::string {
            auto it = this->labelAtPc.find(pcTarget);
            if (it != this->labelAtPc.end()) return it->second;
            std::string lbl = "L" + std::to_string(this->labelCounter++);
            this->labelAtPc[pcTarget] = lbl;
            referencedLabels.insert(lbl);
            return lbl;
        };
    while (pc < this->code.size()) {
        emitLabel(pc);
        uint8_t op = this->code[pc++];
        this->asmText << "\t// PC=" << pc << " OP=" << static_cast<int>(op) << "\n";
        if(debugBrk){ this->asmText << "\tbrk #0\n"; }
        switch (op) {
            // -------- stack/memory ---------
            case Op::NOP: {
                this->asmText << "\t// NOP\n";
                break;
            }
            case Op::PUSH_CONST: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                if (this->strLabels && idx < this->strLabels->size() && !this->strLabels->at(idx).empty()) {
                    const std::string &lbl = this->strLabels->at(idx);
                    this->asmText << "\t// PUSH_CONST string idx=" << idx << "\n";
                    this->asmText << "\tadrp x0, " << lbl << "@PAGE\n";
                    this->asmText << "\tadd  x0, x0, " << lbl << "@PAGEOFF\n";
                    pushReg("x0");
                } else {
                    uint32_t v = (this->constInts && idx < this->constInts->size()) ? this->constInts->at(idx) : idx;
                    this->asmText << "\t// PUSH_CONST " << idx << " = " << v << "\n";
                    this->asmText << "\tmov w0, #" << v << "\n";
                    pushReg("w0");
                }
                break;
            }
            case Op::PUSH_LOCAL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// PUSH_LOCAL " << idx << "\n";
                this->asmText << "\tldr w0, [x29, #" << ( -4 * (idx + 1) ) << "]\n";
                pushReg("w0");
                break;
            }
            case Op::PUSH_GLOBAL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// PUSH_GLOBAL " << idx << "\n";
                this->asmText << "\tldr w0, _globals+" << idx*4 << "\n";
                pushReg("w0");
                break;
            }
            case Op::STORE_LOCAL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// STORE_LOCAL " << idx << "\n";
                popReg("w0");
                this->asmText << "\tstr w0, [x29, #" << ( -4 * (idx + 1) ) << "]\n";
                break;
            }
            case Op::STORE_GLOBAL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// STORE_GLOBAL " << idx << "\n";
                popReg("w0");
                this->asmText << "\tstr w0, _globals+" << idx*4 << "\n";
                break;
            }
            // ---------------- added opcode implementations ----------------
            case Op::POP: {
                this->asmText << "\t// POP\n";
                popReg("w0");
                break;
            }
            case Op::DUP: {
                this->asmText << "\t// DUP\n";
                this->asmText << "\tldr w0, [sp]\n";
                pushReg("w0");
                break;
            }
            case Op::ADD: {
                popReg("w1");
                popReg("w0");
                this->asmText << "\tadd w0, w0, w1\n";
                pushReg("w0");
                break;
            }
            case Op::SUB: {
                popReg("w1");
                popReg("w0");
                this->asmText << "\tsub w0, w0, w1\n";
                pushReg("w0");
                break;
            }
            case Op::MUL: {
                popReg("w1");
                popReg("w0");
                this->asmText << "\tmul w0, w0, w1\n";
                pushReg("w0");
                break;
            }
            case Op::DIV: {
                popReg("w1");
                popReg("w0");
                this->asmText << "\tsdiv w0, w0, w1\n";
                pushReg("w0");
                break;
            }
            case Op::MOD: {
                popReg("w1");
                popReg("w0");
                this->asmText << "\tsdiv w2, w0, w1\n";
                this->asmText << "\tmsub w0, w2, w1, w0\n";
                pushReg("w0");
                break;
            }
            case Op::EQ: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, eq\n";
                pushReg("w0"); break;
            }
            case Op::NEQ: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, ne\n";
                pushReg("w0"); break;
            }
            case Op::LT: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, lt\n";
                pushReg("w0"); break;
            }
            case Op::LE: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, le\n";
                pushReg("w0"); break;
            }
            case Op::GT: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, gt\n";
                pushReg("w0"); break;
            }
            case Op::GTE: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tcmp w0, w1\n";
                this->asmText << "\tcset w0, ge\n";
                pushReg("w0"); break;
            }
            case Op::AND: {
                popReg("w1"); popReg("w0");
                this->asmText << "\tand w0, w0, w1\n";
                pushReg("w0"); break;
            }
            case Op::OR: {
                popReg("w1"); popReg("w0");
                this->asmText << "\torr w0, w0, w1\n";
                pushReg("w0"); break;
            }
            case Op::NOT: {
                popReg("w0");
                this->asmText << "\tcmp w0, #0\n";
                this->asmText << "\tcset w0, eq\n";
                pushReg("w0"); break;
            }
            case Op::JMP: {
                int16_t off = be16s(&this->code[pc]); pc+=2; size_t tgt = pc + off; std::string lbl = ensureLabel(tgt); this->asmText << "\tb " << lbl << "\n"; break;}
            case Op::JMP_IF_FALSE: {
                int16_t off = be16s(&this->code[pc]); pc+=2; popReg("w0"); size_t tgt = pc + off; std::string lbl = ensureLabel(tgt); this->asmText << "\tcmp w0, #0\n" << "\tbeq " << lbl << "\n"; break; }
            case Op::JMP_IF_TRUE: {
                int16_t off = be16s(&this->code[pc]); pc+=2; popReg("w0"); size_t tgt = pc + off; std::string lbl = ensureLabel(tgt); this->asmText << "\tcmp w0, #0\n" << "\tbne " << lbl << "\n"; break; }
            case Op::NEW_ARRAY: {
                uint32_t sz = be32(&this->code[pc]); pc+=4; uint8_t t = this->code[pc++]; (void)t; this->asmText << "\tmov w0, #"<< (sz*4) <<"\n\tbl _malloc\n"; pushReg("x0"); break; }
            case Op::GET_ARRAY: {
                popReg("x1"); popReg("x0"); this->asmText << "\tldr w0, [x0, x1, lsl #2]\n"; pushReg("w0"); break; }
            case Op::SET_ARRAY: {
                popReg("w2"); popReg("x1"); popReg("x0"); this->asmText << "\tstr w2, [x0, x1, lsl #2]\n"; break; }
            case Op::INIT_ARRAY: {
                uint32_t n = be32(&this->code[pc]); pc+=4; popReg("x1"); this->asmText << "\tmov w2, #"<<n<<"\n"; std::string loop = "Linit_"+std::to_string(this->labelCounter++); std::string done=loop+"_done"; this->asmText<< loop <<":\n\tcmp w2,#0\n\tbeq "<<done<<"\n"; popReg("w0"); this->asmText << "\tsub w3,w2,#1\n\tstr w0,[x1,w3,uxtw #2]\n\tsub w2,w2,#1\n\tb "<<loop<<"\n"<<done<<":\n"; pushReg("x1"); break; }
            case Op::INTRINSIC_CALL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// INTRINSIC_CALL " << idx << "\n";
                if (idx == 0) { // println intrinsic – assume int32 value on stack
                    static bool fmtDefined = false;
                    if (!fmtDefined) {
                        // define format string once in cstring section
                        this->asmText << "\t.section __TEXT,__cstring,cstring_literals\n";
                        this->asmText << "Lfmt_int:\n\t.asciz \"%d\\n\"\n";
                        fmtDefined = true;
                        // return to text section for subsequent code
                        this->asmText << "\t.section __TEXT,__text\n";
                    }
                    popReg("w0"); // value to print
                    // move value into w1 argument register
                    this->asmText << "\tmov w1, w0\n";
                    // load address of format string into x0
                    this->asmText << "\tadrp x0, Lfmt_int@PAGE\n";
                    this->asmText << "\tadd  x0, x0, Lfmt_int@PAGEOFF\n";
                    // w1 already holds arg, ensure it is in x1 (upper cleared automatically)

                    this->asmText << "\tbl _printf\n";
                } else {
                    this->asmText << "\t// Unsupported intrinsic index, skipping\n";
                }
                break;
            }
            case Op::HALT: {
                emitEpilogue(); return this->asmText.str(); }

            // end inserted opcodes
            case Op::CALL: {
                uint16_t idx = be16(&this->code[pc]); pc += 2;
                this->asmText << "\t// CALL " << idx << "\n";
                // Save current frame
                this->asmText << "\tstp x29, x30, [sp, #-16]!\n";
                this->asmText << "\tmov x29, sp\n";
                // Load argument
                popReg("x0");
                // Call function
                this->asmText << "\tbl _fun_" << idx << "\n";
                // Save return value
                this->asmText << "\tmov x1, w0\n";
                // Restore frame
                this->asmText << "\tldp x29, x30, [sp], #16\n";
                // Push return value
                this->asmText << "\tmov w0, x1\n";
                pushReg("w0");
                break;
            }
            case Op::RET: {
                popReg("w0"); // return value through w0
                // emit missing labels before finishing function
                for (const auto &lbl : referencedLabels) {
                    if (!definedLabels.count(lbl)) {
                        this->asmText << lbl << ":\n";
                        definedLabels.insert(lbl);
                    }
                }
                emitEpilogue();
                return this->asmText.str();
            }
            default:
                std::cerr << "Unknown opcode 0x" << std::hex << int(op) << " at pc=" << pc-1 << "\n";
                return this->asmText.str();
        }
    }
    // emit any missing labels at end
    for (const auto &lbl : referencedLabels) {
        if (!definedLabels.count(lbl)) {
            this->asmText << lbl << ":\n";
            definedLabels.insert(lbl);
        }
    }
    emitEpilogue();
    return this->asmText.str();
}

#if 0
int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <bytecode file>\n";
        return 1;
    }

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open file " << argv[1] << "\n";
        return 1;
    }

    std::vector<uint8_t> bytecode;
    uint8_t byte;
    while (in.read(reinterpret_cast<char*>(&byte), 1)) {
        bytecode.push_back(byte);
    }
}
 {
    popReg("w2"); // value
    popReg("x1"); // index
    popReg("x0"); // array ptr
    asmText << "\tstr w2, [x0, x1, lsl #2]\n";
    break;
}
 {
    uint32_t n = be32(&code[pc]); pc += 4;
    asmText << "\t// INIT_ARRAY " << n << "\n";
    // Pop array ptr into x1
    popReg("x1");
    // Load element count
    asmText << "\tmov w2, #" << n << "\n";
    std::string lbl = "Linit_" + std::to_string(labelCounter.length());
    asmText << lbl << ":\n";
    asmText << "\tcmp w2, #0\n";
    asmText << "\tbeq " << lbl << "_done\n";
    // Pop value
    popReg("w0");
    // Store value into array [x1, (w2-1)*4]
    asmText << "\tsub w3, w2, #1\n";
    asmText << "\tstr w0, [x1, w3, uxtw #2]\n";
    asmText << "\tsub w2, w2, #1\n";
    asmText << "\tb " << lbl << "\n";
    asmText << lbl << "_done:\n";
    // finally push array pointer back (so it stays on stack)
    pushReg("x1");
    break;
}

default:
    std::cerr << "Unknown opcode 0x" << std::hex << int(op) << " at pc=" << pc-1 << "\n";
    return asmText.str();
}


emitEpilogue();
return asmText.str();
}
}
#endif
}

#if 0
#if __has_include(<filesystem>)
#include <filesystem>
#else
#include <experimental/filesystem>
#endif
using fs = std::filesystem;
int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <bytecode.bin> [output .S]" << std::endl;
        return 1;
    }
    fs::path inPath(argv[1]);
    const char* outPath = (argc == 3) ? argv[2] : "translated.S";

    try {
        vm::code::Reader reader(inPath);
        reader.read_header();
        auto constPool = reader.read_constants();
        vm::memory::Allocator dummyAlloc;
        reader.read_globals(dummyAlloc);
        auto funcTable = reader.read_functions();
        // Choose first function as entry (index 0) or "main" if named
        size_t funcIndex = 0;
        for (size_t i = 0; i < funcTable.size; ++i) {
            if (i < funcTable.function_names.size() && funcTable.function_names[i] == "main") {
                funcIndex = i; break; }
        }
        const auto& fn = funcTable.functions[funcIndex];
        reader.set_offset(fn.offset);
        std::vector<uint8_t> codeBytes(fn.length);
        for (size_t i = 0; i < fn.length; ++i) codeBytes[i] = reader.read_byte();

        // extract int32 constants for translator
        std::vector<uint32_t> intConsts;
        for (u16 idx = 0; idx < constPool.size; ++idx) {
            if (constPool.data[idx]->type == vm::runtime::Type::I32) {
                uint32_t val; std::memcpy(&val, constPool.data[idx]->data, 4);
                intConsts.push_back(val);
            }
        }

        snail::Translator translator(codeBytes, &intConsts, nullptr, &funcTable.function_names);
        std::string asmText = translator.translate();
            if (!out) {
                    std::cerr << "Cannot write output file " << outPath << std::endl;
        return 1;
    }
    out << asmText;
    out.close();
    return 0;
}
#endif

// Clean command-line translator implementation
#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#else
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#endif
int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <bytecode.bin> [output .S]" << std::endl;
        return 1;
    }
    fs::path inPath(argv[1]);
    const char* outPath = (argc == 3) ? argv[2] : "translated.S";
    try {
        vm::code::Reader reader(inPath);
        reader.read_header();
        auto constPool = reader.read_constants();
        vm::memory::Allocator dummyAlloc;
        reader.read_globals(dummyAlloc);
        auto funcTable = reader.read_functions();
        size_t funcIndex = 0;
        for (size_t i = 0; i < funcTable.size && i < funcTable.function_names.size(); ++i) {
            if (funcTable.function_names[i] == "main") { funcIndex = i; break; }
        }
        const auto& fn = funcTable.functions[funcIndex];
        reader.set_offset(fn.offset);
        std::vector<uint8_t> codeBytes(fn.length);
        for (size_t i = 0; i < fn.length; ++i) codeBytes[i] = reader.read_byte();
        std::vector<uint32_t> intConsts;
        for (u16 idx = 0; idx < constPool.size; ++idx) {
            if (constPool.data[idx]->type == vm::runtime::Type::I32) {
                uint32_t val; std::memcpy(&val, constPool.data[idx]->data, 4);
                intConsts.push_back(val);
            }
        }
        snail::Translator translator(codeBytes, &intConsts, nullptr, &funcTable.function_names);
        std::string asmText = translator.translate();
        std::ofstream out(outPath);
        if (!out) {
            std::cerr << "Cannot write output file " << outPath << std::endl;
            return 1;
        }
        out << asmText;
        std::cout << "Assembly written to " << outPath << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}




