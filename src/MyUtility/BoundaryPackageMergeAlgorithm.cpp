//-------------------------------------------------------------
//! @brief	境界パッケージマージアルゴリズム
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
	// @struct シンボル単体情報
	struct SingleSimbol
	{
		unsigned		   alphabet = 0;		//! シンボル識別子
		unsigned		   weight   = 0;		//!	重み (出現回数)

		SingleSimbol()
		{}

		SingleSimbol(unsigned	alp, unsigned wei)
			: alphabet(alp)
			, weight(wei)
		{}
	};

	// @struct ノード情報(Boundary)
	struct BoundaryPMNode
	{
	public:

		unsigned long long	weight				= 0;			//!	重み (出現回数)
		size_t				singleSimbleCount	= 0;			//! このノードの重み以下の重みを持つシンボル単体の数（このノードを含める）
		BoundaryPMNode*		pNextChainNode		= nullptr;		//! ツリー右側のノードへのポインタ
		bool				ref					= false;		//! 使用中(poolから貸し出されている場合)

		BoundaryPMNode()
		{}
		BoundaryPMNode(unsigned wei, BoundaryPMNode* pChain, size_t numMinRank)
			: weight(wei)
			, singleSimbleCount(numMinRank)
			, pNextChainNode(pChain)
		{}
		BoundaryPMNode(BoundaryPMNode* left, BoundaryPMNode* right, size_t numMinRank)
			: weight(left->weight + right->weight)
			, singleSimbleCount(numMinRank)
			, pNextChainNode(right)
		{}

		void operator=(const BoundaryPMNode& base)
		{
			// ref を除く
			weight			  = base.weight;
			singleSimbleCount = base.singleSimbleCount;
			pNextChainNode    = base.pNextChainNode;
		}
	};

	// @class 超簡易ノードプール
	class BoundaryPMNodePool
	{
	public:

		// @brief 貸出
		//---------------------------------------------------------
		BoundaryPMNode* Borrow()
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
		BoundaryPMNode* BorrowOrException()
		{
			BoundaryPMNode* erem = Borrow();
			if (erem == nullptr)
				throw std::runtime_error("プールに空きがないっぽい");

			return erem;
		}

		// @brief 返却
		//---------------------------------------------------------
		void Return(BoundaryPMNode* p)
		{
			p->ref = false;
		}

		// @brief すべての参照を開放
		//---------------------------------------------------------
		void AllReleaseRef()
		{
			for (size_t i = 0; i < m_pool.size(); ++i)
				m_pool[i].ref = false;
		}

		BoundaryPMNodePool(size_t size)
			:m_pool(size)
		{}

	private:
		std::vector<BoundaryPMNode> m_pool;
		unsigned					m_nextIdx = 0;
	};
	// using
	using SingleSymbolList    = std::vector<SingleSimbol>;

	// @struct 先読みヘッダ
	union LookAheadTree
	{
		union
		{
			struct Pair
			{
				BoundaryPMNode*	pFirst;
				BoundaryPMNode*	pSecond;
			};

			Pair				pair;
			BoundaryPMNode*		pElements[2];
		};

		// @brief  先読みヘッダの合計の重みを返す
		//-------------------------------------------------------------
		inline static unsigned long long GetWeight(const LookAheadTree& lookahead)
		{
			if (lookahead.pair.pFirst == nullptr || lookahead.pair.pSecond == nullptr)
				throw std::runtime_error("nullが来るのはあり得ない");

			return (lookahead.pair.pFirst->weight + lookahead.pair.pSecond->weight);
		}
	};

	// @brief  該当の先読みツリーがパッケージに利用されている
	//-------------------------------------------------------------
	inline bool IsUsedByPackage(const LookAheadTree& rLookaheadTree, const BoundaryPMNode& rPackageNode)
	{
		return (rLookaheadTree.pair.pSecond == rPackageNode.pNextChainNode);
	}

	// @brief 実際に使われているシンボルを抽出
	//-------------------------------------------------------------
	void ExtractSymbolList(const unsigned* symbolWeights, size_t arraySize, SingleSymbolList& /*out*/list)
	{
		// 重みがゼロであるシンボルは利用されていないとみなし、
		// 重みのあるシンボルだけを抽出してリスト化する
		for (unsigned i = 0; i < arraySize; ++i)
		{
			if (symbolWeights[i])
				list.push_back(SingleSimbol(i, symbolWeights[i]));
		}
		// 重みの昇順にソート
		std::sort(list.begin(), list.end(),
			[](const SingleSimbol& left, const SingleSimbol& right)
		{
			if (left.weight != right.weight)
				return left.weight < right.weight;

			return left.alphabet < right.alphabet;
		});
	}

	// @brief 長さテーブル構築
	// @note  rSymbolList は 事前に重みの昇順にソートされているものとする
	// @note  bitlengths  は 事前に resize() 等で必要な領域を割り当てておくこと
	//-------------------------------------------------------------
	void ExtractBitLengths(size_t singleSymbolCount, const SingleSymbolList& rSymbolList, std::vector<unsigned>& /*out*/bitlengths)
	{
		for (size_t i = 0; i < singleSymbolCount; ++i)
		{
			unsigned alphabetIdx = rSymbolList.at(i).alphabet;
			bitlengths[alphabetIdx] += 1;
		}
	}
	//-------------------------------------------------------------
	void ExtractBitLengths(const BoundaryPMNode* pNode, const SingleSymbolList& rSymbolList, std::vector<unsigned>& /*out*/bitlengths)
	{
		if (pNode == nullptr)
			throw std::runtime_error("nullが来るのはあり得ない");

		if (pNode->pNextChainNode)
		{
			ExtractBitLengths(pNode->pNextChainNode, rSymbolList, /*out*/bitlengths);
		}
		// note: 
		// このノードの置かれているステージ上で、
		// このノードよりも重みの小さな(このシンボルよりも先に現れた)
		// シンボル単体の符号長をインクリメントする
		ExtractBitLengths(pNode->singleSimbleCount, rSymbolList, /*out*/ bitlengths);
	}
	//-------------------------------------------------------------
	std::vector<unsigned> BuildBitLengthsArray(size_t singleSymbolCount, const SingleSymbolList& rSymbolList, size_t arraySize)
	{
		std::vector<unsigned> bitLengthsList(arraySize);
		ExtractBitLengths(singleSymbolCount, rSymbolList, /*out*/bitLengthsList);
		return bitLengthsList;
	}
	//-------------------------------------------------------------
	std::vector<unsigned> BuildBitLengthsArray(const BoundaryPMNode* pNode, const SingleSymbolList& rSymbolList, size_t arraySize)
	{
		std::vector<unsigned> bitLengthsList(arraySize);
		ExtractBitLengths(pNode, rSymbolList, /*out*/bitLengthsList);
		return bitLengthsList;
	}

	// @brief シンボル単体のノードを作成
	//-------------------------------------------------------------
	BoundaryPMNode* CreateSymbolNode(const BoundaryPMNode& rSymbolNode, BoundaryPMNodePool& /*ref*/rPool)
	{
		BoundaryPMNode* pNode = rPool.BorrowOrException();
		*pNode = rSymbolNode;

		return pNode;
	}

	// @brief ステージ数だけの先読みツリーリストを作成
	//-------------------------------------------------------------
	std::vector<LookAheadTree> CreateInitialLookAheadPairs(const SingleSimbol& firstSymbol, const SingleSimbol& secondSymbol, size_t numStage, BoundaryPMNodePool& /*ref*/rPool)
	{
		std::vector<LookAheadTree> result(numStage);

		// すべてのステージの先読みツリーは
		// シンボルリスト中の一番目、二番目に小さな重みをもつシンボルで初期化される
		for (size_t i = 0; i < numStage; ++i)
		{
			result[i].pair.pFirst  = CreateSymbolNode(BoundaryPMNode(firstSymbol.weight, nullptr, 1), rPool);
			result[i].pair.pSecond = CreateSymbolNode(BoundaryPMNode(secondSymbol.weight,nullptr, 2), rPool);
		}
		return result;
	}
	// @brief 使用可能なノードを見つけて返す
	//-------------------------------------------------------------
	BoundaryPMNode* FindFreeNode(BoundaryPMNodePool& rPool, std::vector<LookAheadTree>& rCurrentUsingTreeList)
	{
		auto nextElem = rPool.Borrow();
		if (nextElem != nullptr)
			return nextElem;

		// 空きがない -> ガーベジコレクション
		rPool.AllReleaseRef();

		for(LookAheadTree& lookaheadTree : rCurrentUsingTreeList)
		{
			for (auto node = lookaheadTree.pair.pFirst; node; node = node->pNextChainNode)
				node->ref = true;

			for (auto node = lookaheadTree.pair.pSecond; node; node = node->pNextChainNode)
				node->ref = true;
		}
		return rPool.BorrowOrException();
	}

	// @brief 次のノード要素を選択して返す
	//-------------------------------------------------------------
	BoundaryPMNode ChooseNextNode(const SingleSymbolList& singleSymbolList, const LookAheadTree& lookaheadTree, const BoundaryPMNode& beforeNode)
	{
		size_t nextSymbolIndex = beforeNode.singleSimbleCount;

		// note: SymbolListを読み切っているため、残りはすべてパッケージ
		if (nextSymbolIndex >= singleSymbolList.size())
			return BoundaryPMNode(lookaheadTree.pair.pFirst, lookaheadTree.pair.pSecond, nextSymbolIndex);

		// note:
		// 重みの小さなノードが先に返る。
		// 重みが等しい場合はパッケージが優先
		unsigned long long nextSymbolWeight  = singleSymbolList[nextSymbolIndex].weight;
		unsigned long long nextPackageWeight = LookAheadTree::GetWeight(lookaheadTree);

		// note: シンボル単体が選ばれた場合は、直前のノードが持つチェインノードを引き継ぐ
		if (nextSymbolWeight < nextPackageWeight)
			return BoundaryPMNode(singleSymbolList[nextSymbolIndex].weight, beforeNode.pNextChainNode, nextSymbolIndex +1);
		else
			return BoundaryPMNode(lookaheadTree.pair.pFirst, lookaheadTree.pair.pSecond, nextSymbolIndex);

	}
	// @brief 再帰的に先読みツリーを再構築する
	//-------------------------------------------------------------
	void IncrementLookAheadTreeRecursive(std::vector<LookAheadTree>& rLookAheadTreeList, size_t stageIdx, const SingleSymbolList& symbolList, BoundaryPMNodePool& rPool)
	{
		auto *pBeforeNode = rLookAheadTreeList[stageIdx].pair.pSecond;

		// 上にステージがないためシンボル単体を加えるだけ。
		if (stageIdx == 0)
		{
			for (size_t i = 0; i < 2; ++i)
			{
				size_t nextSymbolIndex = pBeforeNode->singleSimbleCount;
				if (nextSymbolIndex >= symbolList.size())
					return;

				// note: チェインで参照されていなければ 次のFindFreeNode()時に回収される
				rLookAheadTreeList[0].pElements[i] = nullptr;

				rLookAheadTreeList[0].pElements[i]  = FindFreeNode(rPool, rLookAheadTreeList);
				*rLookAheadTreeList[0].pElements[i] = BoundaryPMNode(symbolList[nextSymbolIndex].weight, pBeforeNode->pNextChainNode, nextSymbolIndex + 1);

				pBeforeNode = rLookAheadTreeList[0].pElements[i];
			}
			return;
		}
		// 上にステージがある場合は シンボル単体 または パッケージで再構築
		for (size_t i = 0; i < 2; ++i)
		{
			rLookAheadTreeList[stageIdx].pElements[i] = nullptr;

			auto   *pNextNode   = FindFreeNode(rPool, rLookAheadTreeList);
			size_t prevStageIdx = stageIdx - 1;

			*pNextNode = ChooseNextNode(/*single symbol*/ symbolList,
										/*or package*/ rLookAheadTreeList[prevStageIdx],
										/*with before node*/ *pBeforeNode);

			rLookAheadTreeList[stageIdx].pElements[i] = pNextNode;

			if (IsUsedByPackage(rLookAheadTreeList[prevStageIdx], *pNextNode))
				IncrementLookAheadTreeRecursive(rLookAheadTreeList, prevStageIdx, symbolList, rPool);

			pBeforeNode = pNextNode;
		}
	}
}

