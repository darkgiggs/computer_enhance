#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include "sim86_shared.h"
#pragma comment (lib, "sim86_shared_debug.lib")
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define RegisterNumber 15

s16 Registers[RegisterNumber] = {};

char const* RegisterNames[][3] =
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

void SimulateInstruction(instruction Instruction)
{
    const char* Op = Sim86_MnemonicFromOperationType(Instruction.Op);
    
    switch (Instruction.Op)
    {
        case Op_mov:
        {
            instruction_operand & Dest = Instruction.Operands[0];
            instruction_operand & Source = Instruction.Operands[1];
            s32 Value; 
            /* 
            When accessing the high 8 bits in a register, we use
            a bit shift right by 8 to store the value.
            If we use a 16bit signed integer and the high bit is 
            set, depending on implementation, we may copy
            the high bit as we shift.
            */
            switch (Source.Type)
            {
            case Operand_Register:
            {
                Value = Registers[Source.Register.Index];
                if (Source.Register.Offset == 1) // Accessing high 8 bits
                {
                    Value >>= 8;
                }
                else if (Source.Register.Count == 1) // Accessing low 8 bits
                {
                    Value &= 0xFF;
                }
            } break;
            case Operand_Immediate:
            {
                Value = Source.Immediate.Value;
            } break;
            }

            Value = (s16)Value;

            switch (Dest.Type)
            {
            case Operand_Register:
            {
                if (Dest.Register.Offset == 1) // Writing high 8 bits
                {
                    s16 Temp = Registers[Dest.Register.Index];
                    Temp &= 0xFF;
                    Value <<= 8;
                    Registers[Dest.Register.Index] = Temp | Value;
                }
                else if (Dest.Register.Count == 1) // Accessing low 8 bits
                {
                    Value &= 0xFF;
                    s16 Temp = Registers[Dest.Register.Index];
                    Temp &= 0xFF00;
                    Registers[Dest.Register.Index] = Temp | Value;
                }
                else
                {
                    Registers[Dest.Register.Index] = Value;
                }
            } break;
            }
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