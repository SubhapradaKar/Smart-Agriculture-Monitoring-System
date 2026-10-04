CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

agri_monitor: agri_monitor.cpp ../include/agri_ioctl.h
	$(CXX) $(CXXFLAGS) -o $@ agri_monitor.cpp

clean:
	rm -f agri_monitor
