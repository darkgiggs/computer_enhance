#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <assert.h>
#include "sim86_shared.h"
#pragma comment (lib, "sim86_shared_debug.lib")
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

static constexpr int RegisterNumber = 15;
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

static void SetFlags(u16 Result, bool* FlagArray)
{
    FlagArray[Flag_SF] = Result & 0x8000;
    FlagArray[Flag_ZF] = (Result == 0);
}

static void PrintFlags(bool* FlagArray)
{
    std::string OutputBuffer = "Flags: ";
    for (u8 i = 0; i < Flag_count; i++)
    {
        if (FlagArray[i])
        {
            OutputBuffer += FlagNames[i];
        }
    }
    std::cout << OutputBuffer << '\n';
}

static u16 GetSourceValue(const instruction_operand& Source, const s16* Registers)
{
    u16 Value = 0xFFFF;

    switch (Source.Type)
    {
        case Operand_Register:
        {
            Value = Registers[Source.Register.Index];
            if (Source.Register.Count == 1) // Accessing half registers
            {
                Value >>= 8 * Source.Register.Offset;
            }
        } break;
        case Operand_Immediate:
        {
            Value = Source.Immediate.Value;
        } break;
        default:
        {
            assert(false);
        } break;
    }

    return Value;
}

static void SimulateInstruction(const instruction& Instruction, s16* Registers, bool* FlagArray)
{
    const char* Op = Sim86_MnemonicFromOperationType(Instruction.Op);
    
    switch (Instruction.Op)
    {
        case Op_mov:
        {
            const instruction_operand& Dest = Instruction.Operands[0];
            const instruction_operand& Source = Instruction.Operands[1];
            u16 Value = GetSourceValue(Source, Registers);
           
            switch (Dest.Type)
            {
                case Operand_Register:
                {
                    if (Dest.Register.Count == 1) // Accessing half registers
                    {
                        u8* DestPointer = reinterpret_cast<u8*>(&Registers[Dest.Register.Index]) + Dest.Register.Offset;
                        *DestPointer = static_cast<u8>(Value);
                    }
                    else
                    {
                        Registers[Dest.Register.Index] = Value;
                    }
                } break;
                default:
                {
                    assert(false);
                } break;
            }
        } break;
        case Op_add:
        case Op_sub:
        case Op_cmp: [[fallthrough]];
        {
            const instruction_operand& Dest = Instruction.Operands[0];
            const instruction_operand& Source = Instruction.Operands[1];
            u16 Value = GetSourceValue(Source, Registers);
            u16 Result = 0xFFFF;

            switch (Dest.Type)
            {
                case Operand_Register:
                {
                    
                    if (Dest.Register.Count == 1) // Accessing half registers
                    {
                        u8* DestPointer = reinterpret_cast<u8*>(&Registers[Dest.Register.Index]) + Dest.Register.Offset;

                        switch (Instruction.Op)
                        {
                            case Op_add:
                            {
                                Result = *DestPointer + Value;
                                *DestPointer = static_cast<u8>(Result);
                            } break;
                            case Op_sub:
                            {
                                Result = *DestPointer - Value;
                                *DestPointer = static_cast<u8>(Result);
                            } break;
                            case Op_cmp:
                            {
                                Result = *DestPointer - Value;
                            } break;
                            default:
                            {
                                assert(false);
                            } break;
                        }
                    }
                    else
                    {
                        switch (Instruction.Op)
                        {
                            case Op_add:
                            {
                                Registers[Dest.Register.Index] += Value;
                                Result = Registers[Dest.Register.Index];
                            } break;
                            case Op_sub:
                            {
                                Registers[Dest.Register.Index] -= Value;
                                Result = Registers[Dest.Register.Index];
                            } break;
                            case Op_cmp:
                            {
                                Result = Registers[Dest.Register.Index] - Value;
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
            SetFlags(Result, FlagArray);
#if _DEBUG
            PrintFlags(FlagArray);
#endif
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
  
    if (ArgCount > 1)
    { 
        for (int ArgIndex = 1; ArgIndex < ArgCount; ArgIndex++)
        {
            s16 Registers[RegisterNumber] = {};
            bool FlagArray[Flag_count] = {};

            std::string OutputBuffer;
            char* FileName = Args[ArgIndex];
            std::ifstream File;
            File.open(FileName, std::ifstream::binary | std::ifstream::in);
            if (!File.good())
            {
                std::cout << "Error opening file " << FileName;
                return -1;
            }
            
            std::cout << "\n" << FileName << "\n";
            std::vector<u8> Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            u32 BytesRead = Bytes.size();

            u32 Offset = 0;
            while (Offset < BytesRead)
            {
                instruction Decoded;
                Sim86_Decode8086Instruction(BytesRead - Offset, &Bytes[0] + Offset, &Decoded);

                if (Decoded.Op)
                {
                    Offset += Decoded.Size;
                    SimulateInstruction(Decoded, Registers, FlagArray);
                }
                else
                {
                    std::cout << "Unrecognized instruction\n";
                    break;
                }

            }

            for (int i = 1; i < RegisterNumber; i++)
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
            std::cout << OutputBuffer;
            PrintFlags(FlagArray);
        }
    } 
}