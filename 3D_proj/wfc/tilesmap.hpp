#pragma once

#include <unordered_map>
#include <vector>
#include <tuple>

#include "array3D.hpp"
#include "genericWFC.hpp"
#include "model.hpp"
#include <string>

/**
* 瓷砖形态
*/
enum class Symmetry {X, T, L, I};

/**
* 返回这个形状能转几个方向
*/
unsigned nb_of_possible_orientations(const Symmetry &symmetry) {
	switch (symmetry){
	case Symmetry::X:
		return 1;
	case Symmetry::I:
		return 2;
	case Symmetry::T:
	case Symmetry::L:
		return 4;
	default:
		return 0;
	}
}

/**
* tile结构
*/

struct Tile{
	std::vector<ObjModel> data;
	Symmetry symmetry;
	double weight;
	int low;
	int high;

	/**
	* 产生旋转id
	*/
	static std::vector<unsigned> 
	generate_rotation_map(const Symmetry &symmetry) noexcept {
		switch (symmetry){
		case Symmetry::X:
			return{ 0 };
		case Symmetry::I:
			return{ 1,0 };
		case Symmetry::T:
		case Symmetry::L:
			return{ 1, 2, 3, 0 };
		default:
			return{ 0 };
		}
	}


	/**
	* 产生对称id
	*/
	static std::vector<unsigned>
	generate_reflection_map(const Symmetry &symmetry) noexcept {
		switch (symmetry){
		case Symmetry::X:
			return { 0 };
		case Symmetry::I:
			return { 0, 1 };
		case Symmetry::T:
			return { 0, 3, 2, 1 };
		case Symmetry::L:
			return { 1, 0, 3, 2 };
		default:
			return { 0 };
		}
	}

	static std::vector<std::vector<unsigned>>
	generate_action_map(const Symmetry &symmetry) noexcept {
		std::vector<unsigned> rotation_map = generate_rotation_map(symmetry);
		std::vector<unsigned> reflection_map = generate_reflection_map(symmetry);
		size_t size = rotation_map.size();
		std::vector<std::vector<unsigned>> action_map(8, std::vector<unsigned>(size));
		for (size_t i = 0; i < size; ++i){
			action_map[0][i] = i;
		}

		for (size_t a = 1; a < 4; ++a){
			for (size_t i = 0; i < size; ++i){
				action_map[a][i] = rotation_map[action_map[a - 1][i]];
			}
		}

		for (size_t i = 0; i < size; ++i){
			action_map[4][i] = reflection_map[action_map[0][i]];
		}

		for (size_t a = 5; a < 8; ++a){
			for (size_t i = 0; i < size; ++i){
				action_map[a][i] = rotation_map[action_map[a - 1][i]];
			}
		}
		return action_map;
	}

	static std::vector<ObjModel> generate_oriented(ObjModel data, Symmetry symmetry) noexcept {
		std::vector<ObjModel> oriented;
		oriented.push_back(data);

		switch (symmetry)
		{
		case Symmetry::I:
			oriented.push_back(data = RotatedModel(data));
			break;
		case Symmetry::T:
		case Symmetry::L:
			oriented.push_back(data = RotatedModel(data));
			oriented.push_back(data = RotatedModel(data));
			oriented.push_back(data = RotatedModel(data));
			break;
		default:
			break;
		}

		return oriented;
	}

	/**
	* 构造函数
	*/
	Tile(std::vector<ObjModel> data, Symmetry symmetry, double weight, int low, int high) noexcept
		: data(data), symmetry(symmetry), weight(weight), low(low), high(high) {}

	/**
	* 创建第一个基础方向 
	*/
	Tile(ObjModel data, Symmetry symmetry, double weight, int low, int high) noexcept
		: data(generate_oriented(data, symmetry)), symmetry(symmetry),
		weight(weight), low(low), high(high) {}
};

struct TilingWFCOptions {
	bool periodic_output;
};

