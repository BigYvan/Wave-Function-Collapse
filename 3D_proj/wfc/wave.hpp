#ifndef WFC_WAVE_HPP_
#define WFC_WAVE_HPP_

#include <iostream>
#include <limits>
#include <math.h>
#include <random>
#include <stdint.h>
#include <vector>
#include "array3D.hpp"

/**
* �ṹ������������������������ֵ
* ��������ÿ�ζ�����´˽ṹ
*/
struct EntropyMemoisation{
	std::vector<double> plogp_sum;	// The sum of p'(pattern) * log(p'(pattern))
	std::vector<double> sum;	// The sum of p'(pattern)
	std::vector<double> log_sum;	//The log of sum
	std::vector<unsigned> nb_patterns;	// The number of pattern present
	std::vector<double> entropy;	// The entropy of the cell
};

/**
* Class Wave
*/
class Wave{
private:
	/**
	* ��״����
	*/
	const std::vector<double> patterns_frequencies;

	/**
	* p * log(p)
	*/
	const std::vector<double> plogp_patterns_frequencies;

	/**
	* min (p * log(p)) / 2
	*/
	const double half_min_plogp;

	/**
	* ����������Ҫ�ı���
	*/
	EntropyMemoisation memoisation;

	/**
	* ���wave������Ԫ�ض�Ϊfalse�������ֵ����true
	*/
	bool is_impossible;

	/**
	* ��ͬ��״����
	*/
	const unsigned nb_patterns;

	Array3D<uint8_t> data;

	/**
	* ����p * log(p)
	*/
	static std::vector<double>
		get_plogp(const std::vector<double> &distribution) noexcept {
		std::vector<double> plogp;
		for (unsigned i = 0; i < distribution.size(); i++){
			plogp.push_back(distribution[i] * log(distribution[i]));
		}
		return plogp;
	}

	/**
	* ����min��v��/ 2
	*/
	static double get_half_min(const std::vector<double> &v) noexcept {
		double half_min = std::numeric_limits<double>::infinity();
		for (unsigned i = 0; i < v.size(); i++){
			half_min = std::min(half_min, v[i] / 2.0);
		}
		return half_min;
	}
public:
	/**
	* wave�ߴ�
	*/
	const unsigned width;
	const unsigned height;
	const unsigned depth;
	const unsigned size;

	/**
	* ��ʼ��
	*/
	Wave(unsigned height, unsigned width, unsigned depth,
		const std::vector<double> &patterns_frequencies) noexcept
		: patterns_frequencies(patterns_frequencies),
		plogp_patterns_frequencies(get_plogp(patterns_frequencies)),
		half_min_plogp(get_half_min(plogp_patterns_frequencies)),
		is_impossible(false), nb_patterns(patterns_frequencies.size()),
		data(width * height * depth, width * height * depth, nb_patterns, 1),
		width(width), height(height), depth(depth), size(width * height * depth) {
		double base_entropy = 0;
		double base_s = 0;
		double half_min_plogp = std::numeric_limits<double>::infinity();
		for (unsigned i = 0; i < nb_patterns; i++){
			half_min_plogp =
				std::min(half_min_plogp, plogp_patterns_frequencies[i] / 2.0);
			base_entropy += plogp_patterns_frequencies[i];
			base_s += patterns_frequencies[i];
		}
		double log_base_s = log(base_s);
		double entropy_base = log_base_s - base_entropy / base_s;
		memoisation.plogp_sum = std::vector<double>(width * height * depth, base_entropy);
		memoisation.sum = std::vector<double>(width * height * depth, base_s);
		memoisation.log_sum = std::vector<double>(width * height * depth, log_base_s);
		memoisation.nb_patterns = std::vector<unsigned>(width * height * depth, nb_patterns);
		memoisation.entropy = std::vector<double>(width * height * depth, entropy_base);
	}

	/**
	* ����true�����״�ܷ���cell��������
	*/
	bool get(unsigned index, unsigned pattern) const noexcept {
		return data.get(index, index, pattern);
	}
    
	/**
	* ����true�����״�ܷŽ�cell��i��j�� k��
	*/
	bool get(unsigned i, unsigned j, unsigned k, unsigned pattern) {
		return get(i * width * depth + j * depth + k, pattern);
	}

	/**
	* ������cell�е���״
	*/
	void set(unsigned index, unsigned pattern, bool value) noexcept {
		bool old_value = data.get(index, index, pattern);
		if (old_value == value)
		{
			return;
		}
		data.get(index, index, pattern) = value;
		memoisation.plogp_sum[index] -= plogp_patterns_frequencies[pattern];
		memoisation.sum[index] -= patterns_frequencies[pattern];
		memoisation.log_sum[index] = log(memoisation.sum[index]);
		memoisation.nb_patterns[index]--;
		memoisation.entropy[index] = 
			memoisation.log_sum[index] - memoisation.plogp_sum[index] / memoisation.sum[index];
		if (memoisation.nb_patterns[index] == 0){
			is_impossible = true;
		}
	}

	/**
	* ����ͼ����cell��i�� j�� z����ֵ
	*/
	void set(unsigned i, unsigned j, unsigned k, unsigned pattern, bool value) noexcept {
		set(i * width + j, pattern, value);
	}

	/**
	* ���ز�Ϊ0����С�ص�����
	* ����м���contradiction��wave�У��򷵻�-2
	* ������е�cell�������壬����-1
	*/
	int get_min_entropy(std::minstd_rand &gen) const noexcept {
		if (is_impossible){
			return -2;
		}
		std::uniform_real_distribution<> dis(0, abs(half_min_plogp));

		double min = std::numeric_limits<double>::infinity();
		int argmin = -1;

		for (unsigned i = 0; i < size; i++){
			double nb_patterns = memoisation.nb_patterns[i];
			if (nb_patterns == 1){
				continue;
			}

			double entropy = memoisation.entropy[i];

			if (entropy <= min){
				double noise = dis(gen);
				if (entropy + noise < min){
					min = entropy + noise;
					argmin = i;
				}
			}
		}
		return argmin;
	}
};

#endif // WFC_WAVE_HPP_


