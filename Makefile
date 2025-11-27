# Variables
CXX = g++
CXXFLAGS = -Wall -g

# Main Target
all: fpga_sim

# Compilation Rule
fpga_sim: megaproject.cpp
	$(CXX) $(CXXFLAGS) megaproject.cpp -o fpga_sim

# Run Rule
run: fpga_sim
	./fpga_sim

# Clean Rule (Safayi)
# Yeh command generated images ko delete karegi taakay folder clean rahe
clean:
	rm -f fpga_sim final_output.ppm debug_stage_*.ppm
