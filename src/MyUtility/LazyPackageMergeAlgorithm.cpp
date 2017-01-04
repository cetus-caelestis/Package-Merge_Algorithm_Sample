//-------------------------------------------------------------
//! @brief	遅延パッケージマージアルゴリズム
//! @author	ｹｰﾄｩｽ=ｶｴﾚｽﾃｨｽ
//-------------------------------------------------------------

//-------------------------------------------------------------
// include
//-------------------------------------------------------------
#include "PackageMergeAlgorithm.h"
#include <algorithm>	// std::sort
#include <memory>

//-------------------------------------------------------------
// using
//-------------------------------------------------------------
using namespace MyUtility;

//-------------------------------------------------------------
// inner
//-------------------------------------------------------------
namespace
{
	// @struct ノード情報(Lazy)
	struct LazyPMNode
	{
		unsigned			alphabet = 0;		//! シンボル識別子
		unsigned long long	weight   = 0;		//!	重み (出現回数)
		LazyPMNode*			pLeft    = nullptr;	//! ペアの左側 (これがパッケージノードであれば)
		LazyPMNode*			pRight   = nullptr;	//! ペアの右側
		bool				ref      = false;	//! 使用中(poolから貸し出されている場合)

		LazyPMNode()
		{}

		LazyPMNode(unsigned	alp, unsigned wei)
			: alphabet(alp)
			, weight(wei)
		{}
		LazyPMNode(LazyPMNode* left, LazyPMNode* right)
			: pLeft (left)
			, pRight(right)
			, weight(left->weight + right->weight)
		{}
		void operator=(const LazyPMNode& base)
		{
			// ref を除く
			alphabet = base.alphabet;
			weight   = base.weight;
			pLeft    = base.pLeft;
			pRight   = base.pRight;
		}

	};

	// @class 超簡易ノードプール
	class LazyPMNodePool
	{
	public:

		// @brief 貸出
		//---------------------------------------------------------
		LazyPMNode* Borrow()
		{
			for (size_t i = 0; i < m_pool.size(); ++i, ++m_nextIdx)
			{
				if (m_nextIdx >= m_pool.size())
					m_nextIdx = 0;

				if (!m_pool[m_nextIdx].ref)
				{
					m_pool[m_nextIdx].ref = true;
					return &m_pool[m_nextIdx++];
				}
			}
			return nullptr;
		}

		// @brief 貸出 or 例外
		//---------------------------------------------------------
		LazyPMNode* BorrowOrException()
		{
			LazyPMNode* erem = Borrow();
			if (erem == nullptr)
				throw std::runtime_error("プールに空きがないっぽい");

			return erem;
		}

		// @brief 返却
		//---------------------------------------------------------
		void Return(LazyPMNode* p)
		{
			p->ref = false;
		}

		LazyPMNodePool(size_t size)
			:m_pool(size)
		{}

	private:
		std::vector<LazyPMNode> m_pool;
		unsigned				m_nextIdx = 0;
	};

	// using
	using SymbolNodeList    = std::vector<LazyPMNode>;

	// @struct 先読みヘッダ
	struct LookAheadTree
	{
		union
		{
			struct Pair
			{
				LazyPMNode*	pFirst;
				LazyPMNode*	pSecond;
			};

			Pair			pair;
			LazyPMNode*		pElements[2];
		};
		size_t				nextSymbleIndex = 0;

		// @brief  先読みヘッダの合計の重みを返す
		//-------------------------------------------------------------
		inline static unsigned long long GetWeight(const LookAheadTree& lookahead)
		{
			if (lookahead.pair.pFirst == nullptr || lookahead.pair.pSecond == nullptr)
				throw std::runtime_error("nullが来るのはあり得ない");

			return (lookahead.pair.pFirst->weight + lookahead.pair.pSecond->weight);
		}
	};

	// @brief  そのノードがパッケージか
	//-------------------------------------------------------------
	inline bool IsPackageNode(const LazyPMNode& node)
	{
		return (node.pLeft != nullptr && node.pRight != nullptr);
	}

