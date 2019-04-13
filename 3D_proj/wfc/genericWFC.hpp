#pragma once
#ifndef WFC_GENERICWFC_HPP_
#define WFC_GENERICWFC_HPP_

#include <cmath>
#include <limits>
#include <random>
#include <unordered_map>

#include "array3D.hpp"
#include "propagator.hpp"
#include "wave.hpp"
#include <optional>
#include <vector>

/**
* 一般wfc算法类
*/
class genericWFC{
private:
	/**
	* 随机种子
	*/
	std::minstd_rand gen;

	/**
	* wave
	*/
	Wave wave;

	/**
	* 分布模式
	*/
	const std::vector<double> patterns_frequencies;

	/**
	* 高度限制
	*/
	const std::vector<int> highth_limit_low;
	const std::vector<int> highth_limit_high;
	/**
	* 不同形状的数量
	*/
	const unsigned nb_patterns;

	/**
	* 传递器
	*/
	Propagator propagator;

	/**
	* 将wave转为3d阵列
	*/
	Array3D<unsigned> wave_to_output() const noexcept {
		Array3D<unsigned> output_patterns(wave.depth, wave.height, wave.width);
		for (unsigned i = 0; i < wave.size; i++){
			for (unsigned k = 0; k < nb_patterns; k++){
				if (wave.get(i, k)){
					output_patterns.data[i] = k;
				}
			}
		}
		return output_patterns;
	}

public:
	/**
	* 构造函数
	*/
	genericWFC(bool periodic_output, int seed, std::vector<double> patterns_frequencies,
		Propagator::PropagatorState propagator,
		unsigned wave_depth, unsigned wave_height, unsigned wave_width, 
		std::vector<int> highth_limit_low, std::vector<int> highth_limit_high)
	noexcept
		:gen(seed), wave(wave_depth, wave_height, wave_width, patterns_frequencies),
		patterns_frequencies(patterns_frequencies),
		nb_patterns(propagator.size()),
		propagator(wave_depth, wave_height, wave_width, periodic_output, propagator),
		highth_limit_low(highth_limit_low), highth_limit_high(highth_limit_high) {}

	/**
	* 返回观察的值
	*/
	enum ObserveStatus{
		success,
		failure,
		to_continue
	};

	/**
	* 定义具有最低熵的cell
	*/
	ObserveStatus observe() noexcept {
		int argmin = wave.get_min_entropy(gen);

		if (argmin == -2){
			return failure;
		}

		if (argmin == -1){
			wave_to_output();
			return success;
		}

		double s = 0;
		for (unsigned k = 0; k < nb_patterns; k++){
			s += wave.get(argmin, k) ? patterns_frequencies[k] : 0;
		}

		std::uniform_real_distribution<> dis(0, s);
		double random_value = dis(gen);
		unsigned chosen_value = nb_patterns - 1;

		for (unsigned k = 0; k < nb_patterns; k++){
			random_value -= wave.get(argmin, k) ? patterns_frequencies[k] : 0;
			if (random_value <= 0){
				chosen_value = k;
				break;
			}
		}

		unsigned z = argmin / wave.width / wave.height;
		unsigned y = argmin % (wave.width * wave.height) / wave.width;
		unsigned x = argmin % (wave.width * wave.height) % wave.width;
		int start = highth_limit_low[chosen_value];
		int end = highth_limit_high[chosen_value];
		int heigh_temp = wave.width - 1 - y;

		if (heigh_temp >= start && heigh_temp <= end){
			for (unsigned k = 0; k < nb_patterns; k++) {

				if (wave.get(argmin, k) != (k == chosen_value)) {
					propagator.add_to_propagator(argmin / wave.width / wave.height,
						argmin % (wave.width * wave.height) / wave.width,
						argmin % (wave.width * wave.height) % wave.width, k);
					wave.set(argmin, k, false);
				}
			}
		}

		return to_continue;
	}
	std::vector<Array3D<unsigned>> tempprocess;
	/**
	* 运行算法，成功的话返回一个结果
	*/
	std::optional<Array3D<unsigned>> run() noexcept {
		while (true){
			ObserveStatus result = observe();
			if (result == failure){
				return std::nullopt;
			}
			else if (result == success) {
				return wave_to_output();
			}
			tempprocess.push_back( wave_to_output());
			propagator.propagate(wave);
		}
	}

	/**
	* 传递wave的信息
	*/
	void propagate() noexcept { propagator.propagate(wave); }

	/**
	* 移除cell（i,j,k）的图形
	*/
	void remove_wave_pattern(unsigned i, unsigned j, unsigned k, unsigned pattern) noexcept {
		if (wave.get(i, j, k, pattern)){
			wave.set(i, j, k, pattern, false);
			propagator.add_to_propagator(i, j, k, pattern);
		}
	}
};

#endif // !WFC_GENERICWFC_HPP_
