#include <fstream>
#include <cstdio>
#include <random>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cstddef>
#include <iostream>
#include <map>

std::mt19937 gen(404);

template<typename T>
class Matrix {
public:
    explicit Matrix(std::size_t width, std::size_t height, T default_value=T{}) : width_(width), height_(height), data_(width*height, default_value) {}
    
    decltype(auto) operator()(std::size_t x, std::size_t y) const {
        return data_[y*width_+x];
    }
    
    decltype(auto) operator()(std::size_t x, std::size_t y) {
        return data_[y*width_+x];
    }
    
    operator Matrix<bool>() const {
        Matrix<bool> result(width_,height_);
        for(std::size_t y=0;y<height_;++y) {
            for(std::size_t x=0;x<width_;++x) {
                result(x,y) = static_cast<bool>((*this)(x,y));
            }
        }
        return result;
    }
    
    auto width() const {
        return width_;
    }
    
    auto height() const {
        return height_;
    }
    
    auto begin() const {
        return data_.begin();
    }
    auto begin() {
        return data_.begin();
    }
    auto end() const {
        return data_.end();
    }
    auto end() {
        return data_.end();
    }
    
    auto data() const {
        return data_;
    }
    
private:
    std::size_t width_;
    std::size_t height_;
    std::vector<T> data_;
};

template<typename T>
bool operator==(const Matrix<T>& lhs, const Matrix<T>& rhs) {
    return lhs.width() == rhs.width() && lhs.data() == rhs.data();
}

template<typename T>
struct std::hash<Matrix<T>> {
    std::size_t operator()(const Matrix<T>& matrix) const noexcept {
        auto h1 = std::hash<std::size_t>{}(matrix.width());
        auto h2 = std::hash<std::size_t>{}(matrix.height());
        auto h3 = std::hash<std::vector<T>>{}(matrix.data());
        return h1^ (h2<<1) ^ (h3<<2);
    }
};

template<typename T>
std::size_t popcount(const Matrix<T>& matrix) {
    std::size_t result = 0;
    for(auto bit : matrix) {
        result+=!!bit;
    }
    return result;
}

class extract_filled_region_detail {
public:
    explicit extract_filled_region_detail(const Matrix<std::byte>& bitmap) :
      bitmap(bitmap), scanned(bitmap.width(), bitmap.height()) {
    }
    
    void scan(std::size_t x, std::size_t y) {
        if (std::to_integer<std::size_t>(scanned(x,y)) || std::to_integer<std::size_t>(bitmap(x,y))==0) {
            return;
        }
        scanned(x,y) = std::byte{1};
        top=std::min(top, y);
        bottom=std::max(bottom, y);
        left=std::min(left, x);
        right=std::max(right, x);
        
        if(x>0) {
            scan(x-1, y);
        }
        if(x+1<bitmap.width()) {
            scan(x+1, y);
        }
        if(y>0) {
            scan(x, y-1);
        }
        if(y+1<bitmap.height()) {
            scan(x, y+1);
        }
    }
    
    void extract_pattern(std::size_t x, std::size_t y) {
        if (std::to_integer<std::size_t>(scanned(x, y)) || std::to_integer<std::size_t>((*pattern)(x-left, y-top)) == 0) {
            return;
        }
        (*pattern)(x-left, y-top) = std::byte{0};
        
        if(x>left) {
            extract_pattern(x-1, y);
        }
        if(x<right) {
            extract_pattern(x+1, y);
        }
        if(y>top) {
            extract_pattern(x, y-1);
        }
        if(y<bottom) {
            extract_pattern(x, y+1);
        }
    }
    
    Matrix<bool> extract_compressed_pattern(std::size_t x, std::size_t y) {
        left=right=x;
        top=bottom=y;
        scan(x,y);
        pattern.emplace(right-left+1, bottom-top+1, std::byte{1});
        for(std::size_t horizontal=left; horizontal<=right;++horizontal) {
            extract_pattern(horizontal, top);
            extract_pattern(horizontal, bottom);
        }
        for(std::size_t vertical=top+1; vertical+1<=bottom; ++vertical) {
            extract_pattern(left, vertical);
            extract_pattern(right, vertical);
        }
        return *pattern;
    }
    
private:
    const Matrix<std::byte>& bitmap;
    std::size_t top, bottom, left, right;
    Matrix<std::byte> scanned;
    std::optional<Matrix<std::byte>> pattern;
};

Matrix<bool> extract_filled_region(const Matrix<std::byte>& bitmap, std::size_t x, std::size_t y) {
    extract_filled_region_detail detail(bitmap);
    return detail.extract_compressed_pattern(x,y);
}