	// @brief 実際に使われているシンボルを抽出
	//-------------------------------------------------------------
	void ExtractSymbolList(const unsigned* symbolWeights, size_t arraySize, SymbolNodeList& /*out*/list)
	{
		// 重みがゼロであるシンボルは利用されていないとみなし、
		// 重みのあるシンボルだけを抽出してリスト化する
		for (unsigned i = 0; i < arraySize; ++i)
		{
			if (symbolWeights[i])
				list.push_back(LazyPMNode(i, symbolWeights[i]));
		}
		// 重みの昇順にソート
		std::sort(list.begin(), list.end(),
			[](const LazyPMNode& left, const LazyPMNode& right)
		{
			if (left.weight != right.weight)
				return left.weight < right.weight;

			return left.alphabet < right.alphabet;
		});
	}

	// @brief 長さテーブル構築
	// @note  bitlengths は 事前に resize() 等で必要な領域を割り当てておくこと
	//-------------------------------------------------------------
	void ExtractBitLengths(const LazyPMNode* node, std::vector<unsigned>& /*out*/bitlengths)
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
		for (const LazyPMNode& node : nodelist)
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

	// @brief ステージ数だけの先読みツリーリストを作成
	//-------------------------------------------------------------
	std::vector<LookAheadTree> CreateInitialLookAheadPairs(const LazyPMNode& firstSymbol, const LazyPMNode& secondSymbol, size_t codeLengthLimit, LazyPMNodePool& /*ref*/rPool)
	{	
		std::vector<LookAheadTree> result(codeLengthLimit);

		// すべてのステージの先読みツリーは
		// シンボルリスト中の一番目、二番目に小さな重みをもつシンボルで初期化される
		for (size_t i = 0; i < codeLengthLimit; ++i)
		{
			result[i].pair.pFirst  = rPool.BorrowOrException();
			*result[i].pair.pFirst = LazyPMNode(firstSymbol);

			result[i].pair.pSecond  = rPool.BorrowOrException();
			*result[i].pair.pSecond = LazyPMNode(secondSymbol);

			result[i].nextSymbleIndex = 2;
		}
		return result;
	}

	// @brief 次のノード要素を選択して返す
	//-------------------------------------------------------------
	LazyPMNode* ChooseNextNode(const SymbolNodeList& singleSymbolList, size_t index, const LookAheadTree& lookaheadTree, LazyPMNodePool& rPool)
	{
		auto     nextElem = rPool.BorrowOrException();

		// note: SymbolListを読み切っているため、残りはすべてパッケージ
		if (index >= singleSymbolList.size())
		{
			*nextElem = LazyPMNode(lookaheadTree.pair.pFirst, lookaheadTree.pair.pSecond);
			return nextElem;
		}

		// note:
		// 重みの小さなノードが先に返る。
		// 重みが等しい場合はパッケージが優先
		unsigned long long nextSymbolWeight  = singleSymbolList[index].weight;
		unsigned long long nextPackageWeight = LookAheadTree::GetWeight(lookaheadTree);

		if (nextSymbolWeight < nextPackageWeight)
		{
			*nextElem = LazyPMNode(singleSymbolList[index]);
		}
		else
		{
			*nextElem = LazyPMNode(lookaheadTree.pair.pFirst, lookaheadTree.pair.pSecond);
		}
		return nextElem;
	}
	// @brief ノードを再帰的に開放する
	//-------------------------------------------------------------
	void ReleaseRecursive(LazyPMNode* pElem, LazyPMNodePool& rPool)
	{
		if (pElem == nullptr)
			return;

		ReleaseRecursive(pElem->pLeft, rPool);
		ReleaseRecursive(pElem->pRight, rPool);

		rPool.Return(pElem);
	}
	// @brief 再帰的に先読みツリーを再構築する
	//-------------------------------------------------------------
	void IncrementLookAheadTreeRecursive(std::vector<LookAheadTree>& rLookAheadTreeList, size_t currentStageIdx, const SymbolNodeList& symbolList, LazyPMNodePool& rPool)
	{
		// 上にステージがないためシンボル単体を加えるだけ。
		if (currentStageIdx == 0)
		{
			for (size_t i = 0; i < 2; ++i)
			{
				size_t nextSymbolIndex = rLookAheadTreeList[0].nextSymbleIndex;
				if (nextSymbolIndex >= symbolList.size())
					return;

				rLookAheadTreeList[0].pElements[i]  = rPool.BorrowOrException();
				*rLookAheadTreeList[0].pElements[i] = LazyPMNode(symbolList[nextSymbolIndex]);
				rLookAheadTreeList[0].nextSymbleIndex += 1;
			}
			return;
		}
		// 上にステージがある場合は シンボル単体 または パッケージで再構築
		for (size_t i = 0; i < 2; ++i)
		{
			size_t nextSymbleIndex = rLookAheadTreeList[currentStageIdx].nextSymbleIndex;
			size_t prevStageIdx = currentStageIdx - 1;

			auto *pNextNode = ChooseNextNode(/*single symbol*/symbolList, nextSymbleIndex,
											 /*or package*/rLookAheadTreeList[prevStageIdx], rPool);

			rLookAheadTreeList[currentStageIdx].pElements[i] = pNextNode;


			if (IsPackageNode(*pNextNode))
				IncrementLookAheadTreeRecursive(rLookAheadTreeList, prevStageIdx, symbolList, rPool);

			else // if was chosen single symbol
				rLookAheadTreeList[currentStageIdx].nextSymbleIndex += 1;
		}
	}
}

