#include <random>
#include <iostream>
#include <fstream>
#include <format>
#include "types.h"
#include "haversine_formula.h"

std::mt19937_64 prng;
constexpr f64 CLUSTER_RANGE = 30.0;
constexpr f64 EARTH_RADIUS = 6372.8;

struct Point2
{
	f64 x;
	f64 y;
};

Point2 clusterCenters[64];

f64 RandomDouble(f64 min, f64 max)
{
	std::uniform_real_distribution<f64> getrandom(min, max);
	return getrandom(prng);
}

int main(int argCount, char** args)
{
	bool properUsage = true;
	bool generateClusters = false;
	u64 seed = 0;
	u64 pairCount = 0;
	u32 clusterCount = 1;

	if (argCount == 4)
	{
		if (strcmp(args[1], "cluster") == 0)
		{
			generateClusters = true;
		}
		else if (strcmp(args[1], "uniform") != 0)
		{
			properUsage = false;
		}
		seed = atoll(args[2]);
		pairCount = atoll(args[3]);
	}
	else
	{
		properUsage = false;
	}

	if (!properUsage || !pairCount)
	{
		std::cout 
			<< "Usage: generator.exe [uniform/cluster] [random seed] [number of coordinate pairs to generate]\n" 
			<< std::endl;
		return 0;
	}

	prng.seed(seed);

	f64* haversineDistances = static_cast<f64*>(malloc((pairCount + 1) * sizeof(f64)));

	if (generateClusters)
	{
		u64 counter = pairCount >> 1;
		while (counter)
		{
			clusterCount++;
			counter >>= 1;
		}

		for (u32 i = 0; i < clusterCount; i++)
		{
			clusterCenters[i].x = RandomDouble(-180.0 + CLUSTER_RANGE, 180.0 - CLUSTER_RANGE);
			clusterCenters[i].y = RandomDouble(-90.0 + CLUSTER_RANGE, 90.0 - CLUSTER_RANGE);
		}
	}

	f64 expectedSum = 0.0;
	std::ofstream json("pairs.json", std::ofstream::out);
	if (!json.good())
	{
		std::cout << "Error opening json file" << std::endl;
		return 0;
	}
	
	json << "{\"pairs\":[\n";

	for (u64 i = 0; i < pairCount; i++)
	{
		f64 x0, y0, x1, y1;

		if (generateClusters)
		{
			u64 clusterIndex = i * clusterCount / pairCount;
			Point2* clusterCenter = &clusterCenters[clusterIndex];

			x0 = RandomDouble(clusterCenter->x - CLUSTER_RANGE, clusterCenter->x + CLUSTER_RANGE);
			x1 = RandomDouble(clusterCenter->x - CLUSTER_RANGE, clusterCenter->x + CLUSTER_RANGE);
			y0 = RandomDouble(clusterCenter->y - CLUSTER_RANGE, clusterCenter->y + CLUSTER_RANGE);
			y1 = RandomDouble(clusterCenter->y - CLUSTER_RANGE, clusterCenter->y + CLUSTER_RANGE);
		}
		else
		{
			x0 = RandomDouble(-180.0, 180.0);
			x1 = RandomDouble(-180.0, 180.0);
			y0 = RandomDouble(-90.0, 90.0);
			y1 = RandomDouble(-90.0, 90.0);
		}
		json << std::vformat("\t{{\"x0\":{:.16f}, \"y0\":{:.16f}, \"x1\":{:.16f}, \"y1\":{:.16f}}}{}\n",
			std::make_format_args(x0, y0, x1, y1, i == (pairCount - 1) ? "" : ","));
		
		f64 haversineDistance = ReferenceHaversine(x0, y0, x1, y1, EARTH_RADIUS);
		expectedSum += haversineDistance;
		haversineDistances[i] = haversineDistance;
	}
	
	json << "]}";
	json.close();

	f64 average = expectedSum / pairCount;
	haversineDistances[pairCount] = average;

	std::ofstream binaryDistances("answer.f64", std::ofstream::out | std::ofstream::binary);
	binaryDistances.write(reinterpret_cast<char*>(haversineDistances), sizeof(f64) * (pairCount + 1));
	binaryDistances.close();

#if _DEBUG
	std::ifstream test("answer.f64", std::ifstream::in | std::ifstream::binary);
	f64 value;
	test.seekg(sizeof(f64) * pairCount);
	test.read(reinterpret_cast<char*>(&value), sizeof(value));
	if (value != average)
	{
		std::cout << "Computed sum/stored sum mismatch" << std::endl;
	}
	test.close();
#endif

	std::cout << std::vformat(
		"Method: {}\nRandom seed: {}\nPair count: {}\nExpected sum: {:.16f}",
		std::make_format_args(generateClusters ? "cluster" : "uniform", seed, pairCount, average))
		<< std::endl;
}