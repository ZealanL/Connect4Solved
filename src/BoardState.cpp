#include "BoardState.h"

std::ostream& operator<<(std::ostream& stream, const BoardState& boardState) {
	const char TEAM_CHARS[] = { '@', 'O' };

	stream << "{" << std::endl;
	stream << "\tTurn: " << TEAM_CHARS[(int)boardState.turnSwitch] << ", movecount: " << (int)boardState.moveCount << std::endl;
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