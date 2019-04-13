#ifndef WFC_UTILS_ARRAY4D_HPP_
#define WFC_UTILS_ARRAY4D_HPP_

#include "assert.h"
#include <vector>

/**
* Represent a 4D array.
* The 4D array is stored in a single array, to improve cache usage.
*/
template <typename T> class Array4D {

public:
	/**
	* The dimensions of the 4D array.
	*/
	unsigned height;
	unsigned width;
	unsigned depth;
	unsigned unknown;

	/**
	* The array containing the data of the 4D array.
	*/
	std::vector<T> data;

	/**
	* Build a 4D array given its height, width ,depth and unknown.
	* All the arrays elements are initialized to default value.
	*/
	Array4D(unsigned height, unsigned width, unsigned depth, unsigned unknown) noexcept
		: height(height), width(width), depth(depth), unknown(unknown),
		data(width * height * depth * unknown) {}

	/**
	* Build a 2D array given its height, width , depth and unknown.
	* All the arrays elements are initialized to value
	*/
	Array4D(unsigned height, unsigned width, unsigned depth, unsigned unknown, T value) noexcept
		: height(height), width(width), depth(depth), unknown(unknown),
		data(width * height * depth * unknown, value) {}

	/**
	* Return a const reference to the element in the i-th line, j-th column, 
	* k-th depth and l-th unknown. i must be lower than height, j lower than width, k lower
	* than depth and l lower than unknown.
	*/
	const T &get(unsigned i, unsigned j, unsigned k, unsigned l) const noexcept {
		assert(i < height && j < width && k < depth && l < unknown);
		return data[i * width * depth * unknown + j * depth * unknown + k * unknown + l];
	}

	/**
	* Return a reference to the element in the i-th line, j-th column, k-th
	* depth and l-th unknown. i must be lower than height, j lower than width, k lower than
	* depth and l lower than unknown.
	*/
	T &get(unsigned i, unsigned j, unsigned k, unsigned l) noexcept {
		return data[i * width * depth * unknown + j * depth * unknown + k * unknown + l];
	}

	/**
	* Check if two 4D arrays are equals.
	*/
	bool operator==(const Array4D &a) const noexcept {
		if (height != a.height || width != a.width || depth != a.depth || unknown != a.unknown) {
			return false;
		}

		for (unsigned i = 0; i < data.size(); i++) {
			if (a.data[i] != data[i]) {
				return false;
			}
		}
		return true;
	}
};

#endif // WFC_UTILS_ARRAY4D_HPP_
