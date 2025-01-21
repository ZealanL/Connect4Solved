#include "Eval.h"

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
	g_CenterMask = {};
	g_VeryCenterMask = {};

	for (int x = 0; x < BOARD_SIZE_X; x++) {
		int marginX = MIN(x, BOARD_SIZE_X - x - 1);
		for (int y = 0; y < BOARD_SIZE_Y; y++) {
			if (marginX >= CENTERED_MARGIN_X)
				g_CenterMask.Set(x, y, true);
			if (marginX >= VERY_CENTERED_MARGIN_X)
				g_VeryCenterMask.Set(x, y, true);
		}
	}

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

int EvalPosition(const BoardMask& hfState) {
	return
		CENTERED_WEIGHT * BITCOUNT64(hfState & g_CenterMask) +
		VERY_CENTERED_WEIGHT * BITCOUNT64(hfState & g_VeryCenterMask)
		;
}

int Eval::EvalBoard(const BoardState& state, bool winOnly) {
	
	BoardMask hbSelf = state.teams[!state.turnSwitch];
	auto& winStates = g_WinStatesForPos[state.lastMoveX][state.lastMoveY];

	for (int i = 0; i < MAX_WINS_PER_POS; i++) {
		if (BITCOUNT64(hbSelf & winStates[i]) == CONNECT_WIN_AMOUNT)
			return -WIN_BASE_VALUE;
	}

	if (winOnly) {
		return 0;
	} else {
		return -EvalPosition(hbSelf);
	}
}

int Eval::EvalMove(const BoardState& state, int move) {
	BoardMask hbSelf = state.teams[state.turnSwitch];
	auto& winStates = g_WinStatesForPos[move][state.GetNextY(move)];

	int eval = 0;
	for (int i = 0; i < MAX_WINS_PER_POS; i++) {
		int connectedAmount = BITCOUNT64(hbSelf & winStates[i]);
		eval += connectedAmount;
	}

	return eval;
}

std::string Eval::EvalToString(int eval, int addedDepth) {
	int absEval = abs(eval);

	if (absEval >= WIN_MIN_VALUE) {
		int movesRemaining = WIN_BASE_VALUE - absEval + addedDepth;
		if (movesRemaining == 0) {
			return eval > 0 ? "WON" : "LOST";
		} else {
			return STR((eval > 0 ? "WIN" : "LOSE") << "(" << movesRemaining << ")");
		}
	} else {
		return STR((eval > 0 ? "+" : "") << eval);
	}
}