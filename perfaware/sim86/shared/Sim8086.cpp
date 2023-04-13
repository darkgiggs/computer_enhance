#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <string_view>
#include <assert.h>
#include "sim86_shared.h"
#pragma comment (lib, "sim86_shared_debug.lib")
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

static constexpr size_t MEGABYTE = 1024 * 1024;
static constexpr int REGISTER_COUNT = 15;
static constexpr int IP_REGISTER = 13;
static constexpr int CX_REGISTER = 3;
static constexpr u16 INVALID_VALUE = 0xFFFF;
static char const* RegisterNames[][3] =
{
    {"", "", ""},
    {"al", "ah", "ax"},
    {"bl", "bh", "bx"},
    {"cl", "ch", "cx"},
    {"dl", "dh", "dx"},
    {"sp", "sp", "sp"},
    {"bp", "bp", "bp"},
    {"si", "si", "si"},
    {"di", "di", "di"},
    {"es", "es", "es"},
    {"cs", "cs", "cs"},
    {"ss", "ss", "ss"},
    {"ds", "ds", "ds"},
    {"ip", "ip", "ip"},
    {"flags", "flags", "flags"}
};
static char const* FlagNames = { "ODITSZAPC" };
enum Flags
{
    Flag_OF,
    Flag_DF,
    Flag_IF,
    Flag_TF,
    Flag_SF,
    Flag_ZF,
    Flag_AF,
    Flag_PF,
    Flag_CF,

    Flag_count
};

static u8 Memory[MEGABYTE];

static void SetFlags(const instruction& Instruction, const u16 LeftOperandValue, 
    const u16 RightOperandValue, const u16 Result, bool* FlagArray)
{   
    u16 HighOrderBit = (Instruction.Operands[1].Register.Count == 1) ? 0x80 : 0x8000;
    bool Parity = true;
    
    FlagArray[Flag_SF] = Result & HighOrderBit;
    FlagArray[Flag_ZF] = (Result == 0);
    for (u16 i = 0; i < 8; i++)
    {
        if (Result & (1 << i))
        {
            Parity = !Parity;
        }
    }
    FlagArray[Flag_PF] = Parity;

    switch (Instruction.Op)
    {
        case Op_add:
        {
            FlagArray[Flag_CF] = ((LeftOperandValue & HighOrderBit) || (RightOperandValue & HighOrderBit))
                && !(Result & HighOrderBit);
            FlagArray[Flag_AF] = ((LeftOperandValue & 0x8) || (RightOperandValue & 0x8))
                && !(Result & 0x8);
            FlagArray[Flag_OF] = (~(LeftOperandValue ^ RightOperandValue) & (Result ^ LeftOperandValue)) & HighOrderBit;
        } break;
        case Op_sub: [[fallthrough]];
        case Op_cmp:
        {
            FlagArray[Flag_CF] = RightOperandValue > LeftOperandValue;
            FlagArray[Flag_AF] = (RightOperandValue & 0xF) > (LeftOperandValue & 0xF);
            FlagArray[Flag_OF] = ((LeftOperandValue ^ RightOperandValue) & ~Result) & HighOrderBit;
        } break;
        default:
        {
            assert(false);
        } break;
    }
}    

static void PrintFlags(const bool* FlagArray)
{
    std::string OutputBuffer = "Flags: ";
    for (size_t i = 0; i < Flag_count; i++)
    {
        if (FlagArray[i])
        {
            OutputBuffer += FlagNames[i];
        }
    }
    std::cout << OutputBuffer << std::endl;
}

static inline size_t ComputeEffectiveAddress(const instruction_operand& Operand, const s16* Registers)
{
    size_t Result = 0;

    Result += Registers[Operand.Address.Terms[0].Register.Index];
    Result += Registers[Operand.Address.Terms[1].Register.Index];
    Result += Operand.Address.Displacement;

    return Result;
}

