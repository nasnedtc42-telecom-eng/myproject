/**
 * ============================================================
 * PROJECT: FPGA-Based Image Processing Pipeline Simulator
 * AUTHOR: [Your Name Here]
 * DESCRIPTION: 
 * This project simulates a hardware image processing pipeline using C++.
 * It utilizes Fixed-Point Arithmetic (Integer Math) to emulate FPGA 
 * behavior, as floating-point operations are costly in hardware.
 * * FEATURES:
 * 1. Manual Dynamic Memory Management (Simulating DMA)
 * 2. Pipeline Architecture with Double Buffering
 * 3. Intermediate Debug Output generation for every stage
 * 4. Polymorphic Filter Design
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint> // For uint8_t (0-255 standard pixel range)
#include <iomanip>

// --- HARDWARE EMULATION SETTINGS ---
#define FIXED_POINT_MODE // Enable integer-only math (Hardware optimization)
#define DEBUG_MODE       // Enable system status logging

using namespace std;

// ============================================================
// MODULE 1: LOGGER (System Monitor)
// ============================================================
class Logger {
public:
    // General system logs
    static void log(string module, string message) {
        cout << left << setw(12) << "[" + module + "]" << " : " << message << endl;
    }
    
    // Hardware register/memory logs
    static void hardwareLog(string msg) {
        #ifdef DEBUG_MODE
        cout << "   >> [HW_REG] " << msg << endl;
        #endif
    }
};

// ============================================================
// MODULE 2: MATH ENGINE (Fixed Point Arithmetic)
// ============================================================
// FPGAs prefer integers over floating-point numbers.
// We simulate decimals using Scaled Integers (Q8.8 format).
// Example: 1.5 is represented as 384 (1.5 * 256)
namespace HardwareMath {
    // Convert Decimal to Fixed Point (Scale up by 256)
    int toFixed(double f) { return (int)(f * 256.0); }
    
    // Convert back/Normalize (Divide by 256 using Bit Shift)
    // Bit shifting (>>) is significantly faster than division (/)
    int fromFixed(int i)  { return i >> 8; }           
    
    // Clamp values to valid pixel range (0-255) to prevent overflow
    uint8_t clamp(int val) {
        if (val < 0) return 0;
        if (val > 255) return 255;
        return (uint8_t)val;
    }
}

// ============================================================
// MODULE 3: IMAGE BUFFER (Memory Block)
// ============================================================
struct Pixel {
    uint8_t r, g, b; // Red, Green, Blue channels
};

class Image {
private:
    int width, height;
    Pixel* data; // Raw pointer to simulate a hardware memory block

public:
    Image(int w, int h) : width(w), height(h) {
        // [DMA SIMULATION] Manually allocating memory buffer
        data = new Pixel[width * height]; 
    }

    // Copy Constructor (Deep Copy for double buffering)
    Image(const Image& other) : width(other.width), height(other.height) {
        data = new Pixel[width * height];
        for(int i=0; i<width*height; i++) data[i] = other.data[i];
    }

    // Destructor (Memory Cleanup)
    ~Image() {
        if (data) delete[] data;
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Read from memory address
    Pixel getPixel(int x, int y) const {
        // Boundary Check (Zero Padding for edges)
        if (x < 0 || x >= width || y < 0 || y >= height) return {0,0,0};
        
        // Linear Addressing: Map 2D coordinates to 1D memory address
        return data[y * width + x]; 
    }

    // Write to memory address
    void setPixel(int x, int y, Pixel p) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            data[y * width + x] = p;
        }
    }
};

// ============================================================
// MODULE 4: FILE I/O (Disk Operations)
// ============================================================
class IOHandler {
public:
    // Helper function to skip comments (lines starting with #) in PPM files
    static void ignoreComments(ifstream& file) {
        while (file >> ws && file.peek() == '#') {
            file.ignore(4096, '\n'); // Skip the entire comment line
        }
    }

    static Image* loadPPM(const string& filename) {
        Logger::log("DMA_READ", "Loading file: " + filename);
        ifstream file(filename);
        
        if (!file) {
            cerr << "[ERROR] File not found!" << endl;
            return nullptr;
        }

        string format;
        int w, h, maxVal;

        // Read PPM Header
        file >> format;
        if (format != "P3") {
            cerr << "[ERROR] Invalid Format. Please use PPM P3 (ASCII)." << endl;
            return nullptr;
        }

        ignoreComments(file); file >> w;
        ignoreComments(file); file >> h;
        ignoreComments(file); file >> maxVal;

        Logger::hardwareLog("Resolution detected: " + to_string(w) + "x" + to_string(h));
        
        Image* img = new Image(w, h);
        int r, g, b;
        
        // Load Pixel Data into RAM
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                file >> r >> g >> b;
                img->setPixel(x, y, {(uint8_t)r, (uint8_t)g, (uint8_t)b});
            }
        }
        return img;
    }

    static void savePPM(const Image* img, const string& filename) {
        ofstream file(filename);
        // Write Header (P3 format)
        file << "P3\n" << img->getWidth() << " " << img->getHeight() << "\n255\n";
        
        // Write Pixel Data
        for (int y = 0; y < img->getHeight(); y++) {
            for (int x = 0; x < img->getWidth(); x++) {
                Pixel p = img->getPixel(x, y);
                file << (int)p.r << " " << (int)p.g << " " << (int)p.b << " ";
            }
            file << "\n";
        }
        file.close();
    }
};

// ============================================================
// MODULE 5: FILTERS (Processing Cores)
// ============================================================

// Base Class (Polymorphism)
class Filter {
public:
    virtual string getName() = 0;
    virtual void apply(Image* src, Image* dest) = 0; // Pure Virtual Function
    virtual ~Filter() {}
};

// --- STAGE 1: GRAYSCALE CONVERTER ---
class GrayscaleFilter : public Filter {
public:
    string getName() override { return "Grayscale Converter"; }
    
    void apply(Image* src, Image* dest) override {
        for (int y = 0; y < src->getHeight(); y++) {
            for (int x = 0; x < src->getWidth(); x++) {
                Pixel p = src->getPixel(x, y);
                // Standard Formula: 0.3R + 0.59G + 0.11B
                // Hardware Optimization: Using integer multiplication and bit shift
                int gray = (p.r * 77 + p.g * 150 + p.b * 29) >> 8; 
                uint8_t val = HardwareMath::clamp(gray);
                dest->setPixel(x, y, {val, val, val});
            }
        }
    }
};

// --- STAGE 2: GAUSSIAN BLUR (3x3) ---
class BlurFilter : public Filter {
public:
    string getName() override { return "Gaussian Blur (3x3)"; }
    
    void apply(Image* src, Image* dest) override {
        // Gaussian Kernel for smoothing
        int kernel[3][3] = {{1,2,1}, {2,4,2}, {1,2,1}}; 
        int divisor = 16; 

        for (int y = 0; y < src->getHeight(); y++) {
            for (int x = 0; x < src->getWidth(); x++) {
                int sum = 0;
                // Convolution Loop
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        Pixel p = src->getPixel(x + kx, y + ky);
                        sum += p.r * kernel[ky+1][kx+1]; // Using Red channel as intensity
                    }
                }
                uint8_t val = HardwareMath::clamp(sum / divisor);
                dest->setPixel(x, y, {val, val, val});
            }
        }
    }
};

// --- STAGE 3: SOBEL EDGE DETECTION ---
class SobelFilter : public Filter {
public:
    string getName() override { return "Sobel Edge Detector"; }
    
    void apply(Image* src, Image* dest) override {
        // Vertical (Gx) and Horizontal (Gy) Kernels
        int gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
        int gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

        for (int y = 1; y < src->getHeight() - 1; y++) {
            for (int x = 1; x < src->getWidth() - 1; x++) {
                int sumX = 0, sumY = 0;

                // Convolution for Gradient Calculation
                for (int i = -1; i <= 1; i++) {
                    for (int j = -1; j <= 1; j++) {
                        int val = src->getPixel(x+j, y+i).r;
                        sumX += val * gx[i+1][j+1];
                        sumY += val * gy[i+1][j+1];
                    }
                }
                
                // Approximate Magnitude = |Gx| + |Gy|
                // This avoids square root (sqrt) which is expensive in hardware
                int mag = abs(sumX) + abs(sumY);
                uint8_t final = HardwareMath::clamp(mag);
                dest->setPixel(x, y, {final, final, final});
            }
        }
    }
};

// ============================================================
// MODULE 6: PIPELINE MANAGER
// ============================================================
class Pipeline {
    vector<Filter*> stages;
    Image* workingBuffer;

public:
    Pipeline(Image* input) {
        workingBuffer = new Image(*input); // Load input into pipeline memory
    }

    ~Pipeline() {
        if (workingBuffer) delete workingBuffer;
        for (auto f : stages) delete f;
    }

    void addStage(Filter* filter) {
        stages.push_back(filter);
    }

    // MAIN EXECUTION LOGIC
    void execute() {
        Logger::log("CONTROL", "Initializing Pipeline...");
        cout << "------------------------------------------------" << endl;
        
        // Secondary buffer for Double Buffering (Ping-Pong buffering)
        Image* backBuffer = new Image(workingBuffer->getWidth(), workingBuffer->getHeight());

        int step = 1;
        for (auto filter : stages) {
            Logger::log("EXECUTE", "Stage " + to_string(step) + ": " + filter->getName());
            
            // 1. Apply Hardware Logic
            filter->apply(workingBuffer, backBuffer);

            // 2. Swap Buffers (Move data to next stage)
            Image* temp = workingBuffer;
            workingBuffer = backBuffer;
            backBuffer = temp;
            
            // 3. Save Intermediate Output for Debugging
            string filename = "debug_stage_" + to_string(step) + ".ppm";
            IOHandler::savePPM(workingBuffer, filename);
            Logger::hardwareLog("Debug frame saved: " + filename);
            
            step++;
        }
        delete backBuffer;
        cout << "------------------------------------------------" << endl;
    }

    Image* getResult() { return workingBuffer; }
};

// ============================================================
// MAIN APPLICATION
// ============================================================
int main() {
    cout << "\n==============================================" << endl;
    cout << "   FPGA IMAGE PROCESSING SIMULATOR (CLI)" << endl;
    cout << "==============================================\n" << endl;

    string filename;
    Image* inputImg = nullptr;

    // User Input Loop
    while (true) {
        cout << "Enter input image filename (e.g., photo_ascii.ppm): ";
        cin >> filename;

        inputImg = IOHandler::loadPPM(filename);
        if (inputImg != nullptr) break;
        
        cout << "[ERROR] File not found or invalid format!" << endl;
        cout << "Hint: Ensure the file is PPM P3 (ASCII) format." << endl;
        cout << "Try again? (y/n): ";
        char choice; cin >> choice;
        if (choice == 'n') return 0;
    }

    // Pipeline Setup
    Pipeline fpgaPipe(inputImg);
    
    // Add Processing Modules
    fpgaPipe.addStage(new GrayscaleFilter());
    fpgaPipe.addStage(new BlurFilter());
    fpgaPipe.addStage(new SobelFilter());

    // Run Simulation
    fpgaPipe.execute();

    // Save Final Result
    IOHandler::savePPM(fpgaPipe.getResult(), "final_output.ppm");

    // Cleanup
    delete inputImg;
    
    cout << "\n[SUCCESS] Pipeline Execution Complete!" << endl;
    cout << "Check your folder for 'final_output.ppm' and debug files." << endl;

    return 0;
}
