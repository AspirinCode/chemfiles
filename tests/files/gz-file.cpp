// Chemfiles, a modern library for chemistry file reading and writing
// Copyright (C) Guillaume Fraux and contributors -- BSD license

#include "catch.hpp"
#include "helpers.hpp"
#include "chemfiles/files/GzFile.hpp"
#include <fstream>
using namespace chemfiles;

static void check_file(GzFile& file) {
    std::string line = file.readline();
    CHECK(line == "297");

    auto lines = file.readlines(42);
    REQUIRE(lines.size() == 42);
    CHECK(lines[0] == " generated by VMD");
    CHECK(lines[1] == "  O          0.417219        8.303366       11.737172");

    file.rewind();
    line = file.readline();
    CHECK(line == "297");
    line = file.readline();
    CHECK(line == " generated by VMD");
}

TEST_CASE("Read a text file") {
    GzFile file_6("data/xyz/water.6.xyz.gz", File::READ);
    check_file(file_6);

    GzFile file_9("data/xyz/water.9.xyz.gz", File::READ);
    check_file(file_9);
}


TEST_CASE("Write a gz file") {
    auto filename = NamedTempPath(".gz");

    {
        GzFile file(filename, File::WRITE);
        file << "Test" << std::endl;
        file << 5467 << std::endl;
    }

    // GZip's header is OS dependant, so let's decompress and compare
    GzFile file(filename, File::READ);
    CHECK(file.readline() == "Test");
    CHECK(file.readline() == "5467");
}