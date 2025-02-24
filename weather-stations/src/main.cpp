#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <map>
#include <limits>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Threadpool.hpp"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input file> <number of threads>" << std::endl;
        return 1;
    }
    const char* file = argv[1];
    
    // Open file using OS-level calls and mmap it.
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Unable to open '" << file << "'" << std::endl;
        return 1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Error getting file size" << std::endl;
        close(fd);
        return 1;
    }
    size_t filesize = sb.st_size;
    char* fileData = static_cast<char*>(mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (fileData == MAP_FAILED) {
        std::cerr << "Error mapping file" << std::endl;
        return 1;
    }
    
    // Create a string view of the entire file and then split into lines.
    std::string_view fileContent(fileData, filesize);
    std::vector<std::string_view> lines;
    size_t pos = 0;
    while (pos < fileContent.size()) {
        size_t next = fileContent.find('\n', pos);
        if (next == std::string_view::npos) {
            lines.push_back(fileContent.substr(pos));
            break;
        } else {
            lines.push_back(fileContent.substr(pos, next - pos));
            pos = next + 1;
        }
    }
    // Not unmapping until we've finished processing
    // (You can later call munmap(fileData, filesize) before return.)

    std::cout << std::fixed << std::setprecision(1);
    unsigned int numThreads = std::stoi(argv[2]);
    std::cout << "Number of threads: " << numThreads << std::endl;

    // Prepare local maps, one per task chunk.
    std::vector<std::map<std::string, WSData>> localMaps(numThreads);

    // Lambda function for processing a chunk of lines.
    auto worker = [&](size_t start, size_t end, unsigned int tid) {
        for (size_t i = start; i < end; i++) {
            std::string_view line = lines[i];
            size_t delim = line.find(';');
            if (delim != std::string_view::npos) {
                // Extract stationName and temp as string_views.
                std::string stationName(line.substr(0, delim));
                std::string tempStr(std::string(line.substr(delim + 1)));
                float temperature = std::stof(tempStr);
                auto& data = localMaps[tid][stationName];
                if (data.count == 0) {
                    data.name = stationName;
                    data.sum = temperature;
                    data.count = 1;
                    data.min = temperature;
                    data.max = temperature;
                } else {
                    data.sum += temperature;
                    data.count++;
                    if (temperature < data.min)
                        data.min = temperature;
                    if (temperature > data.max)
                        data.max = temperature;
                }
            }
        }
    };

    // Divide work into chunks and enqueue tasks in the thread pool.
    Threadpool pool(numThreads);
    size_t totalLines = lines.size();
    size_t chunkSize = totalLines / numThreads;
    size_t remainder = totalLines % numThreads;
    size_t startIdx = 0;
    for (unsigned int tid = 0; tid < numThreads; tid++) {
        size_t extra = (tid < remainder) ? 1 : 0;
        size_t endIdx = startIdx + chunkSize + extra;
        pool.addTask([=, &localMaps, &lines, &worker]() {
            worker(startIdx, endIdx, tid);
        });
        startIdx = endIdx;
    }
    // Wait until all tasks finish.
    pool.wait();
    pool.stop();

    // Merge local maps into a global map.
    std::map<std::string, WSData> stations;
    for (const auto &localMap : localMaps) {
        for (const auto &pair : localMap) {
            const std::string &stationName = pair.first;
            const WSData &localData = pair.second;
            auto &globalData = stations[stationName];
            if (globalData.count == 0) {
                globalData = localData;
            } else {
                globalData.sum += localData.sum;
                globalData.count += localData.count;
                if (localData.min < globalData.min)
                    globalData.min = localData.min;
                if (localData.max > globalData.max)
                    globalData.max = localData.max;
            }
        }
    }

    // Compute averages.
    for (auto &pair : stations) {
        WSData &data = pair.second;
        data.average = data.sum / data.count;
    }

    // Sort results based on maximum temperature (descending).
    std::vector<WSData> sortedStations;
    for (const auto &pair : stations) {
        sortedStations.push_back(pair.second);
    }
    std::sort(sortedStations.begin(), sortedStations.end(),
              [](const WSData &a, const WSData &b) {
                  return a.max > b.max;
              });

    for (const auto &data : sortedStations) {
        std::cout << data.name << ": avg=" << data.average 
                  << " min=" << data.min 
                  << " max=" << data.max << std::endl;
    }

    // Unmap file memory.
    munmap(fileData, filesize);

    return 0;
}