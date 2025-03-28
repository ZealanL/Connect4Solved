#include "BoardState.h"

constexpr int MAX_RECONSTRUCTION_NODES = 10'000;

// Determines the moves used to recreate a board
bool ReconstructMovesRecursive(const BoardState& targetBoard, const BoardState& curBoard, std::vector<int>& moves, uint64_t& nodeCount) {
	if (curBoard.GetCombinedMask() == targetBoard.GetCombinedMask())
		return true;
	
	auto curMoves = curBoard.GetValidMoveMask() & targetBoard.teams[curBoard.turnSwitch];
	auto itr = MoveIterator(curMoves);
	nodeCount++;

	if (nodeCount > MAX_RECONSTRUCTION_NODES)
		return false;

	while (auto move = itr.GetNext()) {
		BoardState nextBoard = curBoard;
		nextBoard.FillMove(move);
		int slotNum = Util::BitMaskToIndex(move) / 8 + 1;
		moves.push_back(slotNum);

		if (ReconstructMovesRecursive(targetBoard, nextBoard, moves, nodeCount))
			return true;
		
		moves.pop_back();
	}

	return false;
}

std::vector<int> ReconstructMoves(const BoardState& board) {
	std::vector<int> moves = {};
	uint64_t nodeCount = 0;
	if (ReconstructMovesRecursive(board, BoardState(), moves, nodeCount)) {
		return moves;
	} else {
		return {};
	}
}

std::ostream& operator<<(std::ostream& stream, const BoardState& boardState) {
	const char TEAM_CHARS[] = { '@', 'O' };

	stream << "{" << std::endl;
	stream << "\tTurn: " << TEAM_CHARS[(int)boardState.turnSwitch] << ", movecount: " << (int)boardState.moveCount << std::endl;

	auto possibleMoves = ReconstructMoves(boardState);
	if (!possibleMoves.empty()) {
		stream << "\tPossible moves: ";
		for (int slotNum : possibleMoves)
			stream << slotNum;
		stream << std::endl;
	} else {
		stream << "\tPossible moves: <UNKNOWN>";
		stream << std::endl;
	}

	stream << '\t' << std::string(BOARD_SIZE_X * 2 + 1, '_') << std::endl;

	for (int y = BOARD_SIZE_Y - 1; y >= 0; y--) {
		stream << "\t|";
		for (int x = 0; x < BOARD_SIZE_X; x++) {
			if (boardState.teams[0].Get(x, y)) {
				stream << TEAM_CHARS[0];
			} else if (boardState.teams[1].Get(x, y)) {
				stream << TEAM_CHARS[1];
			} else {
				stream << ' ';
			}

			if (x < BOARD_SIZE_X - 1)
				stream << ' ';
		}
		stream << '|' << std::endl;
	}
	stream << '\t';
	for (int i = 0; i < BOARD_SIZE_X; i++)
		stream << '=' << (i + 1);
	stream << '=' << std::endl;
	stream << "}";
	return stream;
}