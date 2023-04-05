#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include "sim86_shared.h"
#pragma comment (lib, "sim86_shared_debug.lib")
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

static constexpr int RegisterNumber = 15;
static s16 Registers[RegisterNumber] = {};
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

static constexpr int FlagNumber = 9;
static bool Flags[FlagNumber] = {};
static char const* FlagNames = { "ODITSZAPC" };
static enum Flags
{
    OF, DF, IF, TF, SF, ZF, AF, PF, CF
};

static void SetFlags(u16 Result)
{
    if (Result && 0x8000)
    {
        Flags[SF] = true;
    }
    else
    {
        Flags[SF] = false;
    }
    if (Result == 0)
    {
        Flags[ZF] = true;
    }
    else
    {
        Flags[ZF] = false;
    }
}

static void SimulateInstruction(const instruction& Instruction)
{
    const char* Op = Sim86_MnemonicFromOperationType(Instruction.Op);
    
    switch (Instruction.Op)
    {
        case Op_mov:
        {
            instruction_operand& Dest = Instruction.Operands[0];
            instruction_operand& Source = Instruction.Operands[1];
            u16 Value; 
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
            }

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
            }
        } break;
        case Op_add:
        case Op_sub:
        case Op_cmp:
        {
            const instruction_operand& Dest = Instruction.Operands[0];
            const instruction_operand& Source = Instruction.Operands[1];
            u16 Value;
            u16 Result;
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
            }

            switch (Dest.Type)
            {
                case Operand_Register:
                {
                    
                    if (Dest.Register.Count == 1) // Accessing half registers
                    {
                        u8* DestPointer = reinterpret_cast<u8*>(&Registers[Dest.Register.Index]) + Dest.Register.Offset;
                        Result = *DestPointer + Value;
                        *DestPointer = static_cast<u8>(Result);
                    }
                    else
                    {
                        Registers[Dest.Register.Index] += Value;
                        Result = Registers[Dest.Register.Index];
                    }
                } break;
            }
            SetFlags(Result);
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
            std::string OutputBuffer;
            char* FileName = Args[ArgIndex];
            std::ifstream File;
            File.open(FileName, std::ifstream::binary | std::ifstream::in);
            if (!File.good())
            {
                std::cout << "Error opening file " << FileName;
                return -1;
            }
            OutputBuffer = OutputBuffer + FileName + "\n";
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
                    SimulateInstruction(Decoded);
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
        }
    } 
}