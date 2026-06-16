CXX      ?= g++
CXXFLAGS  = -Wall -O2 -std=c++17 -I/usr/include/opencv4
LDFLAGS   = -lopencv_core -lopencv_videoio -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
TARGET    = timestamp_overlay

$(TARGET): timestamp_overlay.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) timestamp_overlay.cpp $(LDFLAGS)

clean:
	rm -f $(TARGET)
