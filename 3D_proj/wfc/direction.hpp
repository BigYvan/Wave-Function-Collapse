#pragma once


constexpr int direction_x[6] = {0, 1, 0, 0, -1, 0};
constexpr int direction_y[6] = {0, 0, 1, -1, 0, 0};
constexpr int direction_z[6] = {1, 0, 0, 0, 0, -1};


constexpr unsigned get_opposite_direction(unsigned direction) noexcept {
	return 5 - direction;
}