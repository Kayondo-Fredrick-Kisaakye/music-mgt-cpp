CXX := g++
CXXFLAGS := -std=c++17 -O2 -I. -Ivendor -Ivendor/nlohmann
LDLIBS := -lpq -pthread
TARGET := musicapp

.PHONY: all clean run schema

all: $(TARGET)

$(TARGET): main.cpp httplib.h vendor/nlohmann/json.hpp
	$(CXX) $(CXXFLAGS) main.cpp $(LDLIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

schema:
	psql "$${DATABASE_URL}" -v ON_ERROR_STOP=1 -f schema.sql

clean:
	rm -f $(TARGET)