std::unordered_map<std::size_t, std::unordered_set<Matrix<bool>>>
generate_patterns(std::size_t width, std::size_t height, double fill_ratio, std::size_t iterations) {
    std::unordered_map<std::size_t, std::unordered_set<Matrix<bool>>> result;
    
    std::uniform_int_distribution<std::size_t> x_dist(0, width-1);
    std::uniform_int_distribution<std::size_t> y_dist(0, height-1);
    const std::size_t num_samples= width*height*fill_ratio;
    
    for(std::size_t i=0;i<iterations;++i) {
        Matrix<std::byte> bitmap(width, height);
        
        for(std::size_t sample = 0; sample<num_samples; ++sample) {
            std::size_t x=x_dist(gen), y=y_dist(gen);
            if (std::to_integer<std::size_t>(bitmap(x,y))) {
                continue;
            }
            bitmap(x,y) = std::byte{1};
            Matrix<bool> pattern = extract_filled_region(bitmap, x, y);
            std::size_t count = popcount(pattern);
            result[count].insert(std::move(pattern));
        }
    }
    return result;
}

std::size_t apply_pattern(Matrix<std::uint8_t>& texture, const Matrix<bool>& pattern, std::uint8_t value, std::size_t x, std::size_t y) {
    std::size_t result = 0;
    for(std::size_t row=0;row<pattern.height();++row) {
        for(std::size_t column=0;column<pattern.width();++column) {
            if(pattern(column, row) && texture((x+column)%texture.width(),(y+row)%texture.height())!=value) {
                texture((x+column)%texture.width(),(y+row)%texture.height())=value;
                result+=1;
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
    std::size_t width = 40000, height = 40000;
    
    std::map<std::size_t, std::vector<Matrix<bool>>> patterns;
    {
        std::unordered_map<std::size_t, std::unordered_set<Matrix<bool>>> patterns_sparse = generate_patterns(500, 500, 0.9, 1);
        for(auto&[key, value] : patterns_sparse) {
            std::vector<Matrix<bool>> patterns_for_count;
            for(auto &pattern : value) {
                patterns_for_count.push_back(std::move(pattern));
            }
            patterns[key]=std::move(patterns_for_count);
        }
    }
    
    const std::array colors = {
        color_properties{200, 0.8,  0.0005,   1.2},
        color_properties{150, 0.5,  0.00005,  1.1},
        color_properties{100, 0.1,  0.00005,  1.1},
        color_properties{0,   0.05, 0.0000002, 50},
    };
    
    Matrix<std::uint8_t> texture(width, height, 255);
    std::uniform_int_distribution<std::size_t> x_dist(0, width-1);
    std::uniform_int_distribution<std::size_t> y_dist(0, height-1);
    
    for(const color_properties& color : colors) {
        std::size_t current_fill = 0;
        const std::size_t target_fill = width*height*color.fill_ratio;
        const double fill_target = width*height*color.pattern_size_ratio;
        const std::size_t smallest_acceptable = fill_target/color.pattern_size_tolerance;
        const std::size_t biggest_acceptable = fill_target*color.pattern_size_tolerance;
        auto left_i = patterns.lower_bound(smallest_acceptable);
        auto right_i = patterns.upper_bound(biggest_acceptable);
        std::size_t num_buckets = std::distance(left_i, right_i);
        if(num_buckets == 0) {
            std::cout<<"No buckets in range ["<<smallest_acceptable<<"; "<<biggest_acceptable<<"]"<<std::endl;
            abort();
        } else {
            std::cout<<"Bucket ["<<smallest_acceptable<<"; "<<biggest_acceptable<<"], size: "<<num_buckets<<"\n";
        }
        std::uniform_int_distribution<std::size_t> iterator_dist(0, num_buckets-1);
        std::advance(left_i, iterator_dist(gen));
        const std::vector<Matrix<bool>> &patterns_to_apply=left_i->second;
        
        std::uniform_int_distribution<std::size_t> dist(0, patterns_to_apply.size()-1);
        while(current_fill<target_fill) {
            const Matrix<bool>& pattern = patterns_to_apply[dist(gen)];
            current_fill+=apply_pattern(texture, pattern, color.color, x_dist(gen), y_dist(gen));
        }
    }
    
    std::ofstream ofs("texture.ppm", std::ios_base::out | std::ios_base::binary);
    ofs<<"P5\n"<<width<<' '<<height<<"\n255\n";
    for(std::size_t y=0;y<height;++y) {
        for(std::size_t x=0;x<width;++x) {
            ofs << texture(x,y);
        }
    }
}