//-------------------------------------------------------------
// function
//-------------------------------------------------------------

// @brief 境界パッケージマージアルゴリズム
//-------------------------------------------------------------	
std::vector<unsigned> PackageMerge::BoundaryPM(const unsigned* symbolWeights, size_t arraySize, size_t codeLengthLimit)
{
	SingleSymbolList symbolList;
	ExtractSymbolList(symbolWeights, arraySize, /*out*/symbolList);

	if (IsImpossibleCoding(symbolList.size(), codeLengthLimit))
		return std::vector<unsigned>();

	if (symbolList.size() <= 1)
		return BuildBitLengthsArray(symbolList.size(), symbolList, arraySize);

	// 必要なプールの容量 = L(L+1) 
	// 各ステージは先読みツリー(look ahead tree)を保有する。
	// ここで、一番上のステージにはシンボル単体のノードしかないため、参照するノード数は 自身のノードのみの「1」
	// それより下のステージでは、チェインで上のステージのノードを参照するため、
	// 上にあるステージの数だけ、最大で「2,3,4」だけのノード参照する
	// ステージの数を L としたとき、ここまでのルールに従うならば同時に参照するノード数の最大は
	// L + L-1 + ... + 1 
	//	=(L+1) * L/2
	//
	// 先読みツリーは各ステージにつき 2つのノードを保有するため、
	// 
	// (L+1) * L/2*2 
	//	= L(L+1) がステージ数 L に対して必要とされるプールの容量になる
	//
	// 今回は処理の都合で最下段のステージを作らないため、必要になるプールの容量は
	//  = L(L-1) になる
	BoundaryPMNodePool pool(codeLengthLimit * (codeLengthLimit-1) );

	// 処理の都合で、最下段のステージは作らない (codeLengthLimit - 1)
	auto lookaheadStageList  = CreateInitialLookAheadPairs(symbolList[0], symbolList[1], codeLengthLimit-1, pool);

	// 現状リスト最下段の一番右側にあるアクティブなチェインノード。以降のループ処理で順々にシフトする
	BoundaryPMNode rightistChainNode( symbolList[1].weight, nullptr, 2);

	// 最終的にでそろうノードの数は、ステージ数(制限符号長)にかかわらず、シンボル数を n としたとき 2n-2 の数だけとなる
	// 直前の操作ですでに2つのノードを処理済みなので、i=2から始める
	size_t numLastStageNode = (2 * symbolList.size()) - 2;
	for (size_t i = 2; i < numLastStageNode; ++i)
	{
		rightistChainNode = ChooseNextNode(/*single symbol*/symbolList,
										   /*or package*/*lookaheadStageList.rbegin(), 
										   /*with before node*/rightistChainNode);

		if (/*next continue?*/(i + 1) < numLastStageNode)
		{
			if(IsUsedByPackage(*lookaheadStageList.rbegin(),rightistChainNode))
				IncrementLookAheadTreeRecursive(lookaheadStageList, lookaheadStageList.size() - 1, symbolList, pool);
		}
	}
	return BuildBitLengthsArray(&rightistChainNode, symbolList, arraySize);
}
