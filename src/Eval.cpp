#include "Eval.h"

#include "Util.h"

constexpr int CENTERED_MARGIN_X = MAX(1, BOARD_SIZE_X / 4);
constexpr int VERY_CENTERED_MARGIN_X = MAX(2, BOARD_SIZE_X / 3);

constexpr int CENTERED_WEIGHT = 1;
constexpr int VERY_CENTERED_WEIGHT = 2;

static BoardMask g_CenterMask, g_VeryCenterMask;

/////////////////////////////////////////

constexpr int CONNECT_START_MARGIN = CONNECT_WIN_AMOUNT - 1;
constexpr int NUM_WINNING_STATES =
(BOARD_SIZE_X - CONNECT_START_MARGIN) * BOARD_SIZE_Y +
BOARD_SIZE_X * (BOARD_SIZE_Y - CONNECT_START_MARGIN) +
(BOARD_SIZE_X - CONNECT_START_MARGIN) * (BOARD_SIZE_Y - CONNECT_START_MARGIN) * 2;

constexpr int MAX_WINS_PER_POS = CONNECT_WIN_AMOUNT * 4;
static BoardMask g_WinStatesForPos[BOARD_SIZE_X][BOARD_SIZE_Y][MAX_WINS_PER_POS];
static BoardMask g_WinStates[NUM_WINNING_STATES];

void Eval::Init() {
	LOG("Initializing eval...");

	//////////////////

	std::vector<BoardMask> winStates;

	auto fnAddState = [&](int startX, int startY, int deltaX, int deltaY) {
		BoardMask state = {};
		for (int i = 0; i < CONNECT_WIN_AMOUNT; i++)
			state.Set(startX + deltaX * i, startY + deltaY * i, true);
		winStates.push_back(state);
	};

	int numHorizotal = 0, numVertical = 0, numDiagonal = 0;
	for (int startX = 0; startX < BOARD_SIZE_X; startX++) {
		for (int startY = 0; startY < BOARD_SIZE_Y; startY++) {

			if (startX >= CONNECT_START_MARGIN) {
				fnAddState(startX, startY, -1, 0);
				numHorizotal++;
			}

			if (startY >= CONNECT_START_MARGIN) {
				fnAddState(startX, startY, 0, -1);
				numVertical++;
			}

			if (startX >= CONNECT_START_MARGIN && startY >= CONNECT_START_MARGIN) {
				fnAddState(startX, startY, -1, -1);
				fnAddState(startX - CONNECT_START_MARGIN, startY, 1, -1);
				numDiagonal += 2;
			}
		}
	}

	LOG(" > Found " << winStates.size() << " winning states (h: " << numHorizotal << ", v: " << numVertical << ", d: " << numDiagonal << ")");

	RASSERT(numHorizotal == (BOARD_SIZE_X - CONNECT_START_MARGIN) * BOARD_SIZE_Y, "Bad horizontal win generation");
	RASSERT(numVertical == BOARD_SIZE_X * (BOARD_SIZE_Y - CONNECT_START_MARGIN), "Bad vertical win generation");
	RASSERT(numDiagonal == (BOARD_SIZE_X - CONNECT_START_MARGIN) * (BOARD_SIZE_Y - CONNECT_START_MARGIN) * 2, "Bad diagonal win generation");

	RASSERT(winStates.size() == NUM_WINNING_STATES, "Bad winning state cout");
	std::copy(winStates.begin(), winStates.end(), g_WinStates);

	for (int x = 0; x < BOARD_SIZE_X; x++) {
		for (int y = 0; y < BOARD_SIZE_Y; y++) {
			for (int i = 0; i < MAX_WINS_PER_POS; i++)
				g_WinStatesForPos[x][y][i] = {};

			int num = 0;
			for (auto& winState : winStates) {

				if (winState.Get(x, y)) {
					g_WinStatesForPos[x][y][num] = winState;
					num++;

					RASSERT(num <= MAX_WINS_PER_POS, "Bad win generation, too much overlap")
				}
			}
		}
	}

	LOG(" > Generated per-pos win states");
}

