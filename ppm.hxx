#ifndef PPM_HXX
#define PPM_HXX

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "matrix.hxx"

namespace ppm {

struct bitmap {
  using pixel = bool;
  static constexpr std::optional<std::size_t> color_depth = std::nullopt;
  static constexpr std::size_t format = 4;

  static std::size_t blob_size(std::size_t width, std::size_t height);
  static void convert(std::byte *destination,
                      const matrix<std::uint8_t> &matrix);

  static pixel get_pixel(const std::byte *blob, std::size_t width,
                         std::size_t height, std::size_t x, std::size_t y);
};

struct graymap {
  using pixel = std::uint8_t;
  static constexpr std::optional<std::size_t> color_depth = 255;
  static constexpr std::size_t format = 5;

  static std::size_t blob_size(std::size_t width, std::size_t height);
  static void convert(std::byte *destination,
                      const matrix<std::uint8_t> &matrix);

  static pixel get_pixel(const std::byte *blob, std::size_t width,
                         std::size_t height, std::size_t x, std::size_t y);
};

template <class Format> class image {
public:
  image(const image &) = delete;
  image(image &&other);

  image &operator=(image &&other);

  ~image();

  std::size_t width() const;
  std::size_t height() const;

  typename Format::pixel operator()(std::size_t x, std::size_t y) const;

  static std::optional<image> create(const std::string &name,
                                     const matrix<std::uint8_t> &matrix);

  static std::optional<image> open(const std::filesystem::path &file);

private:
  explicit image(std::size_t width, std::size_t height, std::size_t blob_offset,
                 std::byte *data)
      : _width(width), _height(height), _blob_offset(blob_offset), _data(data) {
  }

  std::size_t data_size() const;

  std::size_t _width;
  std::size_t _height;
  std::size_t _blob_offset;
  std::byte *_data;
};

template <typename Format>
image<Format>::image(image &&other)
    : _width(other._width), _height(other._height),
      _blob_offset(other._blob_offset), _data(other._data) {
  other._data = nullptr;
}

template <typename Format>
image<Format> &image<Format>::operator=(image<Format> &&other) {
  _width = other._width;
  _height = other._height;
  _blob_offset = other._blob_offset;
  _data = other._data;
  other._data = nullptr;
  return *this;
}

template <typename Format> image<Format>::~image() {
  if (_data != nullptr && munmap(_data, data_size()) < 0) {
    perror("~image");
  }
}

template <class Format> std::size_t image<Format>::width() const {
  return _width;
}

template <class Format> std::size_t image<Format>::height() const {
  return _height;
}

template <class Format>
typename Format::pixel image<Format>::operator()(std::size_t x,
                                                 std::size_t y) const {
  return Format::get_pixel(_data + _blob_offset, _width, _height, x, y);
}

template <class Format>
std::optional<image<Format>>
image<Format>::create(const std::string &name,
                      const matrix<std::uint8_t> &matrix) {
  int fd = ::open(name.c_str(), O_CREAT | O_RDWR | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    perror("creat");
    return std::nullopt;
  }

  const std::size_t width = matrix.width(), height = matrix.height();
  std::ostringstream buffer;
  buffer << "P" << Format::format << ' ' << width << ' ' << height << ' ';
  if (Format::color_depth.has_value()) {
    buffer << Format::color_depth.value() << ' ';
  }
  std::string header = buffer.str();

  const std::size_t file_size =
      header.length() + Format::blob_size(matrix.width(), matrix.height());
  if (ftruncate(fd, file_size) < 0) {
    perror("ftruncate");
    return std::nullopt;
  }
  std::byte *data = reinterpret_cast<std::byte *>(
      mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  close(fd);
  if (data == MAP_FAILED) {
    perror("mmap");
    return std::nullopt;
  }
  std::memcpy(data, header.data(), header.length());
  Format::convert(data + header.length(), matrix);
  return image(width, height, header.length(), data);
}

template <class Format>
std::optional<image<Format>>
image<Format>::open(const std::filesystem::path &file) {
  int fd = ::open(file.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("image::open");
    return std::nullopt;
  }
  struct stat stat;
  if (fstat(fd, &stat) < 0) {
    perror("image::open");
    return std::nullopt;
  }
  std::byte *data = reinterpret_cast<std::byte *>(
      mmap(nullptr, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (data == MAP_FAILED) {
    perror("image::open");
    return std::nullopt;
  }
  std::size_t width, height;
  sscanf(reinterpret_cast<char *>(data + 3), "%zu%zu", &width, &height);
  return image(width, height, stat.st_size - Format::blob_size(width, height),
               data);
}

template <class Format> std::size_t image<Format>::data_size() const {
  return _blob_offset + Format::blob_size(_width, _height);
}

} // namespace ppm

#endif
