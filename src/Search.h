#pragma once

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

#include "Util.h"

struct SearchInfo {
	BoardMask bestMove = 0;

	uint64_t totalSearched = 0;
	uint64_t totalTableSeaches = 0;
	uint64_t totalTableHits = 0;
	uint64_t totalWins = 0;
	uint64_t totalPruned = 0; // Times we pruned due to beta

	double GetTableHitFrac() const {
		return (totalTableSeaches > 0) ? (double)totalTableHits / (double)totalTableSeaches : 0;
	}

	double GetWinFrac() const {
		return (double)totalWins / (double)totalSearched;
	}
};

struct SearchCache {
	Value min = -1;
	Value max = 1;
	uint8_t depthElapsed = 0;

	SearchCache ProgressDepth() {
		return SearchCache{
			-max,
			-min,
			(uint8_t)(depthElapsed + 1)
		};
	}
};

struct SearchResult {
	BoardMask move;
	Value eval;
};

namespace Search {
	uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0);
	Value AlphaBetaSearch(TranspositionTable* table, const BoardState& board, SearchInfo& outInfo, SearchCache cache = {});

	SearchResult Search(TranspositionTable* table, const BoardState& board, bool log);
}