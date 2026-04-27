// DTED2 load POC — set FILEPATH before compiling.
// Build: g++ -std=c++17 -O2 -o dted2_poc dted2_poc.cpp && ./dted2_poc

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

static const char* FILEPATH = "path/to/your/file.dt2";

static constexpr int DTED2_COLS = 3601;
static constexpr int DTED2_ROWS = 3601;

static constexpr int UHL_SIZE     = 80;
static constexpr int DSI_SIZE     = 648;
static constexpr int ACC_SIZE     = 2700;
static constexpr int HEADER_TOTAL = UHL_SIZE + DSI_SIZE + ACC_SIZE; // 3428

static constexpr int RECORD_HEADER = 8;
static constexpr int RECORD_FOOTER = 4; // checksum

// 3601x3601x2 = ~26 MB — global to avoid stack overflow.
static int16_t elevation[DTED2_COLS * DTED2_ROWS];

// Parses DDDMMSSH (lon, len=8) or DDMMSSH (lat, len=7) from UHL.
static double parse_coord(const char* s, int len)
{
    char hemi = s[len - 1];
    int coord_len = len - 1;
    int deg, min, sec;
    if (coord_len == 7) {
        deg = (s[0]-'0')*100 + (s[1]-'0')*10 + (s[2]-'0');
        min = (s[3]-'0')*10  + (s[4]-'0');
        sec = (s[5]-'0')*10  + (s[6]-'0');
    } else {
        deg = (s[0]-'0')*10  + (s[1]-'0');
        min = (s[2]-'0')*10  + (s[3]-'0');
        sec = (s[4]-'0')*10  + (s[5]-'0');
    }
    double v = deg + min / 60.0 + sec / 3600.0;
    return (hemi == 'S' || hemi == 'W') ? -v : v;
}

int main()
{
    std::ifstream f(FILEPATH, std::ios::binary);
    if (!f) { fprintf(stderr, "Cannot open: %s\n", FILEPATH); return 1; }

    // --- UHL (80 bytes) ---
    char uhl[UHL_SIZE];
    f.read(uhl, UHL_SIZE);

    // Bytes 0-3:  sentinel "UHL1"
    // Bytes 4-11: longitude of SW corner DDDMMSSH
    // Bytes 12-18: latitude  of SW corner DDMMSSH
    // Bytes 47-50: num longitude lines (cols) as 4-char ASCII
    // Bytes 51-54: num latitude  points (rows) as 4-char ASCII
    double origin_lon = parse_coord(uhl + 4,  8);
    double origin_lat = parse_coord(uhl + 12, 7);

    char cols_str[5] = {}, rows_str[5] = {};
    memcpy(cols_str, uhl + 47, 4);
    memcpy(rows_str, uhl + 51, 4);
    int uhl_cols = atoi(cols_str);
    int uhl_rows = atoi(rows_str);

    printf("UHL sentinel : %.4s\n", uhl);
    printf("Origin       : lat=%.4f  lon=%.4f\n", origin_lat, origin_lon);
    printf("Grid (UHL)   : %d cols x %d rows\n", uhl_cols, uhl_rows);

    if (uhl_cols != DTED2_COLS || uhl_rows != DTED2_ROWS) {
        fprintf(stderr, "WARNING: expected %dx%d, UHL says %dx%d\n",
                DTED2_COLS, DTED2_ROWS, uhl_cols, uhl_rows);
    }

    // --- Skip DSI + ACC, jump to data ---
    f.seekg(HEADER_TOTAL, std::ios::beg);

    // --- Read columns ---
    uint8_t row_buf[DTED2_ROWS * 2];
    int voids = 0;

    for (int col = 0; col < DTED2_COLS; ++col) {
        f.seekg(RECORD_HEADER, std::ios::cur);
        f.read(reinterpret_cast<char*>(row_buf), DTED2_ROWS * 2);
        f.seekg(RECORD_FOOTER, std::ios::cur);

        for (int row = 0; row < DTED2_ROWS; ++row) {
            int16_t v = static_cast<int16_t>((row_buf[row*2] << 8) | row_buf[row*2+1]);
            elevation[col * DTED2_ROWS + row] = v;
            if (v == -32768) ++voids;
        }
    }

    if (!f) { fprintf(stderr, "Read error or unexpected EOF\n"); return 1; }

    // --- Stats ---
    int16_t emin = 32767, emax = -32767;
    for (int i = 0; i < DTED2_COLS * DTED2_ROWS; ++i) {
        int16_t v = elevation[i];
        if (v == -32768) continue;
        if (v < emin) emin = v;
        if (v > emax) emax = v;
    }

    int center_col = DTED2_COLS / 2, center_row = DTED2_ROWS / 2;
    printf("Elev min     : %d m\n", emin);
    printf("Elev max     : %d m\n", emax);
    printf("Void posts   : %d / %d\n", voids, DTED2_COLS * DTED2_ROWS);
    printf("Center post  : col=%d row=%d  elev=%d m\n",
           center_col, center_row,
           elevation[center_col * DTED2_ROWS + center_row]);

    // --- Point lookup ---
    static constexpr double QUERY_LAT = 31.8589944;
    static constexpr double QUERY_LON = 34.9180298;

    double frac_col = (QUERY_LON - origin_lon) * (DTED2_COLS - 1);
    double frac_row = (QUERY_LAT - origin_lat) * (DTED2_ROWS - 1);
    int col = static_cast<int>(frac_col);
    int row = static_cast<int>(frac_row);

    if (col < 0 || col >= DTED2_COLS || row < 0 || row >= DTED2_ROWS) {
        fprintf(stderr, "Query point (%.7f, %.7f) is outside this tile\n", QUERY_LAT, QUERY_LON);
    } else {
        int16_t v = elevation[col * DTED2_ROWS + row];
        double elev = (v == -32768) ? 0.0 : static_cast<double>(v);
        printf("Query        : lat=%.7f  lon=%.7f\n", QUERY_LAT, QUERY_LON);
        printf("Nearest post : col=%d  row=%d  elev=%.0f m ASL\n", col, row, elev);
    }

    printf("OK\n");
    return 0;
}
