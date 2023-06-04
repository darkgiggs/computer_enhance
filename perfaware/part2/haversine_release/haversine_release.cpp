#include <iostream>
#include <fstream>
#include <format>
#include <assert.h>
#include "types.h"
#include "haversine_formula.h"

constexpr f64 EARTH_RADIUS = 6372.8;

int main(int argCount, char** args)
{
    bool properUsage = true;
    bool validation = false;
    
    char c;
    char value[30];
    char* pvalue = value;

    int coordCounter = 0;
    u64 pairCount = 0;

    f64 points[4];
    f64 sum = 0.0;


    if (argCount == 1 || argCount > 3)
    {
        properUsage = false;
    }
    else if (argCount == 3)
    {
        validation = true;
    }

    if (!properUsage)
    {
        std::cout
            << "Usage: haversine_release.exe [haversine_input.json]\n"
            << "       haversine_release.exe [haversine_input.json] [answer.f64]\n";
        return 0;
    }
    
    std::ifstream json(args[1], std::ifstream::in);
    if (!json.good())
    {
        std::cout << "Error opening json file";
        return 0;
    }

    while ((c = json.get()) != '[') //Seek to the table
    {
        ;
    }

    while ((c = json.get()) != ']')
    {
        if (c == ':')
        {
            while ((c = json.get()) != ',' && c != '}') 
            {
                *pvalue++ = c; 
                assert((pvalue - value) < 29);
                //NOTE(giggs) :
                // I expect this is slow and doing a memcpy on the whole number would be faster?
                // It might be more practical using std::from_chars on the memory range than using
                // atof?
            }
            *pvalue = '\0';
            points[coordCounter++] = atof(value);
            pvalue = value;

            if (coordCounter == 4)
            {
                sum += ReferenceHaversine(points[0], points[1], points[2], points[3], EARTH_RADIUS);
                pairCount++;
                coordCounter = 0;
            }
        }
    }

    json.close();

    sum /= pairCount;

    std::cout << std::vformat("Pair count: {}\nHaversine sum: {:.16f}\n", 
        std::make_format_args(pairCount, sum)) << std::endl;

    if (validation)
    {
        std::ifstream reference(args[2], std::ifstream::in | std::ifstream::binary);
        if (!reference.good())
        {
            std::cout << "Error opening f64 file";
            return 0;
        }
        f64 referenceSum;
        reference.seekg(sizeof(f64) * pairCount);
        reference.read(reinterpret_cast<char*>(&referenceSum), sizeof(referenceSum));
        reference.close();

        std::cout << std::vformat("Validation:\nReference sum: {:.16f}\nDifference: {:.16f}",
            std::make_format_args(referenceSum, referenceSum - sum));
    }
}
