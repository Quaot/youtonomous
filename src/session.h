#pragma once

#include <string>
#include <vector>

#include "library.h"

struct Block {
    std::string label;
    int start = 0;
    int end = 0;
    bool rest = false;
};

struct SessionState {
    int block = -1;
    double elapsed = 0;
};

struct SessionAdvice {
    int seek_to = -1;
    bool block_done = false;
};

std::vector<Block> blocksOf(const Video& video);
int blockAt(const std::vector<Block>& blocks, int position);
SessionAdvice advance(SessionState& state, const std::vector<Block>& blocks, int position, double seconds, bool playing);
int remaining(const SessionState& state, const std::vector<Block>& blocks);
