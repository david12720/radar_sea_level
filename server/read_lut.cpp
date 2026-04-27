/**
 * Minimal C++ utility to read and query binary LUT files.
 * Mirrors the logic of read_lut.py.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>

class PolarLut {
public:
    const uint32_t AZ_COUNT = 3600;      // 0.1 deg steps
    const double   AZ_STEP  = 0.1;
    const double   RNG_STEP = 15.0;      // 15 m steps

    bool load(const std::string& path, double max_range) {
        range_count_ = static_cast<uint32_t>(std::ceil(max_range / RNG_STEP)) + 1;
        size_t expected_size = static_cast<size_t>(AZ_COUNT) * range_count_ * sizeof(int32_t);

        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) {
            std::cerr << "Error: Could not open file " << path << "\n";
            return false;
        }

        if (static_cast<size_t>(f.tellg()) != expected_size) {
            std::cerr << "Error: File size mismatch. Expected " << expected_size << " bytes.\n";
            return false;
        }

        f.seekg(0, std::ios::beg);
        data_.resize(AZ_COUNT * range_count_);
        return (bool)f.read(reinterpret_cast<char*>(data_.data()), expected_size);
    }

    int32_t getElevation(double az_deg, double rng_m) const {
        if (data_.empty()) return 0;
        double az = std::fmod(az_deg, 360.0);
        if (az < 0) az += 360.0;

        uint32_t ai = static_cast<uint32_t>(std::round(az / AZ_STEP)) % AZ_COUNT;
        uint32_t ri = static_cast<uint32_t>(std::round(rng_m / RNG_STEP));
        ri = std::min(ri, range_count_ - 1);

        return data_[ai * range_count_ + ri];
    }

private:
    std::vector<int32_t> data_;
    uint32_t             range_count_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "Usage: read_lut <filename> <max_range_m> <az_deg> <range_m>\n";
        std::cout << "Example: read_lut output.bin 15000 12.5 4500\n";
        return 1;
    }

    std::string path = argv[1];
    double max_range = std::stod(argv[2]);
    double query_az  = std::stod(argv[3]);
    double query_rng = std::stod(argv[4]);

    PolarLut lut;
    if (lut.load(path, max_range)) {
        int32_t elev = lut.getElevation(query_az, query_rng);
        std::cout << "Elevation at " << query_az << " deg, " << query_rng << " m: " << elev << " m\n";
    }

    return 0;
}
