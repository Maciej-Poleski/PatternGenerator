#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

std::mt19937 gen(404);

template <typename T> class Matrix {
public:
  explicit Matrix(std::size_t width, std::size_t height, T default_value = T{})
      : width_(width), height_(height), data_(width * height, default_value) {}

  decltype(auto) operator()(std::size_t x, std::size_t y) const {
    return data_[y * width_ + x];
  }

  decltype(auto) operator()(std::size_t x, std::size_t y) {
    return data_[y * width_ + x];
  }

  operator Matrix<bool>() const {
    Matrix<bool> result(width_, height_);
    for (std::size_t y = 0; y < height_; ++y) {
      for (std::size_t x = 0; x < width_; ++x) {
        result(x, y) = static_cast<bool>((*this)(x, y));
      }
    }
    return result;
  }

  void erase(const Matrix &matrix, std::size_t anchor_left,
             std::size_t anchor_top) {
    for (std::size_t y = 0; y < matrix.height(); ++y) {
      for (std::size_t x = 0; x < matrix.width(); ++x) {
        if (matrix(x, y)) {
          (*this)(anchor_left + x, anchor_top + y) = T{};
        }
      }
    }
  }

  auto width() const { return width_; }

  auto height() const { return height_; }

  auto begin() const { return data_.begin(); }
  auto begin() { return data_.begin(); }
  auto end() const { return data_.end(); }
  auto end() { return data_.end(); }

  auto data() const { return data_; }

private:
  std::size_t width_;
  std::size_t height_;
  std::vector<T> data_;
};

template <typename T>
bool operator==(const Matrix<T> &lhs, const Matrix<T> &rhs) {
  return lhs.width() == rhs.width() && lhs.data() == rhs.data();
}

