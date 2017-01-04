//-------------------------------------------------------------
//! @brief	ふつうのパッケージマージアルゴリズム
//! @author	ｹｰﾄｩｽ=ｶｴﾚｽﾃｨｽ
//-------------------------------------------------------------

//-------------------------------------------------------------
// include
//-------------------------------------------------------------
#include "PackageMergeAlgorithm.h"
#include <algorithm>	// std::sort

//-------------------------------------------------------------
// using
//-------------------------------------------------------------
using namespace MyUtility;

//-------------------------------------------------------------
// inner
//-------------------------------------------------------------
namespace
{
	// @struct ノード情報
	struct SymbolNode
	{
		unsigned		   alphabet = 0;		//! シンボル識別子
		unsigned long long weight   = 0;		//!	重み (出現回数)
		
		const SymbolNode*  pLeft   = nullptr;	//! ペアの左側 (これがパッケージノードであれば)
		const SymbolNode*  pRight  = nullptr;	//! ペアの右側

		SymbolNode()
		{}

		SymbolNode(unsigned	alp, unsigned wei)
			: alphabet(alp)
			, weight(wei)
		{}
		SymbolNode(const SymbolNode* left, const SymbolNode* right)
			: pLeft(left)
			, pRight(right)
			, weight(left->weight + right->weight)
		{}
	};

	// using
	using SymbolNodeList = std::vector<SymbolNode>;


	// @brief 実際に使われているシンボルを抽出
	//-------------------------------------------------------------
	void ExtractSymbolList(const unsigned* symbolWeights, size_t arraySize, SymbolNodeList& /*out*/list)
	{
		// 重みがゼロであるシンボルは利用されていないとみなし、
		// 重みのあるシンボルだけを抽出してリスト化する
		for (size_t i = 0; i < arraySize; ++i)
		{
			if (symbolWeights[i])
			{
				unsigned symbol = static_cast<unsigned>(i);
				list.push_back(SymbolNode(symbol, symbolWeights[i]));
			}
		}
	}

	// @brief  そのノードがパッケージか
	//-------------------------------------------------------------
	inline bool IsPackageNode(const SymbolNode& node)
	{
		return (node.pLeft != nullptr && node.pRight != nullptr);
	}

	// @brief ソート用比較関数
	//-------------------------------------------------------------
	bool LessCompare(const SymbolNode& left, const SymbolNode& right)
	{
		if (left.weight != right.weight)
			return left.weight < right.weight;

		if (left.alphabet != right.alphabet)
			return left.alphabet < right.alphabet;

		// note: 遅延 / 境界パッケージマージと結果を合わせる目的。なくても可
		if (IsPackageNode(left))
		{
			// ポインタ比較。上のステージでより左側にあるノードを参照しているほうが左側
			if (IsPackageNode(right))
				return left.pLeft < right.pLeft;

			// シンボル単体のノードよりパッケージのほうが常に左側
			else
				return true;
		}
		return false;
	}

	// @brief ノードステージ整理
	//-------------------------------------------------------------
	void ResolveNodeStage(SymbolNodeList& /*inout*/list)
	{
		// 値の昇順のアルファベットの昇順にソート
		std::sort(list.begin(), list.end(), LessCompare);

		// 2組のペアから漏れる要素がある場合、一番大きなノード１つを除外する
		if (list.size() & 0x1)
			list.pop_back();
	}

	// @brief 長さテーブル構築
	// @note  bitlengths は 事前に resize() 等で必要な領域を割り当てておくこと
	//-------------------------------------------------------------
	void ExtractBitLengths(const SymbolNode* node, std::vector<unsigned>& /*out*/bitlengths)
	{
		if (node == nullptr)
			throw std::runtime_error("nullが来るのはあり得ない");

		if (IsPackageNode(*node))
		{
			ExtractBitLengths(node->pLeft,  bitlengths);
			ExtractBitLengths(node->pRight, bitlengths);
			return;
		}
		// 対象のアルファベットの符号長 +1
		bitlengths[node->alphabet]++;
	}
	//-------------------------------------------------------------
	void ExtractBitLengths(const SymbolNodeList& nodelist, std::vector<unsigned>& /*out*/bitlengths)
	{
		for (const SymbolNode& node : nodelist)
		{
			ExtractBitLengths(&node, /*out*/bitlengths);
		}
	}
	//-------------------------------------------------------------
	std::vector<unsigned> BuildBitLengthsArray(const SymbolNodeList& nodelist, size_t arraySize)
	{
		std::vector<unsigned> bitLengthsList(arraySize);
		ExtractBitLengths(nodelist, /*out*/bitLengthsList);

		return bitLengthsList;
	}
}

//-------------------------------------------------------------
// function
//-------------------------------------------------------------

// @brief 純粋なパッケージマージアルゴリズム
//-------------------------------------------------------------	
std::vector<unsigned> PackageMerge::NaturalPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit)
{
	std::vector<SymbolNodeList> nodeStages(codeLengthLimit);
	ExtractSymbolList(symbolWeights, arraySize, /*out*/nodeStages[0]);

	// キャパオーバー
	if (IsImpossibleCoding(nodeStages[0].size(), codeLengthLimit))
		return std::vector<unsigned>(); // 空の配列を返す

	// 有効なシンボルが2つ以上存在しない
	if (nodeStages[0].size() <= 1)
		return BuildBitLengthsArray(nodeStages[0], arraySize);

	// 各ステージを初期化
	for (size_t i = 1; i < codeLengthLimit; ++i)
	{
		nodeStages[i].assign(nodeStages[0].begin(), nodeStages[0].end());
	}
	ResolveNodeStage(/*ref*/nodeStages[0]);

	// 上から下に向かって順番にマージする
	for (unsigned stage_i = 1; stage_i < nodeStages.size(); ++stage_i)
	{
		const SymbolNodeList& prevStage = nodeStages[stage_i - 1];
		SymbolNodeList&       nextStage = nodeStages[stage_i];

		// 2つのノードを子とするパッケージを作成して次のステージに追加
		// ペアから漏れる要素に対しては処理が通らないことに注意 (elem_i = 1 3 5...)
		for (unsigned elem_i = 1; elem_i < prevStage.size(); elem_i += 2)
		{
			const SymbolNode* left  = &prevStage[elem_i - 1];
			const SymbolNode* right = &prevStage[elem_i];

			nextStage.push_back(SymbolNode(left, right));
		}
		ResolveNodeStage(/*ref*/nextStage);
	}

	// 結果を生成する
	return BuildBitLengthsArray(nodeStages[codeLengthLimit - 1], arraySize);
}

// @brief  符号化が不可能か
// @return 不可能なら true
//-------------------------------------------------------------	
bool PackageMerge::IsImpossibleCoding(size_t numSymbol, size_t codeLengthLimit)
{
	// シンボルの数を "n"
	// 要求される最大符号長を "L" としたとき
	// シンボルの符号化を正しく行うには
	// L ≥ [log2 n] の式を満たす必要がある
	//
	// この式を満たさないほどにシンボルの数が増えると
	// 符号を割り当てることができない
	return numSymbol > (1ULL << codeLengthLimit);
}