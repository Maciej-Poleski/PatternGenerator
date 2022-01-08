#include "ppm.hxx"
#include <iostream>
#include <random>

std::mt19937 gen(404);

class extract_filled_region_detail {
public:
  explicit extract_filled_region_detail(const matrix<std::uint8_t> &bitmap)
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

  matrix<std::uint8_t> extract_compressed_pattern(std::size_t x,
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
  const matrix<std::uint8_t> &bitmap;
  std::size_t top_, bottom, left_, right;
  matrix<std::uint8_t> scanned;
  std::optional<matrix<std::uint8_t>> pattern;
};

struct filled_region {
  matrix<std::uint8_t> pattern;
  std::size_t anchor_left;
  std::size_t anchor_top;
};

filled_region extract_filled_region(const matrix<std::uint8_t> &bitmap,
                                    std::size_t x, std::size_t y) {
  extract_filled_region_detail detail(bitmap);
  return {detail.extract_compressed_pattern(x, y), detail.left(), detail.top()};
}

struct pattern {
  matrix<bool> pattern;
  std::size_t size;
};

void generate_patterns(std::size_t width, std::size_t height, double fill_ratio,
                       std::size_t iterations) {
  std::size_t biggest_pattern = 0;
  std::uniform_int_distribution<std::size_t> x_dist(0, width - 1);
  std::uniform_int_distribution<std::size_t> y_dist(0, height - 1);
  const std::size_t num_samples = width * height * fill_ratio;

  for (std::size_t i = 0; i < iterations; ++i) {
    matrix<std::uint8_t> bitmap(width, height);

    for (std::size_t sample = 0; biggest_pattern < num_samples; ++sample) {
      std::size_t x = x_dist(gen), y = y_dist(gen);
      if (bitmap(x, y)) {
        continue;
      }
      bitmap(x, y) = 1;
      filled_region region = extract_filled_region(bitmap, x, y);
      std::size_t count = popcount(region.pattern);
      if (biggest_pattern < count) {
        //                 bitmap.erase(region.pattern, region.anchor_left,
        //                 region.anchor_top);
        auto maybe_ppm = ppm::image<ppm::bitmap>::create(std::to_string(count)+".ppm",
                                                         region.pattern);
        biggest_pattern = count;
        std::cout << "Generated pattern: " << count << "\n";
        if (!maybe_ppm.has_value()) {
          std::cout << "PPM store failed.\n";
        }
        bitmap = matrix<std::uint8_t>(width, height);
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
}

int main(int argc, char **argv) { generate_patterns(1000, 1000, 0.9, 1); }
