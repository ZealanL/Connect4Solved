#pragma once
#include "BoardState.h"

#define TEST_HASH_COLLISION 0
#define PRINT_HASHES 0

struct TranspositionTable {
	struct __declspec(align(1)) Entry {
		uint64_t hash;
		int eval;
		uint16_t depthRemaining;

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

	constexpr static int SIZE = 1 << 25 /* Power of two for maximum "%" speed */;
	constexpr static int SIZE_MBS = (sizeof(Entry) * SIZE) / 1'000'000;

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
};