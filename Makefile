CXX = em++
HOST_CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -sUSE_WEBGL2=1 -sFULL_ES3=1
HOST_CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = index.js
TEST_TARGET = validation_tests
SRC = main.cpp environment.cpp gpu.cpp light.cpp material.cpp math3d.cpp sheet_mesh.cpp shaders.cpp
EMSDK_ENV = ./emsdk/emsdk_env.sh

all: $(TARGET)

$(TARGET): $(SRC)
	bash -c "source $(EMSDK_ENV) && $(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)"

run: $(TARGET)
	python3 -m http.server 8001

$(TEST_TARGET): validation.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) -o $(TEST_TARGET) validation.cpp

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f index.js index.wasm $(TEST_TARGET)
