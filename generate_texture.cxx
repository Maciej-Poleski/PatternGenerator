#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <random>

#include "matrix.hxx"
#include "ppm.hxx"

struct pattern {
  std::size_t size;
  ppm::image<ppm::bitmap> image;
};

std::vector<pattern> read_patterns(std::filesystem::path location) {
  std::vector<pattern> result;
  for (std::filesystem::directory_entry entry :
       std::filesystem::directory_iterator(location)) {
    if (entry.is_regular_file() && entry.path().extension() == ".ppm") {
      std::string stem = entry.path().stem();
      std::size_t size = std::strtoul(stem.c_str(), nullptr, 10);
      if (stem != std::to_string(size)) {
        continue;
      }
      auto maybe_image = ppm::image<ppm::bitmap>::open(entry);
      if (maybe_image.has_value()) {
        result.push_back({size, std::move(maybe_image.value())});
      }
    }
  }

  std::sort(result.begin(), result.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.size < rhs.size;
  });
  return result;
};

std::size_t apply_pattern(matrix<std::uint8_t> &texture,
                          const ppm::image<ppm::bitmap> &pattern,
                          std::uint8_t value, std::size_t x, std::size_t y) {
  std::size_t result = 0;
  for (std::size_t row = 0; row < pattern.height(); ++row) {
    for (std::size_t column = 0; column < pattern.width(); ++column) {
      if (pattern(column, row) &&
          texture((x + column) % texture.width(),
                  (y + row) % texture.height()) != value) {
        texture((x + column) % texture.width(), (y + row) % texture.height()) =
            value;
        result += 1;
      }
    }
  }
  return result;
}

std::mt19937 gen(404);

template <std::size_t dim>
matrix<std::uint8_t> apply_transformation(
    const matrix<std::uint8_t> texture,
    const std::array<std::array<double, dim>, dim> &transformation) {
  matrix<std::uint8_t> result(texture.width(), texture.height());
  double total_weight = 0;
  for (auto row : transformation) {
    for (auto cell : row) {
      total_weight += cell;
    }
  }
  for (std::size_t y = 0; y < texture.height(); ++y) {
    for (std::size_t x = 0; x < texture.width(); ++x) {
      double total_value = 0;
      for (std::size_t vertical_offset = 0; vertical_offset < dim;
           ++vertical_offset) {
        for (std::size_t horizontal_offset = 0; horizontal_offset < dim;
             ++horizontal_offset) {
          total_value +=
              transformation[vertical_offset][horizontal_offset] *
              texture((texture.width() + x + horizontal_offset - dim / 2) %
                          texture.width(),
                      (texture.height() + y + vertical_offset - dim / 2) %
                          texture.height());
        }
      }
      result(x, y) =
          static_cast<std::uint8_t>(total_value / total_weight + 0.5);
    }
  }
  return result;
}

void apply_noise(matrix<std::uint8_t> &texture, std::size_t delta) {
  std::uniform_int_distribution<std::int8_t> noise_dist(-delta, delta);

  for (std::size_t y = 0; y < texture.height(); ++y) {
    for (std::size_t x = 0; x < texture.width(); ++x) {
      texture(x, y) = std::clamp(texture(x, y) + noise_dist(gen), 0, 255);
    }
  }
}

struct color_properties {
  std::uint8_t color;
  double fill_ratio;
  double pattern_size_ratio;
  double pattern_size_tolerance;
};

int main(int argc, char **argv) {
  std::size_t width = 4000, height = 4000;

  const std::vector<pattern> patterns =
      read_patterns(std::filesystem::current_path());

  const std::array colors = {
      color_properties{200, 0.80, 0.0005, 1.2},
      color_properties{150, 0.40, 0.00005, 1.1},
      color_properties{100, 0.10, 0.00005, 1.1},
      color_properties{40, 0.05, 0.0000002, 50},
      color_properties{130, 0.05, 0.0000002, 5},
      color_properties{230, 0.10, 0.0000002, 3},
  };

  matrix<std::uint8_t> texture(width, height, 255);
  std::uniform_int_distribution<std::size_t> x_dist(0, width - 1);
  std::uniform_int_distribution<std::size_t> y_dist(0, height - 1);

  for (const color_properties &color : colors) {
    std::size_t current_fill = 0;
    const std::size_t target_fill = width * height * color.fill_ratio;
    const double fill_target = width * height * color.pattern_size_ratio;
    const std::size_t smallest_acceptable =
        fill_target / color.pattern_size_tolerance;
    const std::size_t biggest_acceptable =
        fill_target * color.pattern_size_tolerance;
    auto left_i = std::lower_bound(
        patterns.begin(), patterns.end(), smallest_acceptable,
        [](const pattern &p, std::size_t size) { return p.size < size; });
    auto right_i = std::upper_bound(
        patterns.begin(), patterns.end(), biggest_acceptable,
        [](std::size_t size, const pattern &p) { return size < p.size; });
    std::size_t num_buckets = std::distance(left_i, right_i);
    if (num_buckets == 0) {
      std::cout << "No buckets in range [" << smallest_acceptable << "; "
                << biggest_acceptable << "]" << std::endl;
      abort();
    } else {
      std::cout << "Bucket [" << smallest_acceptable << "; "
                << biggest_acceptable << "], size: " << num_buckets << "\n";
    }
    std::uniform_int_distribution<std::size_t> iterator_dist(0,
                                                             num_buckets - 1);

    while (current_fill < target_fill) {
      auto i = left_i;
      std::advance(i, iterator_dist(gen));
      const ppm::image<ppm::bitmap> &pattern_to_apply = i->image;

      current_fill += apply_pattern(texture, pattern_to_apply, color.color,
                                    x_dist(gen), y_dist(gen));
    }
  }

  apply_noise(texture, 10);

  std::array transformation = {
      std::array{0.1, 0.2, 0.4, 0.2, 0.1},  //
      std::array{0.2, 0.4, 0.8, 0.4, 0.2},  //
      std::array{0.4, 0.8, 2.0, 0.8, 0.4},  //
      std::array{0.2, 0.4, 0.8, 0.4, 0.2},  //
      std::array{0.1, 0.2, 0.4, 0.2, 0.1},  //
  };

  ppm::image<ppm::graymap>::create(
      "texture.ppm", apply_transformation(texture, transformation));
}