static u16 GetRightOperandValue(const instruction_operand& Source, const s16* Registers)
{
    u16 RightOperandValue = INVALID_VALUE;

    switch (Source.Type)
    {
        case Operand_Register:
        {
            RightOperandValue = Registers[Source.Register.Index];
            if (Source.Register.Count == 1) // Accessing half registers
            {
                RightOperandValue >>= 8 * Source.Register.Offset;
            }
        } break;
        case Operand_Immediate:
        {
            RightOperandValue = static_cast<u16>(Source.Immediate.Value);
        } break;
        case Operand_Memory:
        {
            size_t EffectiveAddress = ComputeEffectiveAddress(Source, Registers);
            if (Source.Register.Count == 2) // word value
            {
                u16* SourcePointer = reinterpret_cast<u16*>(&Memory[EffectiveAddress]);
                RightOperandValue = *SourcePointer;
            }
            else
            {
                RightOperandValue = Memory[EffectiveAddress];
            }
        } break;
        default:
        {
            assert(false);
        } break;
    }

    return RightOperandValue;
}

static void SimulateInstruction(const instruction& Instruction, s16* Registers, bool* FlagArray)
{
    switch (Instruction.Op)
    {
        case Op_mov:
        {
            const instruction_operand& Dest = Instruction.Operands[0];
            const instruction_operand& Source = Instruction.Operands[1];
            u16 RightOperandValue = GetRightOperandValue(Source, Registers);
           
            switch (Dest.Type)
            {
                case Operand_Register:
                {
                    if (Dest.Register.Count == 1) // Accessing half registers
                    {
                        u8* DestPointer = reinterpret_cast<u8*>(&Registers[Dest.Register.Index]) + Dest.Register.Offset;
                        *DestPointer = static_cast<u8>(RightOperandValue);
                    }
                    else
                    {
                        Registers[Dest.Register.Index] = RightOperandValue;
                    }
                } break;
                case Operand_Memory:
                {
                    size_t EffectiveAddress = ComputeEffectiveAddress(Dest, Registers);
                    if (Dest.Register.Count == 2) // word value
                    {
                        u16* DestPointer = reinterpret_cast<u16*>(&Memory[EffectiveAddress]);                            
                        *DestPointer = RightOperandValue;
                    }
                    else
                    {
                        Memory[EffectiveAddress] = static_cast<u8>(RightOperandValue);
                    }
                } break;
                default:
                {
                    assert(false);
                } break;
            }
        } break;
        case Op_add: [[fallthrough]];
        case Op_sub: [[fallthrough]];
        case Op_cmp: 
        {
            const instruction_operand& Dest = Instruction.Operands[0];
            const instruction_operand& Source = Instruction.Operands[1];
            u16 RightOperandValue = GetRightOperandValue(Source, Registers);
            u16 LeftOperandValue = INVALID_VALUE;
            u16 Result = INVALID_VALUE;

            switch (Dest.Type)
            {
                case Operand_Register:
                {
                    
                    if (Dest.Register.Count == 1) // Accessing half registers
                    {
                        u8* DestPointer = reinterpret_cast<u8*>(&Registers[Dest.Register.Index]) + Dest.Register.Offset;
                        LeftOperandValue = *DestPointer;

                        switch (Instruction.Op)
                        {
                            case Op_add:
                            {
                                Result = LeftOperandValue + RightOperandValue;
                                *DestPointer = static_cast<u8>(Result);
                            } break;
                            case Op_sub:
                            {
                                Result = LeftOperandValue - RightOperandValue;
                                *DestPointer = static_cast<u8>(Result);
                            } break;
                            case Op_cmp:
                            {
                                Result = LeftOperandValue - RightOperandValue;
                            } break;
                            default:
                            {
                                assert(false);
                            } break;
                        }
                    }
                    else
                    {
                        LeftOperandValue = Registers[Dest.Register.Index];
                        switch (Instruction.Op)
                        {
                            case Op_add:
                            {                           
                                Result = LeftOperandValue + RightOperandValue;
                                Registers[Dest.Register.Index] = Result;
                            } break;
                            case Op_sub:
                            {
                                Result = LeftOperandValue - RightOperandValue;
                                Registers[Dest.Register.Index] = Result;
                            } break;
                            case Op_cmp:
                            {
                                Result = LeftOperandValue - RightOperandValue;
                            } break;
                            default:
                            {
                                assert(false);
                            } break;
                        }
                        
                    }
                } break;
                default:
                {
                    assert(false);
                } break;
            }
            SetFlags(Instruction, LeftOperandValue, RightOperandValue, Result, FlagArray);
#if _DEBUG
            PrintFlags(FlagArray);
#endif
        } break;
        case Op_jne: // JNE/JNZ
        {
            if (!FlagArray[Flag_ZF])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        case Op_je:
        {
            if (FlagArray[Flag_ZF])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        case Op_jb:
        {
            if (FlagArray[Flag_CF])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        case Op_jp:
        {
            if (FlagArray[Flag_PF])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        case Op_loopnz:
        {
            Registers[CX_REGISTER] -= 1;
            if (!FlagArray[Flag_ZF] && Registers[CX_REGISTER])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        case Op_loop:
        {
            Registers[CX_REGISTER] -= 1;
            if (Registers[CX_REGISTER])
            {
                Registers[IP_REGISTER] += static_cast<s8>(Instruction.Operands[0].Immediate.Value);
            }
        } break;
        default:
        {
            assert(false);
        } break;
    }
}

int main(int ArgCount, char** Args)
{
    u32 Version = Sim86_GetVersion();
    printf("Sim86 Version: %u (expected %u)\n", Version, SIM86_VERSION);
    if (Version != SIM86_VERSION)
    {
        printf("ERROR: Header file version doesn't match DLL.\n");
        return -1;
    }

    instruction_table Table;
    Sim86_Get8086InstructionTable(&Table);
    printf("8086 Instruction Instruction Encoding Count: %u\n", Table.EncodingCount);
    
    bool DumpFile = false;
    
    int ArgIndex = 1;
    for (; ArgIndex < ArgCount; ArgIndex++)
    {
        if (std::string_view(Args[ArgIndex]) == std::string_view("-dump"))
        {
            DumpFile = true;
        }
        else
        {
            break;
        }
    }
    for (; ArgIndex < ArgCount; ArgIndex++)
    {
        std::string FileName = Args[ArgIndex];
        memset(Memory, 0, sizeof(Memory));
        s16 Registers[REGISTER_COUNT] = {};
        bool FlagArray[Flag_count] = {};
        
        std::string OutputBuffer;
        std::ifstream File;
        File.open(FileName, std::ifstream::binary | std::ifstream::in);
        if (!File.good())
        {
            std::cout << "Error opening file " << FileName << std::endl;
            continue;
        }

        std::cout << "\n" << FileName << std::endl;

        u16 BytesRead = 0;
        for (u8 Byte = static_cast<u8>(File.get()); !File.fail(); Byte = static_cast<u8>(File.get()))
        {
            Memory[BytesRead++] = Byte;
        }

        u16& InstructionPointer = reinterpret_cast<u16&>(Registers[IP_REGISTER]);

        while (InstructionPointer < BytesRead)
        {
            instruction Decoded;
            Sim86_Decode8086Instruction(BytesRead - InstructionPointer, Memory + InstructionPointer, &Decoded);

            if (Decoded.Op)
            {
                InstructionPointer += static_cast<u16>(Decoded.Size);
                SimulateInstruction(Decoded, Registers, FlagArray);
            }
            else
            {
                std::cout << "Unrecognized instruction" << std::endl;
                break;
            }
#if _DEBUG
            std::cout << Registers[IP_REGISTER] << std::endl;
#endif
        }

        for (size_t i = 1; i < REGISTER_COUNT; i++)
        {
            if (Registers[i] != 0)
            {
                std::stringstream Stream;
                Stream << RegisterNames[i][2]
                    << ": 0x" << std::hex << Registers[i]
                    << " (" << std::to_string(Registers[i]) << ")\n";
                OutputBuffer += Stream.str();
            }
        }
        std::cout << OutputBuffer << std::endl;
        PrintFlags(FlagArray);
        if (DumpFile)
        {
            std::string OutFileName = FileName + ".data";
            std::ofstream OutFile;
            OutFile.open(OutFileName, std::ofstream::binary);
            for (size_t Counter = 0; Counter < MEGABYTE; Counter++)
            {
                OutFile << Memory[Counter];
            }
            OutFile.close();
        }
    }
}