/**
* class tilingWFC
*/
template <typename T> class TilingWFC {
private:
	/**
	* tiles
	*/
	std::vector<Tile> tiles;

	std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile;

	std::vector<std::vector<unsigned>> oriented_tile_ids;

	TilingWFCOptions options;


	/**
	* wfc一般算法
	*/
	genericWFC wfc;

	static std::pair<std::vector<std::pair<unsigned, unsigned>>,
					std::vector<std::vector<unsigned>>>
	generate_oriented_tile_ids(const std::vector<Tile> &tiles) noexcept {
		std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile;
		std::vector<std::vector<unsigned>> oriented_tile_ids;

		unsigned id = 0;
		for (unsigned i = 0; i < tiles.size(); i++){
			oriented_tile_ids.push_back({});
			for (unsigned j = 0; j < tiles[i].data.size(); j++){
				id_to_oriented_tile.push_back({ i,j });
				oriented_tile_ids[i].push_back(id);
				id++;
			}
		}

		return { id_to_oriented_tile, oriented_tile_ids };
	}



	/**
	* 创造传递器
	*/
	static std::vector<std::array<std::vector<unsigned>, 6>> generate_propagator(
		const std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned>>
		&neighbors,
		std::vector<Tile> tiles,
		std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile,
		std::vector<std::vector<unsigned>> oriented_tile_ids) {
		size_t nb_oriented_tiles = id_to_oriented_tile.size();
		std::vector<std::array<std::vector<bool>, 6>> dense_propagator(
			nb_oriented_tiles, { 
			std::vector<bool>(nb_oriented_tiles, false),
			std::vector<bool>(nb_oriented_tiles, false),
			std::vector<bool>(nb_oriented_tiles, false),
			std::vector<bool>(nb_oriented_tiles, false),
			std::vector<bool>(nb_oriented_tiles, false),
			std::vector<bool>(nb_oriented_tiles, false) 
			});


		//dense_propagator[1][0][2] = true;
		//dense_propagator[1][0][3] = true;
		//dense_propagator[1][0][0] = true;
		//dense_propagator[1][0][6] = true;
		//dense_propagator[1][1][3] = true;
		//dense_propagator[1][1][4] = true;
		//dense_propagator[1][1][0] = true;
		//dense_propagator[1][1][5] = true;

		//dense_propagator[1][5][2] = true;
		//dense_propagator[1][5][3] = true;
		//dense_propagator[1][5][0] = true;
		//dense_propagator[1][5][5] = true;
		//dense_propagator[1][4][3] = true;
		//dense_propagator[1][4][4] = true;
		//dense_propagator[1][4][0] = true;
		//dense_propagator[1][4][6] = true;


		//dense_propagator[2][0][1] = true;
		//dense_propagator[2][0][4] = true;
		//dense_propagator[2][0][0] = true;
		//dense_propagator[2][0][5] = true;
		//dense_propagator[2][1][3] = true;
		//dense_propagator[2][1][4] = true;
		//dense_propagator[2][1][0] = true;
		//dense_propagator[2][1][5] = true;


		//dense_propagator[2][5][1] = true;
		//dense_propagator[2][5][4] = true;
		//dense_propagator[2][5][0] = true;
		//dense_propagator[2][5][6] = true;
		//dense_propagator[2][4][3] = true;
		//dense_propagator[2][4][4] = true;
		//dense_propagator[2][4][0] = true;
		//dense_propagator[2][4][6] = true;


		//dense_propagator[3][0][1] = true;
		//dense_propagator[3][0][4] = true;
		//dense_propagator[3][0][0] = true;
		//dense_propagator[3][0][5] = true;
		//dense_propagator[3][1][1] = true;
		//dense_propagator[3][1][2] = true;
		//dense_propagator[3][1][0] = true;
		//dense_propagator[3][1][6] = true;


		//dense_propagator[3][5][1] = true;
		//dense_propagator[3][5][4] = true;
		//dense_propagator[3][5][0] = true;
		//dense_propagator[3][5][6] = true;
		//dense_propagator[3][4][1] = true;
		//dense_propagator[3][4][2] = true;
		//dense_propagator[3][4][0] = true;
		//dense_propagator[3][4][5] = true;


		//dense_propagator[4][0][2] = true;
		//dense_propagator[4][0][3] = true;
		//dense_propagator[4][0][0] = true;
		//dense_propagator[4][0][6] = true;
		//dense_propagator[4][1][1] = true;
		//dense_propagator[4][1][2] = true;
		//dense_propagator[4][1][0] = true;
		//dense_propagator[4][1][6] = true;


		//dense_propagator[4][5][2] = true;
		//dense_propagator[4][5][3] = true;
		//dense_propagator[4][5][0] = true;
		//dense_propagator[4][5][5] = true;
		//dense_propagator[4][4][1] = true;
		//dense_propagator[4][4][2] = true;
		//dense_propagator[4][4][0] = true;
		//dense_propagator[4][4][5] = true;

		//for (int i = 0; i < 6; i++) {
		//	for (size_t j = 0; j < nb_oriented_tiles; j++) {
		//		dense_propagator[0][i][j] = true;
		//	}
		//}


		//dense_propagator[5][0][1] = true;
		//dense_propagator[5][0][4] = true;
		//dense_propagator[5][0][0] = true;
		//dense_propagator[5][0][5] = true;
		//dense_propagator[5][1][3] = true;
		//dense_propagator[5][1][4] = true;
		//dense_propagator[5][1][0] = true;
		//dense_propagator[5][1][5] = true;


		//dense_propagator[5][5][2] = true;
		//dense_propagator[5][5][3] = true;
		//dense_propagator[5][5][0] = true;
		//dense_propagator[5][5][5] = true;
		//dense_propagator[5][4][1] = true;
		//dense_propagator[5][4][2] = true;
		//dense_propagator[5][4][0] = true;
		//dense_propagator[5][4][5] = true;


		//dense_propagator[6][0][2] = true;
		//dense_propagator[6][0][3] = true;
		//dense_propagator[6][0][0] = true;
		//dense_propagator[6][0][6] = true;
		//dense_propagator[6][1][1] = true;
		//dense_propagator[6][1][2] = true;
		//dense_propagator[6][1][0] = true;
		//dense_propagator[6][1][6] = true;


		//dense_propagator[6][5][1] = true;
		//dense_propagator[6][5][4] = true;
		//dense_propagator[6][5][0] = true;
		//dense_propagator[6][5][6] = true;
		//dense_propagator[6][4][3] = true;
		//dense_propagator[6][4][4] = true;
		//dense_propagator[6][4][0] = true;
		//dense_propagator[6][4][6] = true;



		for (auto neighbor : neighbors) {
			unsigned tile1 = std::get<0>(neighbor);
			unsigned orientation1 = std::get<1>(neighbor);
			unsigned tile2 = std::get<2>(neighbor);
			unsigned orientation2 = std::get<3>(neighbor);
			unsigned horizontal = std::get<4>(neighbor);
			std::vector<std::vector<unsigned>> action_map1 =
				Tile::generate_action_map(tiles[tile1].symmetry);
			std::vector<std::vector<unsigned>> action_map2 =
				Tile::generate_action_map(tiles[tile2].symmetry);

			auto add = [&](unsigned action, unsigned direction) {
				unsigned temp_orientation1 = action_map1[action][orientation1];
				unsigned temp_orientation2 = action_map2[action][orientation2];
				unsigned oriented_tile_id1 =
					oriented_tile_ids[tile1][temp_orientation1];
				unsigned oriented_tile_id2 =
					oriented_tile_ids[tile2][temp_orientation2];
				dense_propagator[oriented_tile_id1][direction][oriented_tile_id2] =
					true;
				direction = get_opposite_direction(direction);
				dense_propagator[oriented_tile_id2][direction][oriented_tile_id1] =
					true;
			};
			if (horizontal == 1){
				add(0, 0);
				add(1, 4);
				add(2, 5);
				add(3, 1);
				add(4, 5);
				add(5, 1);
				add(6, 0);
				add(7, 4);
			}else{
				for (auto itr = 0; itr < 8; itr++){
					add(itr, 2);
					add(itr, 3);
				}
			}
		}

		std::vector<std::array<std::vector<unsigned>, 6>> propagator(
			nb_oriented_tiles);
		for (size_t i = 0; i < nb_oriented_tiles; ++i) {
			for (size_t j = 0; j < nb_oriented_tiles; ++j) {
				for (size_t d = 0; d < 6; ++d) {
					if (dense_propagator[i][d][j]) {
						propagator[i][d].push_back(j);
					}
				}
			}
		}
		cout << "test";
		return propagator;
	}

	/**
	* 获得形状的概率
	*/
	static std::vector<double>
		get_tiles_weight(const std::vector<Tile> &tiles) {
		std::vector<double> frequencies;
		for (size_t i = 0; i < tiles.size(); i++){
			for (size_t j = 0; j < tiles[i].data.size(); ++j){
				frequencies.push_back(tiles[i].weight / tiles[i].data.size());
			}
		}
		return frequencies;
	}

	static std::vector<int>
		get_tiles_low(const std::vector<Tile> &tiles) {
		std::vector<int> height_low;
		for (size_t i = 0; i < tiles.size(); i++) {
			for (size_t j = 0; j < tiles[i].data.size(); ++j) {
				height_low.push_back(tiles[i].low);
			}
		}
		return height_low;

	}

	static std::vector<int>
		get_tiles_high(const std::vector<Tile> &tiles) {
		std::vector<int> height_high;
		for (size_t i = 0; i < tiles.size(); i++) {
			for (size_t j = 0; j < tiles[i].data.size(); ++j) {
				height_high.push_back(tiles[i].high);
			}
		}
		return height_high;
	}

	ObjModel id_to_tiling(Array3D<unsigned> ids) {
		ObjModel tiling;
		int cnt = 0;
		for (unsigned i = 0; i < ids.height; i++){
			for (unsigned j = 0; j < ids.width; j++){
				for (unsigned k = 0; k < ids.depth; k++){
					std::pair<unsigned, unsigned> oriented_tile =
						id_to_oriented_tile[ids.get(i, j, k)];
					ObjModel temp = tiles[oriented_tile.first].data[oriented_tile.second];
					for (unsigned m = 0; m < temp.V.size(); m++) {
						tiling.V.push_back({ temp.V[m].X + i, temp.V[m].Y + j, temp.V[m].Z + k });
					}
					for (unsigned m = 0; m < temp.VN.size(); m++) {
						tiling.VN.push_back({ temp.VN[m].NX, temp.VN[m].NY, temp.VN[m].NZ });
					}
					for (unsigned m = 0; m < temp.F.size(); m++) {
						tiling.F.push_back({ 
							{ temp.F[m].V[0] + cnt, temp.F[m].V[1] + cnt, temp.F[m].V[2] + cnt }, 
							{ temp.F[m].T[0] + cnt, temp.F[m].T[1] + cnt, temp.F[m].T[2] + cnt },
							{ temp.F[m].N[0] + cnt, temp.F[m].N[1] + cnt, temp.F[m].N[2] + cnt }
							});
					}
					//WriteModel("c:/users/xugaoyuan/desktop/wfc_3d/myproj/results/asdas.obj", *tiling);
					cnt += temp.V.size();
				}
			}
		}
		return tiling;
	}


	public:
	/**
	* 构造函数
	*/
	TilingWFC(
		const std::vector<Tile> &tiles,
		const std::vector < std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned>> &neighbors,
		const unsigned height, const unsigned width, const unsigned depth,
		const TilingWFCOptions &options, int seed)
		:tiles(tiles),
		id_to_oriented_tile(generate_oriented_tile_ids(tiles).first),
		oriented_tile_ids(generate_oriented_tile_ids(tiles).second),
		options(options),
		wfc(options.periodic_output, seed, get_tiles_weight(tiles),
			generate_propagator(neighbors, tiles, id_to_oriented_tile, oriented_tile_ids),
			height, width, depth, get_tiles_low(tiles), get_tiles_high(tiles)) {}

	void getprocess() {
		for (auto i = 0; i < wfc.tempprocess.size(); i++) {
			auto temp = wfc.tempprocess[i];
			ObjModel j = id_to_tiling(temp);
			WriteModel("C:/Users/xugaoyuan/Desktop/wfc_3d/myproj/results/" + to_string(i) + ".obj", j);
		}
	}

	/**
	* 算法入口
	*/
	std::optional<ObjModel> run() {
		auto a = wfc.run();
		
		if (a == std::nullopt){
			return std::nullopt;
		}
		//getprocess();
		return id_to_tiling(*a);
	}



};



