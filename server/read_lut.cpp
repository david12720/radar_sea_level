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

// Static buffer sized for maximum possible LUT dimensions
#ifndef MAX_LUT_AZIMUTHS
    #define MAX_LUT_AZIMUTHS 3600
#endif
#ifndef MAX_LUT_RANGES
    #define MAX_LUT_RANGES   4000 // Covers up to 60km at 15m steps
#endif

class PolarLut {
public:
    const uint32_t AZ_COUNT = 3600;      // 0.1 deg steps
    const double   AZ_STEP  = 0.1;
    const double   RNG_STEP = 15.0;      // 15 m steps

    bool load(const std::string& path, double max_range) {
        // Changed: exact count without +1 (e.g., 15000 / 15 = 1000)
        range_count_ = static_cast<uint32_t>(std::floor(max_range / RNG_STEP));
        size_t total_cells = static_cast<size_t>(AZ_COUNT) * range_count_;
        size_t expected_size = total_cells * sizeof(int32_t);

        if (total_cells > MAX_LUT_AZIMUTHS * MAX_LUT_RANGES) {
            std::cerr << "Error: Request exceeds static buffer capacity.\n";
            return false;
        }

        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::cerr << "Error: Could not open file " << path << "\n";
            return false;
        }

        // Using the requested binary read style
        for (size_t i = 0; i < total_cells; ++i) {
            int32_t height_mtr;
            if (!f.read(reinterpret_cast<char*>(&height_mtr), sizeof(int32_t))) {
                std::cerr << "Error: Premature end of file.\n";
                return false;
            }
            results_buffer_[i] = height_mtr;
        }
        return true;
    }

    int32_t getElevation(double az_deg, double rng_m) const {
        double az = std::fmod(az_deg, 360.0);
        if (az < 0) az += 360.0;

        uint32_t ai = static_cast<uint32_t>(std::round(az / AZ_STEP)) % AZ_COUNT;
        uint32_t ri = static_cast<uint32_t>(std::round(rng_m / RNG_STEP));
        ri = std::min(ri, range_count_ - 1);

        return results_buffer_[ai * range_count_ + ri];
    }

private:
    uint32_t range_count_ = 0;
    static int32_t results_buffer_[MAX_LUT_AZIMUTHS * MAX_LUT_RANGES];
};

// Allocate the static buffer in global memory
int32_t PolarLut::results_buffer_[MAX_LUT_AZIMUTHS * MAX_LUT_RANGES];

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
