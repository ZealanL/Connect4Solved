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

Value Eval::EvalValidMoves(BoardMask hbSelf, BoardMask hbOpp, BoardMask selfWinMask, BoardMask oppWinMask, BoardMask& validMovesMask) {
	// Ref: https://github.com/PascalPons/connect4/blob/master/Position.hpp#L188

	BoardMask oppWinNextMask = oppWinMask & validMovesMask;

	{ // Detect win in 2
		if (oppWinNextMask) {
			// If the opponent has a win next turn, we must play there to block

			if (Util::HasMinBitsSet<2>(oppWinNextMask)) {
				// Opponent has multiple winning moves next turn, we can only stop 1 :(
				return { -1, 2 };
			}

			validMovesMask = oppWinNextMask;
		}

		// Everywhere below a winning square for the opponent
		BoardMask belowOppWin = (oppWinMask >> 1);

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

int Eval::RateMove(BoardMask hbSelf, BoardMask hbOpp, BoardMask moveMask) {
	BoardMask hbSelfMoved = hbSelf | moveMask;
	int threatsEval = Util::BitCount64(hbSelfMoved.MakeWinMask());

	bool makesStackSelf = (moveMask & (hbSelf << 1)) != 0;
	bool makesStackOpp = (moveMask & (hbOpp << 1)) != 0;

	bool makesRowSelf = ((moveMask & (hbSelf << 8)) | (moveMask & (hbSelf >> 8))) != 0;
	bool makesRowOpp = ((moveMask & (hbOpp << 8)) | (moveMask & (hbOpp >> 8))) != 0;

	return
		threatsEval * 2048

		+ (int)makesStackSelf * 8
		- (int)makesStackOpp * 4

		- (int)makesRowSelf * 2
		+ (int)makesRowOpp;
		;
		
}