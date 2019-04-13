#pragma once
#ifndef WFC_PROPAGATOR_HPP_
#define WFC_PROPAGATOR_HPP_

#include "wave.hpp"
#include <tuple>
#include <vector>
#include <array>
#include "array4D.hpp"
#include "direction.hpp"

class Propagator{
public:
	using PropagatorState = std::vector<std::array<std::vector<unsigned>, 6 >>;

private:
	/**
	* 图形大小
	*/
	const unsigned pattern_size;

	PropagatorState propagator_state;

	/**
	* wave的尺寸
	*/
	const unsigned wave_width;
	const unsigned wave_height;
	const unsigned wave_depth;

	/**
	* 返回true如果输出是平铺
	*/
	const bool periodic_output;

	std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned>> propagating;

	Array4D<std::array<int, 6>> compatible;

	/**
	* 初始化compatible
	*/
	void init_compatible() noexcept {
		std::array<int, 6> value;
		for (unsigned z = 0; z < wave_depth; z++){
			for (unsigned y = 0; y < wave_height; y++){
				for (unsigned x = 0; x < wave_width; x++){
					for (unsigned pattern = 0; pattern < pattern_size; pattern++){
						for (int direction = 0; direction < 6; direction++){
							value[direction] =
								propagator_state[pattern][get_opposite_direction(direction)].size();
						}
						compatible.get(z, y, x, pattern) = value;
					}
				}
			}
		}
	}

public:
	/**
	* 构造并初始化
	*/
	Propagator(unsigned wave_height, unsigned wave_width, unsigned wave_depth,
		bool periodic_output, PropagatorState propagator_state) noexcept
		: pattern_size(propagator_state.size()),
		propagator_state(propagator_state), wave_width(wave_width),
		wave_height(wave_height), wave_depth(wave_depth),
		periodic_output(periodic_output), compatible(wave_height, wave_width, wave_depth, pattern_size) {
		init_compatible();
	}

	/**
	* 添加元素
	*/
	void add_to_propagator(unsigned z, unsigned y, unsigned x, unsigned pattern) noexcept {
		std::array<int, 6> temp = {};
		compatible.get(z, y, x, pattern) = temp;
		propagating.emplace_back(z, y, x, pattern);
	}

	/**
	* 传递信息
	*/
	void propagate(Wave &wave) noexcept {
	
		while (propagating.size() != 0){
			unsigned z1, y1, x1, pattern;
			std::tie(z1, y1, x1, pattern) = propagating.back();
			propagating.pop_back();

			//在六个方向传递
			for (unsigned direction = 0; direction < 6; direction++){
				int dx = direction_x[direction];
				int dy = direction_y[direction];
				int dz = direction_z[direction];
				int z2, y2, x2;
				if (periodic_output){
					x2 = ((int)x1 + dx + (int)wave.width) % wave.width;
					y2 = ((int)y1 + dy + (int)wave.height) % wave.height;
					z2 = ((int)z1 + dz + (int)wave.depth) % wave.depth;
				}else{
					x2 = x1 + dx;
					y2 = y1 + dy;
					z2 = z1 + dz;
					if (x2 < 0 || x2 >= (int)wave.width){
						continue;
					}
					if (y2 < 0 || y2 >= (int)wave.height){
						continue;
					}
					if (z2 < 0 || z2 >= (int)wave.depth){
						continue;
					}
				}
				unsigned i2 = x2 + y2 * wave_width + z2 * wave_width * wave_depth;
				const std::vector<unsigned> &patterns =
					propagator_state[pattern][direction];

				for (auto it = patterns.begin(), it_end = patterns.end(); it < it_end; ++it){
					std::array<int, 6> &value = compatible.get(z2, y2, x2, *it);
					value[direction]--;

					if (value[direction] == 0){
						add_to_propagator(z2, y2, x2, *it);
						wave.set(i2, *it, false);
					}
				}
			}
		}
	}
};

#endif // !WFC_PROPAGATOR_HPP_
