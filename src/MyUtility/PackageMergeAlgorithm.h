//-------------------------------------------------------------
//! @brief	�p�b�P�[�W�}�[�W�A���S���Y���܂��
//! @author	��ĩ�=��ڽè�
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
	//! �����ȃp�b�P�[�W�}�[�W�A���S���Y��
	std::vector<unsigned> NaturalPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! �x���p�b�P�[�W�}�[�W�A���S���Y��
	std::vector<unsigned>  LazyPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! ���E�p�b�P�[�W�}�[�W�A���S���Y��
	std::vector<unsigned> BoundaryPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit);

	//! ���������s�\�H
	bool IsImpossibleCoding(size_t numSymbol, size_t codeLengthLimit);
}
}// end namespace