//-------------------------------------------------------------
// function
//-------------------------------------------------------------

// @brief 遅延パッケージマージアルゴリズム
//-------------------------------------------------------------	
std::vector<unsigned>  PackageMerge::LazyPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit)
{
	SymbolNodeList symbolList;
	ExtractSymbolList(symbolWeights, arraySize, /*out*/symbolList);

	if (IsImpossibleCoding(symbolList.size(), codeLengthLimit))
		return std::vector<unsigned>(); 

	if (symbolList.size() <= 1)
		return BuildBitLengthsArray(symbolList, arraySize);

	// note: プールのサイズは シンボルの数×ステージ数分だけ確保すれば足りる
	LazyPMNodePool pool(symbolList.size() * codeLengthLimit);

	// note: 処理の都合で、一番末尾のステージは作らない (codeLengthLimit - 1)
	auto lookaheadStageList = CreateInitialLookAheadPairs(symbolList[0], symbolList[1], codeLengthLimit - 1, pool);

	// 先頭二つは確定
	std::vector<unsigned> bitLengthsList(arraySize);
	ExtractBitLengths(&symbolList[0], /*out*/bitLengthsList);
	ExtractBitLengths(&symbolList[1], /*out*/bitLengthsList);

	// 最終的にでそろうノードの数は、ステージ数(制限符号長)にかかわらず、シンボル数を n としたとき 2n-2 の数だけとなる
	// 直前の操作ですでに2つのノードを処理済みなので、i=2から始める
	size_t nextSymbleIndex  = 2;
	size_t numLastStageNode = (2 * symbolList.size()) - 2;

	for (size_t i = 2; i < numLastStageNode; ++i)
	{
		auto *pNextNode = ChooseNextNode(/*single symbol*/symbolList, nextSymbleIndex,
										 /*or package*/*lookaheadStageList.rbegin(), pool);
		// 符号長を更新
		ExtractBitLengths(pNextNode, /*out*/bitLengthsList);

		if (/*next continue?*/(i + 1) < numLastStageNode)
		{
			// 更新に使ったツリーはもう使わないため解体
			bool wasChosenPackage = IsPackageNode(*pNextNode);
			ReleaseRecursive(pNextNode, pool);

			if (wasChosenPackage)
				IncrementLookAheadTreeRecursive(lookaheadStageList, lookaheadStageList.size() -1, symbolList, pool);

			else // if was chosen single symbol
				nextSymbleIndex += 1;
		}
	}
	return bitLengthsList;
}
