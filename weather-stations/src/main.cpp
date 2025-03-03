#include <iostream>
#include <string>
#include <string_view>
#include <map>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <charconv>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Threadpool.hpp"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input file> <number of threads>" << std::endl;
        return 1;
    }
    const char* file = argv[1];

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
    char* fileData = static_cast<char*>(mmap(nullptr, filesize, PROT_READ,
                                               MAP_PRIVATE, fd, 0));
    close(fd);
    if (fileData == MAP_FAILED) {
        std::cerr << "Error mapping file" << std::endl;
        return 1;
    }

    std::string_view fileContent(fileData, filesize);

    unsigned int numThreads = std::stoi(argv[2]);
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Number of threads: " << numThreads << std::endl;

    // Determine chunk boundaries for each thread without copying lines.
    // Each chunk is defined by it's starting and ending index in the fileContent.
    std::vector<std::pair<size_t, size_t>> chunks;
    size_t approxChunkSize = filesize / (numThreads);
    size_t startOffset = 0;
    for (unsigned int tid = 0; tid < numThreads; ++tid) {
        size_t chunkStart = startOffset;
        size_t chunkEnd;
        if (tid == numThreads - 1) {
            chunkEnd = filesize;
        } else {
            chunkEnd = chunkStart + approxChunkSize;
            // verify that the chuck ends at a newline
            // size_t newlinePos = fileContent.find('\n', chunkEnd);
            // if (newlinePos != std::string_view::npos)
            //     chunkEnd = newlinePos;
            // else
            //     chunkEnd = filesize;

            while(fileContent[chunkEnd] != '\n' && chunkEnd < filesize)
            {
                chunkEnd++;
            }
        }
        chunks.emplace_back(chunkStart, chunkEnd);
        // Set next startOffset; if not at end, skip past the newline.
        startOffset = (chunkEnd < filesize) ? chunkEnd + 1 : chunkEnd;
    }

    std::vector<std::map<std::string_view, WSData>> localMaps(numThreads);

    // Lambda for processing a chunk of fileContent.
    auto worker = [&](size_t start, size_t end, unsigned int tid) {
        size_t pos = start;
        while (pos < end) {
            size_t next = fileContent.find('\n', pos);
            if (next == std::string_view::npos || next > end)
                next = end;
            if (pos < next) {
                std::string_view line = fileContent.substr(pos, next - pos);
                size_t delim = line.find(';');
                if (delim != std::string_view::npos) {
                    std::string_view stationName = line.substr(0, delim);
                    std::string_view tempStr = line.substr(delim + 1);
                    float temperature = 0.0f;
                    std::from_chars(tempStr.data(),
                                    tempStr.data() + tempStr.size(),
                                    temperature);
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
            pos = next + 1;
        }
    };

    // Enqueue tasks to the thread pool.
    Threadpool pool(numThreads);
    for (unsigned int tid = 0; tid < numThreads; tid++) {
        auto [chunkStart, chunkEnd] = chunks[tid];
        pool.addTask([=, &worker]() {
            worker(chunkStart, chunkEnd, tid);
        });
    }
    int tasks_left = pool.ReturnJobs();
    printf("Tasks left: %d\n", tasks_left);
    pool.wait();
    pool.stop();

    // Merging all workers local maps
    std::map<std::string_view, WSData> stations;
    for (const auto &localMap : localMaps) {
        for (const auto &pair : localMap) {
            std::string_view stationName = pair.first;
            const WSData &localData = pair.second;
            auto &globalData = stations[stationName];
            if (globalData.count == 0)
                globalData = localData;
            else {
                globalData.sum += localData.sum;
                globalData.count += localData.count;
                if (localData.min < globalData.min)
                    globalData.min = localData.min;
                if (localData.max > globalData.max)
                    globalData.max = localData.max;
            }
        }
    }

    for (auto &pair : stations) {
        WSData &data = pair.second;
        data.average = data.sum / data.count;
    }

    // Sort results based on maximum temperature (descending).
    std::vector<WSData> sortedStations;
    for (const auto &pair : stations)
        sortedStations.push_back(pair.second);
    std::sort(sortedStations.begin(), sortedStations.end(),
              [](const WSData &a, const WSData &b) {
                  return a.max > b.max;
              });

    for (const auto &data : sortedStations) {
        std::cout << data.name << ": avg=" << data.average
                  << " min=" << data.min
                  << " max=" << data.max << std::endl;
    }

    munmap(fileData, filesize);

    return 0;
}