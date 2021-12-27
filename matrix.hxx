#ifndef MATRIX_HXX
#define MATRIX_HXX

#include <vector>

template <typename T> class matrix {
public:
  explicit matrix(std::size_t width, std::size_t height, T default_value = T{})
      : width_(width), height_(height), data_(width * height, default_value) {}

  decltype(auto) operator()(std::size_t x, std::size_t y) const {
    return data_[y * width_ + x];
  }

  decltype(auto) operator()(std::size_t x, std::size_t y) {
    return data_[y * width_ + x];
  }

  operator matrix<bool>() const {
    matrix<bool> result(width_, height_);
    for (std::size_t y = 0; y < height_; ++y) {
      for (std::size_t x = 0; x < width_; ++x) {
        result(x, y) = static_cast<bool>((*this)(x, y));
      }
    }
    return result;
  }

  void erase(const matrix &matrix, std::size_t anchor_left,
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
bool operator==(const matrix<T> &lhs, const matrix<T> &rhs) {
  return lhs.width() == rhs.width() && lhs.data() == rhs.data();
}

template <typename T> struct std::hash<matrix<T>> {
  std::size_t operator()(const matrix<T> &matrix) const noexcept {
    auto h1 = std::hash<std::size_t>{}(matrix.width());
    auto h2 = std::hash<std::size_t>{}(matrix.height());
    auto h3 = std::hash<std::vector<T>>{}(matrix.data());
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

template <typename T> std::size_t popcount(const matrix<T> &matrix) {
  std::size_t result = 0;
  for (auto bit : matrix) {
    result += !!bit;
  }
  return result;
}

#endif
