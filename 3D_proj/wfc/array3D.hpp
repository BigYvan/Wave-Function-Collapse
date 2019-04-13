#ifndef WFC_UTILS_ARRAY3D_HPP_
#define WFC_UTILS_ARRAY3D_HPP_

#include "assert.h"
#include <vector>

/**
* Represent a 3D array.
* The 3D array is stored in a single array, to improve cache usage.
*/
template <typename T> class Array3D {

public:
	/**
	* The dimensions of the 3D array.
	*/
	unsigned height;
	unsigned width;
	unsigned depth;

	/**
	* The array containing the data of the 3D array.
	*/
	std::vector<T> data;

	/**
	* Build a 2D array given its height, width and depth.
	* All the arrays elements are initialized to default value.
	*/
	Array3D(unsigned height, unsigned width, unsigned depth) noexcept
		: height(height), width(width), depth(depth),
		data(width * height * depth) {}

	/**
	* Build a 2D array given its height, width and depth.
	* All the arrays elements are initialized to value
	*/
	Array3D(unsigned height, unsigned width, unsigned depth, T value) noexcept
		: height(height), width(width), depth(depth),
		data(width * height * depth, value) {}

	/**
	* Return a const reference to the element in the i-th line, j-th column, and
	* k-th depth. i must be lower than height, j lower than width, and k lower
	* than depth.
	*/
	const T &get(unsigned i, unsigned j, unsigned k) const noexcept {
		assert(i < height && j < width && k < depth);
		return data[i * width * depth + j * depth + k];
	}

	/**
	* Return a reference to the element in the i-th line, j-th column, and k-th
	* depth. i must be lower than height, j lower than width, and k lower than
	* depth.
	*/
	T &get(unsigned i, unsigned j, unsigned k) noexcept {
		return data[i * width * depth + j * depth + k];
	}

	/**
	* Check if two 3D arrays are equals.
	*/
	bool operator==(const Array3D &a) const noexcept {
		if (height != a.height || width != a.width || depth != a.depth) {
			return false;
		}

		for (unsigned i = 0; i < data.size(); i++) {
			if (a.data[i] != data[i]) {
				return false;
			}
		}
		return true;
	}

	/**
	* add by xgy
	*/
	Array3D<T> &operator=(const Array3D<T> &a) noexcept {
		height = a.height;
		width = a.width;
		depth = a.depth;
		data = a.data;
		return *this;
	}


	Array3D<T> rotated() const noexcept {
		Array3D<T> result = Array3D<T>(depth, width, height);
		for (unsigned z = 0; z < depth; z++){
			for (unsigned y = 0; y < width; y++){
				for (unsigned x = 0; x < height; x++){
					result.get(z, y, x) = get(x, width - 1 - y, z);
				}
			}
		}
		return result;
	}














};

/**
* Hash function.
*/
namespace std {
	template <typename T> class hash<Array3D<T>> {
	public:
		size_t operator()(const Array3D<T> &a) const noexcept {
			std::size_t seed = a.data.size();
			for (const T &i : a.data) {
				seed ^= hash<T>()(i) + (size_t)0x9e3779b9 + (seed << 6) + (seed >> 2);
			}
			return seed;
		}
	};
} // namespace std

#endif // FAST_WFC_UTILS_ARRAY3D_HPP_
