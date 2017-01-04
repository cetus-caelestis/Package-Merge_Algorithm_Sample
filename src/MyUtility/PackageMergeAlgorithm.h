//-------------------------------------------------------------
//! @brief	パッケージマージアルゴリズムまわり
//! @author	ｹｰﾄｩｽ=ｶｴﾚｽﾃｨｽ
//-------------------------------------------------------------
#pragma once

//-------------------------------------------------------------
// include
//-------------------------------------------------------------
#include <vector>

namespace MyUtility
{
namespace PackageMerge
{
	//! 純粋なパッケージマージアルゴリズム
	std::vector<unsigned> NaturalPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! 遅延パッケージマージアルゴリズム
	std::vector<unsigned>  LazyPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! 境界パッケージマージアルゴリズム
	std::vector<unsigned> BoundaryPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! 符号化が不可能？
	bool IsImpossibleCoding(size_t numSymbol, size_t codeLengthLimit);
}
}// end namespace