
#include <unordered_map>
#include <vector>

#include "utils/array2D.hpp"
#include "wfc.hpp"

/**
 * 瓷砖形态（旋转，对称）
 */
enum class Symmetry { X, T, I, L, backslash, P };

/**
 * 返回这个形态的瓷砖能转几个方向
 */
unsigned nb_of_possible_orientations(const Symmetry &symmetry) {
  switch (symmetry) {
  case Symmetry::X:
    return 1;
  case Symmetry::I:
  case Symmetry::backslash:
    return 2;
  case Symmetry::T:
  case Symmetry::L:
    return 4;
  default:
    return 8;
  }
}

/**
 * 瓷砖结构
 */
template <typename T> struct Tile {
  std::vector<Array2D<T>> data; // 位置
  Symmetry symmetry;            // 对称结构
  double weight;				// 

  /**
   * 产生旋转id
   */
  static std::vector<unsigned>
  generate_rotation_map(const Symmetry &symmetry) noexcept {
    switch (symmetry) {
    case Symmetry::X:
      return {0};
    case Symmetry::I:
    case Symmetry::backslash:
      return {1, 0};
    case Symmetry::T:
    case Symmetry::L:
      return {1, 2, 3, 0};
    case Symmetry::P:
    default:
      return {1, 2, 3, 0, 5, 6, 7, 4};
    }
  }

  /**
   * 产生对称id
   */
  static std::vector<unsigned>
  generate_reflection_map(const Symmetry &symmetry) noexcept {
    switch (symmetry) {
    case Symmetry::X:
      return {0};
    case Symmetry::I:
      return {0, 1};
    case Symmetry::backslash:
      return {1, 0};
    case Symmetry::T:
      return {0, 3, 2, 1};
    case Symmetry::L:
      return {1, 0, 3, 2};
    case Symmetry::P:
    default:
      return {4, 7, 6, 5, 0, 3, 2, 1};
    }
  }


  static std::vector<std::vector<unsigned>>
  generate_action_map(const Symmetry &symmetry) noexcept {
    std::vector<unsigned> rotation_map = generate_rotation_map(symmetry);
    std::vector<unsigned> reflection_map = generate_reflection_map(symmetry);
    size_t size = rotation_map.size();
    std::vector<std::vector<unsigned>> action_map(8,
                                                  std::vector<unsigned>(size));
    for (size_t i = 0; i < size; ++i) {
      action_map[0][i] = i;
    }

    for (size_t a = 1; a < 4; ++a) {
      for (size_t i = 0; i < size; ++i) {
        action_map[a][i] = rotation_map[action_map[a - 1][i]];
      }
    }
    for (size_t i = 0; i < size; ++i) {
      action_map[4][i] = reflection_map[action_map[0][i]];
    }
    for (size_t a = 5; a < 8; ++a) {
      for (size_t i = 0; i < size; ++i) {
        action_map[a][i] = rotation_map[action_map[a - 1][i]];
      }
    }
    return action_map;
  }

  /**
   * 根据瓷砖类型产生所有旋转
   */
  static std::vector<Array2D<T>> generate_oriented(Array2D<T> data,
                                                   Symmetry symmetry) noexcept {
    std::vector<Array2D<T>> oriented;
    oriented.push_back(data);

    switch (symmetry) {
    case Symmetry::I:
    case Symmetry::backslash:
      oriented.push_back(data.rotated());
      break;
    case Symmetry::T:
    case Symmetry::L:
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      break;
    case Symmetry::P:
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated().reflected());
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      oriented.push_back(data = data.rotated());
      break;
    default:
      break;
    }

    return oriented;
  }

  /**
   * 构造函数
   */
  Tile(std::vector<Array2D<T>> data, Symmetry symmetry, double weight) noexcept
      : data(data), symmetry(symmetry), weight(weight) {}

  /*
   * 创建瓷砖
   */
  Tile(Array2D<T> data, Symmetry symmetry, double weight) noexcept
      : data(generate_oriented(data, symmetry)), symmetry(symmetry),
        weight(weight) {}
};


struct TilingWFCOptions {
  bool periodic_output;
};

/**
 * 用wfc产生tilemap新图
 */
