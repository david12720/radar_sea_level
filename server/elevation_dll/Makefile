CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -fPIC -std=c++11 -pthread
LDFLAGS  ?= -shared -pthread
TARGET   := libelevation.so
SRCS     := src/elevation_service.cpp src/tile_store.cpp \
            src/bicubic.cpp src/mapped_file.cpp
OBJS     := $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -DELEVATION_DLL_EXPORTS -Iinclude -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
