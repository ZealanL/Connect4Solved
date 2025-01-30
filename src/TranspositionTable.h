#pragma once
#include "BoardState.h"
#include "Eval.h"

#define TEST_HASH_COLLISION 0
#define PRINT_HASHES 0

struct TranspositionTable {
	struct __declspec(align(1)) Entry {
		uint64_t hash;
		Value eval;

#if TEST_HASH_COLLISION
		BoardState board;
#endif

		bool IsValid() const {
			return hash != NULL;
		}

		void Reset() {
			hash = NULL;
		}
	};

	constexpr static size_t SIZE = 1 << 25 /* Power of two for maximum "%" speed */;
	constexpr static size_t SIZE_MBS = (sizeof(Entry) * SIZE) / 1'000'000;

	////////////////////////////////////////////////////////////////////////

	Entry entries[SIZE];

	TranspositionTable() {
		Reset();
	}

	static uint64_t HashBoard(const BoardState& board) {
		return Util::FastHash(board.teams[0], false) ^ Util::FastHash(board.teams[1], true);
	}

	void Reset() {
		memset(entries, 0, sizeof(entries));
	}

	size_t LoopIndex(size_t index) {
		return index % SIZE;
	}

	Entry* Get(size_t index) {
		return &entries[LoopIndex(index)];
	}

	Entry* Find(uint64_t hash) {

#if PRINT_HASHES
		LOG((void*)hash << ": " << std::hex << "0x" << LoopIndex(hash));
#endif

		return Get(hash);
	}

	double GetFillFrac() const {
		size_t numFilled = 0;
		for (size_t i = 0; i < SIZE; i++)
			numFilled += entries[i].IsValid();
		return (double)numFilled / (double)SIZE;
	}
};