#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <complex>
#include <thread>
#include <mutex>
#include <typeinfo>

// Settings
int imageWidth = 1920;
int imageHeight = 1080;

int maxIterations = 255;
int numThreads = 1;
int granLevel = 1;
double granularity = ceil(double(imageHeight) / double(numThreads) / double(granLevel));

//  For default mandelbrot z = z * z + c
// double minR = -1.5;
// double maxR = 0.7;
// double minI = -1.0;
// double maxI = 1.0;

double minR = -2.5;
double maxR = 1.0;
double minI = -2.0;
double maxI = 2.0;

// Structures
struct DrawPixel {
  int x, y;
  int red, green, blue;
};

struct Task {
  bool finished;
  int assignedThread;
  int yStart, yEnd;
};

// Global variables, shared between threads
std::mutex mtx;
std::vector<DrawPixel> collected_data;
std::vector<Task> thread_tasks;

// Functions
// Custom sort function for the structure DrawPixel
// Sorting by line number because we draw line by line
bool CustomSort(DrawPixel a, DrawPixel b) {
  if (a.y == b.y) {
    return a.x < b.x;
  }
  else {
    return a.y < b.y;
  }
}

// Convert pixel coordinates to complex number
double ConvertXToReal(int x, int imageWidth, double minR, double maxR) {
  double range = double(x) / double(imageWidth);
  return minR + (maxR - minR) * range;
}

double ConvertYToImaginary(int y, int imageHeight, double minI, double maxI) {
  double range = double(y) / double(imageHeight);
  return minI + (maxI - minI) * range;
}

// Check if the pixel is part of the Mandelbrot set
// Each thread takes its own tasks from the shared memory -> thread_tasks
void FindMandelbrot(int threadNum, int maxIterations) {
  std::vector<Task>::iterator task;
  while ((task = std::find_if(thread_tasks.begin(), thread_tasks.end(), [threadNum](const Task& currTask) { return currTask.assignedThread == threadNum && !currTask.finished; })) != thread_tasks.end()) {
    for (int i = task->yStart; i < task->yEnd; ++i) {
      for (int x = 0; x < imageWidth; ++x) {
        std::complex<double> z(0.0, 0.0);
        double complexReal = ConvertXToReal(x, imageWidth, minR, maxR);
        double complexImaginary = ConvertYToImaginary(i, imageHeight, minI, maxI);
        std::complex<double> c(complexReal, complexImaginary);

        int iter = 0;
        while (iter < maxIterations) {
          // z = z * z + c;
          std::complex<double> expResult = exp(-z);
          std::complex<double> powResult = pow(z, 2);
          z = c * expResult + powResult;
          ++iter;

          if (abs(z) > 2) {
            break;
          }
        }

        // Define the color
        DrawPixel newPixel;
        newPixel.x = x;
        newPixel.y = i;
        newPixel.red = (iter % 256) * 3;
        newPixel.green = (iter % 256) * 15;
        newPixel.blue = (iter % 256) * 17;

        mtx.lock();
        collected_data.push_back(newPixel);
        mtx.unlock();
      }
    }
    // When the task is finished, remove it from the vector of tasks
    task->finished = true;
  }
}

int main() {
  // Prepare the tasks
  Task task;
  int j = 0;
  int controlPoint = imageHeight / numThreads;

  for (int i = 0; i < numThreads; ++i) {
    for (int g = 0; g < imageHeight / numThreads; g += granularity) {
      task.finished = false;
      task.assignedThread = i;
      task.yStart = j;

      if (i == numThreads - 1 && g + granularity >= imageHeight / numThreads) {
        j = imageHeight;
      }
      else if (g + granularity >= imageHeight / numThreads) {
        j = controlPoint;
        controlPoint += imageHeight / numThreads;
      }
      else {
        j += granularity;
      }

      task.yEnd = j;
      thread_tasks.push_back(task);
    }
  }

  // Start the threads
  std::vector<std::thread> started_threads;

  // Start measuring time
  struct timeval begin, end;
  gettimeofday(&begin, 0);

  for (int y = 0; y < numThreads; ++y) {
    std::thread current_thread(FindMandelbrot, y, maxIterations);
    started_threads.push_back(std::move(current_thread));
  }

  // Wait for the threads to finish
  for (auto& current_thread: started_threads) {
    current_thread.join();
  }

  gettimeofday(&end, 0);
  long seconds = end.tv_sec - begin.tv_sec;
  long microseconds = end.tv_usec - begin.tv_usec;
  double elapsed = (seconds + microseconds*1e-6) * 1e+3;
  std::cout << "Time measured in milliseconds: " << int(elapsed) << std::endl;

  // Sort the collected data by line number
  std::sort(collected_data.begin(), collected_data.end(), CustomSort);
  std::vector<DrawPixel>::iterator it;

  // Draw the picture
  std::ofstream fout("mandelbrot.ppm");
  fout << "P3" << std::endl;
  fout << imageWidth << " " << imageHeight << std::endl;
  fout << "256" << std::endl;

  for (it = collected_data.begin(); it != collected_data.end(); ++it) {
    fout << it->red << " " << it->green << " " << it->blue << " ";

    if ((it + 1) != collected_data.end()) {
      if (it->y != (it + 1)->y) {
        fout << std::endl;
      }
    }
  }
  fout << std::endl;
  fout.close();

  return 0;
}
