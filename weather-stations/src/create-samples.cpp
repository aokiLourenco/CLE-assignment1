
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <random>
#include <sstream>

#include "cities.cpp"

/* 
DATA FORMAT
Input value ranges are as follows:
    - Station name: non null UTF-8 string of min length 1 character and max length 100 bytes, containing neither ; nor \n characters. (i.e. this could be 100 one-byte characters, or 50 two-byte characters, etc.)
    - Temperature value: non null double between -99.9 (inclusive) and 99.9 (inclusive), always with one fractional digit

    - There is a maximum of 10,000 unique station names
    - Line endings in the file are \n characters on all platforms
*/

static std::random_device rnd;
static std::mt19937 gen(rnd());

double rand_normal(double mean, double stddev)
{
    std::normal_distribution<double> dist(mean, stddev);
    return dist(gen);
}

int rand_uniform(int min, int max)
{
    std::uniform_int_distribution dist(min, max);
    return dist(gen);
}

int main(int argc, char* argv[])
{
    // 1. Number of samples to generate as argument
    if (argc < 2) {
        std::cerr << "Missing argument: create-sample NUM_SAMPLES";
        return 1;
    }

    int nsamples = atoi(argv[1]);
    // number of cities to samples from
    int ncities = sizeof(data)/sizeof(data[0]);

    // 2. Create the file
    std::stringstream ss; ss << "samples-" << nsamples << ".txt";
    std::string fname = ss.str();

    int fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    std::cout << "Generating " << nsamples << " samples ...";
    for (int i = 0; i < nsamples; ++i){

        int select = rand_uniform(0, ncities-1);
        std::stringstream ss;
        ss << data[select].city << ";"
           << rand_normal(data[select].mean, 4) << std::endl;

        std::string text = ss.str();
        size_t ret = write(fd, text.c_str(), text.size());
        if (ret != text.size()){
            std::cerr << "Unable to write to file." << std::endl;
            return 1;
        }

    }// end for

    // Done
    std::cout << " Done" << std::endl;

    return 0;
}