template <typename T> class TilingWFC {
private:
  /**
   * 存储瓷砖
   */
  std::vector<Tile<T>> tiles;

  std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile;

  std::vector<std::vector<unsigned>> oriented_tile_ids;



  TilingWFCOptions options;

  /**
   * wfc底层算法
   */
  WFC wfc;

  /**
   * id映射瓷砖
   */
  static std::pair<std::vector<std::pair<unsigned, unsigned>>,
                   std::vector<std::vector<unsigned>>>
  generate_oriented_tile_ids(const std::vector<Tile<T>> &tiles) noexcept {
    std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile;
    std::vector<std::vector<unsigned>> oriented_tile_ids;

    unsigned id = 0;
    for (unsigned i = 0; i < tiles.size(); i++) {
      oriented_tile_ids.push_back({});
      for (unsigned j = 0; j < tiles[i].data.size(); j++) {
        id_to_oriented_tile.push_back({i, j});
        oriented_tile_ids[i].push_back(id);
        id++;
      }
    }

    return {id_to_oriented_tile, oriented_tile_ids};
  }

  /**
   * 创造传递器
   */
  static std::vector<std::array<std::vector<unsigned>, 4>> generate_propagator(
      const std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned>>
          &neighbors,
      std::vector<Tile<T>> tiles,
      std::vector<std::pair<unsigned, unsigned>> id_to_oriented_tile,
      std::vector<std::vector<unsigned>> oriented_tile_ids) {
    size_t nb_oriented_tiles = id_to_oriented_tile.size();
    std::vector<std::array<std::vector<bool>, 4>> dense_propagator(
        nb_oriented_tiles, {std::vector<bool>(nb_oriented_tiles, false),
                            std::vector<bool>(nb_oriented_tiles, false),
                            std::vector<bool>(nb_oriented_tiles, false),
                            std::vector<bool>(nb_oriented_tiles, false)});

    for (auto neighbor : neighbors) {
      unsigned tile1 = std::get<0>(neighbor);
      unsigned orientation1 = std::get<1>(neighbor);
      unsigned tile2 = std::get<2>(neighbor);
      unsigned orientation2 = std::get<3>(neighbor);
      std::vector<std::vector<unsigned>> action_map1 =
          Tile<T>::generate_action_map(tiles[tile1].symmetry);
      std::vector<std::vector<unsigned>> action_map2 =
          Tile<T>::generate_action_map(tiles[tile2].symmetry);

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

      add(0, 2);
      add(1, 0);
      add(2, 1);
      add(3, 3);
      add(4, 1);
      add(5, 3);
      add(6, 2);
      add(7, 0);
    }

    std::vector<std::array<std::vector<unsigned>, 4>> propagator(
        nb_oriented_tiles);
    for (size_t i = 0; i < nb_oriented_tiles; ++i) {
      for (size_t j = 0; j < nb_oriented_tiles; ++j) {
        for (size_t d = 0; d < 4; ++d) {
          if (dense_propagator[i][d][j]) {
            propagator[i][d].push_back(j);
          }
        }
      }
    }

    return propagator;
  }

  /**
   * 获得瓷砖的概率
   */
  static std::vector<double>
  get_tiles_weights(const std::vector<Tile<T>> &tiles) {
    std::vector<double> frequencies;
    for (size_t i = 0; i < tiles.size(); ++i) {
      for (size_t j = 0; j < tiles[i].data.size(); ++j) {
        frequencies.push_back(tiles[i].weight / tiles[i].data.size());
      }
    }
    return frequencies;
  }

  /**
   * 转换数据
   */
  Array2D<T> id_to_tiling(Array2D<unsigned> ids) {
    unsigned size = tiles[0].data[0].height;
    Array2D<T> tiling(size * ids.height, size * ids.width);
    for (unsigned i = 0; i < ids.height; i++) {
      for (unsigned j = 0; j < ids.width; j++) {
        std::pair<unsigned, unsigned> oriented_tile =
            id_to_oriented_tile[ids.get(i, j)];
        for (unsigned y = 0; y < size; y++) {
          for (unsigned x = 0; x < size; x++) {
            tiling.get(i * size + y, j * size + x) =
                tiles[oriented_tile.first].data[oriented_tile.second].get(y, x);
          }
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
      const std::vector<Tile<T>> &tiles,
      const std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned>>
          &neighbors,
      const unsigned height, const unsigned width,
      const TilingWFCOptions &options, int seed)
      : tiles(tiles),
        id_to_oriented_tile(generate_oriented_tile_ids(tiles).first),
        oriented_tile_ids(generate_oriented_tile_ids(tiles).second),
        options(options),
        wfc(options.periodic_output, seed, get_tiles_weights(tiles),
            generate_propagator(neighbors, tiles, id_to_oriented_tile,
                                oriented_tile_ids),
            height, width) {}

  /**
   * 运行算法入口
   */
  std::optional<Array2D<T>> run() {
    auto a = wfc.run();
    if (a == std::nullopt) {
      return std::nullopt;
    }
    return id_to_tiling(*a);
  }
};

