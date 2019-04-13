#ifndef FAST_WFC_WFC_HPP_
#define FAST_WFC_WFC_HPP_

#include <cmath>
#include <limits>
#include <random>
#include <unordered_map>

#include "utils/array2D.hpp"
#include "propagator.hpp"
#include "wave.hpp"
#include <optional>

/**
 * Class containing the generic WFC algorithm.
 */
class WFC {
private:
  /**
   * The random number generator.
   * 随机种子
   */
  std::minstd_rand gen;

  /**
   * The wave, indicating which patterns can be put in which cell.
   * wave，表示哪个图案应该被填入哪个格中
   */
  Wave wave;

  /**
   * The distribution of the patterns as given in input.
   * 输入中给的分布模式
   */
  const std::vector<double> patterns_frequencies;

  /**
   * The number of distinct patterns.
   * 不同图案的数量
   */
  const unsigned nb_patterns;

  /**
   * The propagator, used to propagate the information in the wave.
   * 传递器，用于传递wave中的信息
   */
  Propagator propagator;

  /**
   * Transform the wave to a valid output (a 2d array of patterns that aren't in contradiction). 
   * This function should be used only when all cell of the wave are defined.
   * 将波转换为有效的输出（一个不矛盾的2d阵列）
   * 此函数只有当波的所有格子都被定义
   */
  Array2D<unsigned> wave_to_output() const noexcept {
    Array2D<unsigned> output_patterns(wave.height, wave.width);
    for (unsigned i = 0; i < wave.size; i++) {
      for (unsigned k = 0; k < nb_patterns; k++) {
        if (wave.get(i, k)) {
          output_patterns.data[i] = k;
        }
      }
    }
    return output_patterns;
  }

public:
  /**
   * Basic constructor initializing the algorithm.
   * 构造函数，初始化
   */
  WFC(bool periodic_output, int seed, std::vector<double> patterns_frequencies,
      Propagator::PropagatorState propagator, unsigned wave_height,
      unsigned wave_width)
  noexcept
    : gen(seed), wave(wave_height, wave_width, patterns_frequencies),
        patterns_frequencies(patterns_frequencies),
        nb_patterns(propagator.size()),
        propagator(wave.height, wave.width, periodic_output, propagator) {}

  /**
   * add Constrained synthesis
   * by xgy 2018.7.23
   */
  void constrainedSynthesis(unsigned index, unsigned pattern, bool value) {
	  for (unsigned k = 0; k < nb_patterns; k++) {
		  if (wave.get(index, k) != (k == pattern)) {
			  propagator.add_to_propagator(index / wave.width, index % wave.width,
				  k);
			  wave.set(index, k, false);
		  }
	  }
  }
  

  /**
   * Run the algorithm, and return a result if it succeeded.
   * 运行算法，成功的话并返回一个结果
   */
  std::optional<Array2D<unsigned>> run() noexcept {

	 // 指定初始图片位置
	constrainedSynthesis(5, 5, false);
    while (true) {
      // Define the value of an undefined cell.
	  // 定义未定义的网格值
      ObserveStatus result = observe();

      // Check if the algorithm has terminated.
	  // 检查算法是否结束
      if (result == failure) {
        return std::nullopt;
      } else if (result == success) {
        return wave_to_output();
      }

      // Propagate the information.
	  // 传递信息
      propagator.propagate(wave);
    }
  }

  /**
   * Return value of observe.
   * 返回观察的值
   */
  enum ObserveStatus {
    success,    // WFC has finished and has succeeded.
				// wfc完成并取得成功
    failure,    // WFC has finished and failed.
				// wfc完成并失败
    to_continue // WFC isn't finished.
				// wfc没有完成
  };

  /**
   * Define the value of the cell with lowest entropy.
   * 定义具有最低熵的网格值
   */
  ObserveStatus observe() noexcept {
    // Get the cell with lowest entropy.
	// 得到具有最低熵的网格
    int argmin = wave.get_min_entropy(gen);

    // If there is a contradiction, the algorithm has failed.
	// 检查冲突，返回failure
    if (argmin == -2) {
      return failure;
    }

    // If the lowest entropy is 0, then the algorithm has succeeded and finished.
	// 如果最低熵是0，那么算法成功并完成
    if (argmin == -1) {
      wave_to_output();
      return success;
    }

    // Choose an element according to the pattern distribution
	// 根据分布结构选择一个元素
    double s = 0;
    for (unsigned k = 0; k < nb_patterns; k++) {
      s += wave.get(argmin, k) ? patterns_frequencies[k] : 0;
    }

    std::uniform_real_distribution<> dis(0, s);
    double random_value = dis(gen);
    unsigned chosen_value = nb_patterns - 1;

    for (unsigned k = 0; k < nb_patterns; k++) {
      random_value -= wave.get(argmin, k) ? patterns_frequencies[k] : 0;
      if (random_value <= 0) {
        chosen_value = k;
        break;
      }
    }

    // And define the cell with the pattern.
	// 根据图案定义网格
    for (unsigned k = 0; k < nb_patterns; k++) {
      if (wave.get(argmin, k) != (k == chosen_value)) {
        propagator.add_to_propagator(argmin / wave.width, argmin % wave.width,
                                     k);
        wave.set(argmin, k, false);
      }
    }

    return to_continue;
  }

  /**
   * Propagate the information of the wave.
   * 传递波的信息
   */
  void propagate() noexcept { propagator.propagate(wave); }

  /**
   * Remove pattern from cell (i,j).
   * 移除cell（i，j）的图案
   */
  void remove_wave_pattern(unsigned i, unsigned j, unsigned pattern) noexcept {
    if (wave.get(i, j, pattern)) {
      wave.set(i, j, pattern, false);
      propagator.add_to_propagator(i, j, pattern);
    }
  }
};

#endif // FAST_WFC_WFC_HPP_
