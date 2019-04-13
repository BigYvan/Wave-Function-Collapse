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
   * �������
   */
  std::minstd_rand gen;

  /**
   * The wave, indicating which patterns can be put in which cell.
   * wave����ʾ�ĸ�ͼ��Ӧ�ñ������ĸ�����
   */
  Wave wave;

  /**
   * The distribution of the patterns as given in input.
   * �����и��ķֲ�ģʽ
   */
  const std::vector<double> patterns_frequencies;

  /**
   * The number of distinct patterns.
   * ��ͬͼ��������
   */
  const unsigned nb_patterns;

  /**
   * The propagator, used to propagate the information in the wave.
   * �����������ڴ���wave�е���Ϣ
   */
  Propagator propagator;

  /**
   * Transform the wave to a valid output (a 2d array of patterns that aren't in contradiction). 
   * This function should be used only when all cell of the wave are defined.
   * ����ת��Ϊ��Ч�������һ����ì�ܵ�2d���У�
   * �˺���ֻ�е��������и��Ӷ�������
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
   * ���캯������ʼ��
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
   * �����㷨���ɹ��Ļ�������һ�����
   */
  std::optional<Array2D<unsigned>> run() noexcept {

	 // ָ����ʼͼƬλ��
	constrainedSynthesis(5, 5, false);
    while (true) {
      // Define the value of an undefined cell.
	  // ����δ���������ֵ
      ObserveStatus result = observe();

      // Check if the algorithm has terminated.
	  // ����㷨�Ƿ����
      if (result == failure) {
        return std::nullopt;
      } else if (result == success) {
        return wave_to_output();
      }

      // Propagate the information.
	  // ������Ϣ
      propagator.propagate(wave);
    }
  }

  /**
   * Return value of observe.
   * ���ع۲��ֵ
   */
  enum ObserveStatus {
    success,    // WFC has finished and has succeeded.
				// wfc��ɲ�ȡ�óɹ�
    failure,    // WFC has finished and failed.
				// wfc��ɲ�ʧ��
    to_continue // WFC isn't finished.
				// wfcû�����
  };

  /**
   * Define the value of the cell with lowest entropy.
   * �����������ص�����ֵ
   */
  ObserveStatus observe() noexcept {
    // Get the cell with lowest entropy.
	// �õ���������ص�����
    int argmin = wave.get_min_entropy(gen);

    // If there is a contradiction, the algorithm has failed.
	// ����ͻ������failure
    if (argmin == -2) {
      return failure;
    }

    // If the lowest entropy is 0, then the algorithm has succeeded and finished.
	// ����������0����ô�㷨�ɹ������
    if (argmin == -1) {
      wave_to_output();
      return success;
    }

    // Choose an element according to the pattern distribution
	// ���ݷֲ��ṹѡ��һ��Ԫ��
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
	// ����ͼ����������
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
   * ���ݲ�����Ϣ
   */
  void propagate() noexcept { propagator.propagate(wave); }

  /**
   * Remove pattern from cell (i,j).
   * �Ƴ�cell��i��j����ͼ��
   */
  void remove_wave_pattern(unsigned i, unsigned j, unsigned pattern) noexcept {
    if (wave.get(i, j, pattern)) {
      wave.set(i, j, pattern, false);
      propagator.add_to_propagator(i, j, pattern);
    }
  }
};

#endif // FAST_WFC_WFC_HPP_