template <typename T> struct std::hash<Matrix<T>> {
  std::size_t operator()(const Matrix<T> &matrix) const noexcept {
    auto h1 = std::hash<std::size_t>{}(matrix.width());
    auto h2 = std::hash<std::size_t>{}(matrix.height());
    auto h3 = std::hash<std::vector<T>>{}(matrix.data());
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

template <typename T> std::size_t popcount(const Matrix<T> &matrix) {
  std::size_t result = 0;
  for (auto bit : matrix) {
    result += !!bit;
  }
  return result;
}

class extract_filled_region_detail {
public:
  explicit extract_filled_region_detail(const Matrix<std::uint8_t> &bitmap)
      : bitmap(bitmap), scanned(bitmap.width(), bitmap.height()) {}

  std::size_t left() const { return left_; }

  std::size_t top() const { return top_; }

  void scan(std::size_t x, std::size_t y) {
    if (scanned(x, y) || bitmap(x, y) == 0) {
      return;
    }
    scanned(x, y) = 1;
    top_ = std::min(top_, y);
    bottom = std::max(bottom, y);
    left_ = std::min(left_, x);
    right = std::max(right, x);

    if (x > 0) {
      scan(x - 1, y);
    }
    if (x + 1 < bitmap.width()) {
      scan(x + 1, y);
    }
    if (y > 0) {
      scan(x, y - 1);
    }
    if (y + 1 < bitmap.height()) {
      scan(x, y + 1);
    }
  }

  void extract_pattern(std::size_t x, std::size_t y) {
    if (scanned(x, y) || (*pattern)(x - left_, y - top_) == 0) {
      return;
    }
    (*pattern)(x - left_, y - top_) = 0;

    if (x > left_) {
      extract_pattern(x - 1, y);
    }
    if (x < right) {
      extract_pattern(x + 1, y);
    }
    if (y > top_) {
      extract_pattern(x, y - 1);
    }
    if (y < bottom) {
      extract_pattern(x, y + 1);
    }
  }

  Matrix<std::uint8_t> extract_compressed_pattern(std::size_t x,
                                                  std::size_t y) {
    left_ = right = x;
    top_ = bottom = y;
    scan(x, y);
    pattern.emplace(right - left_ + 1, bottom - top_ + 1, 1);
    for (std::size_t horizontal = left_; horizontal <= right; ++horizontal) {
      extract_pattern(horizontal, top_);
      extract_pattern(horizontal, bottom);
    }
    for (std::size_t vertical = top_ + 1; vertical + 1 <= bottom; ++vertical) {
      extract_pattern(left_, vertical);
      extract_pattern(right, vertical);
    }
    return *pattern;
  }

private:
  const Matrix<std::uint8_t> &bitmap;
  std::size_t top_, bottom, left_, right;
  Matrix<std::uint8_t> scanned;
  std::optional<Matrix<std::uint8_t>> pattern;
};

struct filled_region {
  Matrix<std::uint8_t> pattern;
  std::size_t anchor_left;
  std::size_t anchor_top;
};

filled_region extract_filled_region(const Matrix<std::uint8_t> &bitmap,
                                    std::size_t x, std::size_t y) {
  extract_filled_region_detail detail(bitmap);
  return {detail.extract_compressed_pattern(x, y), detail.left(), detail.top()};
}

struct pattern {
  Matrix<bool> pattern;
  std::size_t size;
};

std::vector<pattern> generate_patterns(std::size_t width, std::size_t height,
                                       double fill_ratio,
                                       std::size_t iterations) {
  std::vector<pattern> result;

  std::uniform_int_distribution<std::size_t> x_dist(0, width - 1);
  std::uniform_int_distribution<std::size_t> y_dist(0, height - 1);
  const std::size_t num_samples = width * height * fill_ratio;

  for (std::size_t i = 0; i < iterations; ++i) {
    Matrix<std::uint8_t> bitmap(width, height);

    for (std::size_t sample = 0;
         result.empty() || result.back().size < num_samples; ++sample) {
      std::size_t x = x_dist(gen), y = y_dist(gen);
      if (bitmap(x, y)) {
        continue;
      }
      bitmap(x, y) = 1;
      filled_region region = extract_filled_region(bitmap, x, y);
      std::size_t count = popcount(region.pattern);
      if (result.empty() || result.back().size < count) {
        //                 bitmap.erase(region.pattern, region.anchor_left,
        //                 region.anchor_top);
        result.push_back({std::move(region.pattern), count});
        std::cout << "Generated pattern: " << count << "\n";
        bitmap = Matrix<std::uint8_t>(width, height);
        for (std::size_t i = 0; i < count; ++i) {
          std::size_t x, y;
          do {
            x = x_dist(gen), y = y_dist(gen);
          } while (bitmap(x, y));
          bitmap(x, y) = 1;
        }
      }
    }
  }
  return result;
}

std::size_t apply_pattern(Matrix<std::uint8_t> &texture,
                          const Matrix<bool> &pattern, std::uint8_t value,
                          std::size_t x, std::size_t y) {
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

struct color_properties {
  std::uint8_t color;
  double fill_ratio;
  double pattern_size_ratio;
  double pattern_size_tolerance;
};

int main() {
  std::size_t width = 4000, height = 4000;

  std::vector<pattern> patterns = generate_patterns(1000, 1000, 0.9, 1);

  //     const std::array colors = {
  //         color_properties{200, 0.8,  0.0005,   1.2},
  //         color_properties{150, 0.5,  0.00005,  1.1},
  //         color_properties{100, 0.1,  0.00005,  1.1},
  //         color_properties{0,   0.05, 0.0000002, 50},
  //     };
  //
  //     Matrix<std::uint8_t> texture(width, height, 255);
  //     std::uniform_int_distribution<std::size_t> x_dist(0, width-1);
  //     std::uniform_int_distribution<std::size_t> y_dist(0, height-1);
  //
  //     for(const color_properties& color : colors) {
  //         std::size_t current_fill = 0;
  //         const std::size_t target_fill = width*height*color.fill_ratio;
  //         const double fill_target = width*height*color.pattern_size_ratio;
  //         const std::size_t smallest_acceptable =
  //         fill_target/color.pattern_size_tolerance; const std::size_t
  //         biggest_acceptable = fill_target*color.pattern_size_tolerance; auto
  //         left_i = patterns.lower_bound(smallest_acceptable); auto right_i =
  //         patterns.upper_bound(biggest_acceptable); std::size_t num_buckets =
  //         std::distance(left_i, right_i); if(num_buckets == 0) {
  //             std::cout<<"No buckets in range ["<<smallest_acceptable<<";
  //             "<<biggest_acceptable<<"]"<<std::endl; abort();
  //         } else {
  //             std::cout<<"Bucket ["<<smallest_acceptable<<";
  //             "<<biggest_acceptable<<"], size: "<<num_buckets<<"\n";
  //         }
  //         std::uniform_int_distribution<std::size_t> iterator_dist(0,
  //         num_buckets-1); std::advance(left_i, iterator_dist(gen)); const
  //         std::vector<Matrix<bool>> &patterns_to_apply=left_i->second;
  //
  //         std::uniform_int_distribution<std::size_t> dist(0,
  //         patterns_to_apply.size()-1); while(current_fill<target_fill) {
  //             const Matrix<bool>& pattern = patterns_to_apply[dist(gen)];
  //             current_fill+=apply_pattern(texture, pattern, color.color,
  //             x_dist(gen), y_dist(gen));
  //         }
  //     }

  for (std::size_t i = 0; i < patterns.size(); ++i) {
    std::ofstream ofs(std::to_string(patterns[i].size) + ".ppm",
                      std::ios_base::out | std::ios_base::binary);
    const Matrix<bool> &pattern = patterns[i].pattern;
    ofs << "P5\n" << pattern.width() << ' ' << pattern.height() << "\n255\n";
    for (std::size_t y = 0; y < pattern.height(); ++y) {
      for (std::size_t x = 0; x < pattern.width(); ++x) {
        ofs << static_cast<std::uint8_t>(
            255 * (1 - static_cast<uint8_t>(pattern(x, y))));
      }
    }
  }
}
