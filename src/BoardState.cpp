#include "BoardState.h"

std::ostream& operator<<(std::ostream& stream, const BoardState& boardState) {
	stream << "BoardState {" << std::endl;
	stream << std::string(BOARD_SIZE_X * 2 + 1, '_') << std::endl;
	for (int y = BOARD_SIZE_Y - 1; y >= 0; y--) {
		stream << '|';
		for (int x = 0; x < BOARD_SIZE_X; x++) {
			if (boardState.teams[0].Get(x, y)) {
				stream << '@';
			} else if (boardState.teams[1].Get(x, y)) {
				stream << 'O';
			} else {
				stream << ' ';
			}

			if (x < BOARD_SIZE_X - 1)
				stream << ' ';
		}
		stream << '|' << std::endl;
	}
	for (int i = 0; i < BOARD_SIZE_X; i++)
		stream << '=' << i;
	stream << '=' << std::endl;
	stream << "}";
	return stream;
}