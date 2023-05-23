#include "haversine_formula.cpp"
#include "types.h"
#include <random>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>

std::mt19937_64 rng;
std::uniform_real_distribution<f64> longitude(-180.0, 180.0);
std::uniform_real_distribution<f64> latitude(-90.0, 90.0);
constexpr f64 EARTH_RADIUS = 6372.8;

void GeneratePair(f64 points[4])
{
	points[0] = longitude(rng);
	points[1] = latitude(rng);
	points[2] = longitude(rng);
	points[3] = latitude(rng);
}

void AddPairToJsonBuffer(std::string& buffer, f64 points[4])
{
	std::stringstream stream;
	
	stream << std::fixed << std::setprecision(16) 
		<< "\t{\"x0\":"
		<< points[0]
		<< ", \"y0\":"
		<< points[1]
		<< ", \"x1\":"
		<< points[2]
		<< ", \"y1\":"
		<< points[3]
		<< "},\n";
	
	buffer += stream.str();
}



int main(int argCount, char** args)
{
	f64 expectedSum = 0.0;
	u64 pairCount = 10;
	std::string outputBuffer;

	outputBuffer += "{\"pairs\":[\n";

	f64 points[4];

	for (u64 i = 0; i < pairCount; i++)
	{
		GeneratePair(points);
		AddPairToJsonBuffer(outputBuffer, points);
		expectedSum += ReferenceHaversine(points[0], points[1], points[2], points[3], EARTH_RADIUS);
	}
	outputBuffer[outputBuffer.size() - 2] = '\n';
	outputBuffer[outputBuffer.size() - 1] = ']';
	outputBuffer.push_back('}');
	
	std::ofstream json("pairs.json", std::ofstream::out);
	json << outputBuffer;
	json.close();

	std::cout << std::fixed << std::setprecision(16) << expectedSum / pairCount;

}