bool Eval::IsWonAfterMove(const BoardState& board) {
	BoardMask movedTeam = board.teams[!board.turnSwitch];

	for (auto winState : g_WinStates) 
		if (Util::BitCount64(winState & movedTeam) == CONNECT_WIN_AMOUNT)
			return true;
	
	return false;
}

Value Eval::EvalAndCropValidMoves(const BoardState& board, BoardMask& validMovesMask) {
	
	BoardMask hbSelf = board.teams[board.turnSwitch];
	BoardMask hbOpp = board.teams[!board.turnSwitch];
	BoardMask selfWin = board.winMasks[board.turnSwitch];
	BoardMask oppWin = board.winMasks[!board.turnSwitch];

	BoardMask oppWinNextMask = oppWin & validMovesMask;

	// Ref: https://github.com/PascalPons/connect4/blob/master/Position.hpp#L188

	{ // Detect loss in 2
		if (oppWinNextMask) {
			// If the opponent has a win next turn, we must play there to block

			if (Util::HasMinBitsSet<2>(oppWinNextMask)) {
				// Opponent has multiple winning moves next turn, we can only stop 1 :(
				return { -1, 2 };
			}

			validMovesMask = oppWinNextMask;
		}

		// Everywhere below a winning square for the opponent
		BoardMask belowOppWin = (oppWin >> 1);

		// We can't play there
		validMovesMask &= ~belowOppWin;

		if (!validMovesMask) {
			// No valid moves, we are just gonna lose
			return { -1, 2 };
		}
	}

	{ // Detect draw in 2

		BoardMask unplayedSpots = ~(hbSelf | hbOpp) & BoardMask::GetBoardMask();
		if (!Util::HasMinBitsSet<3>(unplayedSpots)) {
			// Only two moves left, and since we haven't found a win in 2 moves, it's a draw
			return { 0, 2 };
		}
	}

	return VALUE_INVALID;
}

float RateBoardTeam(BoardMask hbSelf, BoardMask hbOpp, int teamIdx) {
	float rating = 0;

	BoardMask threatsMask = hbSelf.MakeWinMask() & ~hbOpp;
	int numThreats = Util::BitCount64(threatsMask);
	rating += numThreats * 512;

	// Gaining an odd-row threat is generally advantageous
	if (threatsMask & BoardMask::GetParityRows(true))
		rating += 256;

	// Stacked threats are super powerful
	BoardMask stackedThreatsMask = (threatsMask >> 1) & threatsMask;
	int numStackedThreats = Util::BitCount64(stackedThreatsMask);
	rating += 4096 * numStackedThreats;

	return rating;
}

float RateBoard(const BoardState& board, BoardMask moveMask) {
	return RateBoardTeam(board.teams[board.turnSwitch] | moveMask, board.teams[!board.turnSwitch], board.turnSwitch);
}

float Eval::RateMove(const BoardState& board, BoardMask moveMask) {

	auto hbSelf = board.teams[board.turnSwitch];
	auto hbOpp = board.teams[!board.turnSwitch];

	float nextBoardRating = RateBoard(board, moveMask);

	constexpr auto fnBumpMask = [](BoardMask hb, BoardMask move, int shift) -> BoardMask {
		return move & (((shift > 0) ? (hb << shift) : (hb >> -shift)) & BoardMask::GetBoardMask());
	};

	constexpr auto fnBumpMask2 = [fnBumpMask](BoardMask hb, BoardMask move, int shift) -> BoardMask {
		return (fnBumpMask(hb, move, shift) | fnBumpMask(hb, move, -shift));
	};

	// Measure how off-center the move is
	int moveIdx = Util::BitMaskToIndex(moveMask);
	int moveX = moveIdx / 8;
	int moveY = moveIdx % 8;
	float offCenterAmountX = 2 * abs(moveX - BOARD_SIZE_X / 2.f);

	bool closesColumn = (moveY == (BOARD_SIZE_Y - 1));

	return
		nextBoardRating
		+ closesColumn * 256 // Closing columns is usually good as it restricts the opponent
		+ -offCenterAmountX // Last priority is being centered
		;
}