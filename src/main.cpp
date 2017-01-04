#include <iostream>
#include <vector>
#include <random>		// random
#include <ctime>		// use to make random seed
#include <algorithm>	// std::equal

#include "MyUtility/PackageMergeAlgorithm.h"

// proto type
std::vector<unsigned> RandomWeightArray(unsigned maxAlphabet);
bool				  CheckAllResultEquivalent();

//! @brief main
int main()
{
	constexpr unsigned MAX_ALPHABET = 286;
	constexpr size_t   LENGTH_LIMIT = 15;

	auto alphabetArray = RandomWeightArray(MAX_ALPHABET);
		
		//RandomWeightArray(MAX_ALPHABET);

	// 純粋なパッケージマージアルゴリズム
	{
		auto codeLength = 
			MyUtility::PackageMerge::NaturalPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		for (unsigned value : codeLength)
			std::cout << value << ", ";
	}
	std::cout << "\n----------------------------------------------\n";

	// 遅延パッケージマージアルゴリズム
	{
		auto codeLength = 
			MyUtility::PackageMerge::LazyPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		for (unsigned value : codeLength)
			std::cout << value << ", ";
	}
	std::cout << "\n----------------------------------------------\n";

	// 境界パッケージマージアルゴリズム
	{
		auto codeLength = 
			MyUtility::PackageMerge::BoundaryPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		for (unsigned value : codeLength)
			std::cout << value << ", ";
	}
	getchar();
	return 0;
}

//! @brief ランダムなアルファベットの重み表を構築する
std::vector<unsigned> RandomWeightArray(unsigned maxAlphabet)
{
	std::mt19937 mt(static_cast<unsigned>(time(nullptr)));
	std::uniform_int_distribution<unsigned> randomFunc(0, 1024);

	std::vector<unsigned> result(maxAlphabet);

	for (size_t i = 0; i < result.size(); ++i)
		result[i] = randomFunc(mt);

	return result;
}

//! @brief テスト用
#if 0
bool CheckAllResultEquivalent()
{
	constexpr unsigned MAX_LOOP = 100;
	for (unsigned i = 0; i < MAX_LOOP; ++i)
	{
		constexpr unsigned MAX_ALPHABET = 286;
		constexpr size_t   LENGTH_LIMIT = 15;

		auto alphabetArray = RandomWeightArray(MAX_ALPHABET);

		// 純粋なパッケージマージアルゴリズム
		auto codeLength_1 =
			MyUtility::PackageMerge::NaturalPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		// 遅延パッケージマージアルゴリズム
		auto codeLength_2 =
			MyUtility::PackageMerge::LazyPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		// 境界パッケージマージアルゴリズム
		auto codeLength_3 =
			MyUtility::PackageMerge::BoundaryPM(alphabetArray.data(), std::size(alphabetArray), LENGTH_LIMIT);

		if (!std::equal(codeLength_1.begin(), codeLength_1.end(), codeLength_2.begin()) ||
			!std::equal(codeLength_1.begin(), codeLength_1.end(), codeLength_3.begin()))
		{
			std::cout << "***error なんか違うっぽい:" << i;
			std::cout << "\n----------------------------------------------\n";
			for (unsigned value : codeLength_1)
				std::cout << value << ", ";
			std::cout << "\n----------------------------------------------\n";
			for (unsigned value : codeLength_2)
				std::cout << value << ", ";
			std::cout << "\n----------------------------------------------\n";
			for (unsigned value : codeLength_3)
				std::cout << value << ", ";
			std::cout << "\n----------------------------------------------\n";
			return false;
		}
	}
	std::cout << "OK: 正常終了しました！" << std::endl;
	std::cout << "----------------------------------------------\n";
	return true;
}
#endif