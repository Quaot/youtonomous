#include "session.h"

#include <algorithm>
#include <cctype>

static std::string lowerCase(const std::string& text)
{
    std::string lower;
    for (char c : text)
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower;
}

std::vector<Block> blocksOf(const Video& video)
{
    std::vector<Block> blocks;
    if (video.duration <= 0)
        return blocks;

    std::vector<Bookmark> chapters;
    for (const Bookmark& mark : video.marks) {
        if (mark.chapter)
            chapters.push_back(mark);
    }
    std::stable_sort(chapters.begin(), chapters.end(),
                     [](const Bookmark& a, const Bookmark& b) { return a.time < b.time; });

    for (size_t i = 0; i < chapters.size(); ++i) {
        std::string label = lowerCase(chapters[i].label);
        if (label.find("intro") != std::string::npos || label.find("outro") != std::string::npos)
            continue;

        int start = chapters[i].time;
        int end = i + 1 < chapters.size() ? chapters[i + 1].time : video.duration;
        if (end <= start)
            continue;
        blocks.push_back({chapters[i].label, start, end, label.find("break") != std::string::npos});
    }

    while (!blocks.empty() && blocks.back().rest)
        blocks.pop_back();
    return blocks;
}

int blockAt(const std::vector<Block>& blocks, int position)
{
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (position >= blocks[i].start && position < blocks[i].end)
            return static_cast<int>(i);
    }
    return -1;
}

SessionAdvice advance(SessionState& state, const std::vector<Block>& blocks, int position, double seconds, bool playing, bool loop)
{
    SessionAdvice advice;
    if (blocks.empty())
        return advice;

    int count = static_cast<int>(blocks.size());
    if (state.block < 0 || state.block >= count) {
        state.block = blockAt(blocks, position);
        state.elapsed = 0;
    }
    if (state.block < 0 || !playing)
        return advice;

    state.elapsed += std::max(0.0, seconds);

    const Block& block = blocks[state.block];
    double length = block.end - block.start;
    if (state.elapsed >= length) {
        advice.block_done = true;
        if (state.resume_block >= 0 && state.resume_block < count) {
            state.block = state.resume_block;
            state.elapsed = state.resume_elapsed;
            advice.seek_to = state.resume_position;
            state.resume_block = -1;
            state.resume_elapsed = 0;
            state.resume_position = 0;
            return advice;
        }
        state.elapsed = 0;
        if (state.block + 1 < count)
            state.block = state.block + 1;
        else
            state.block = loop ? loopTarget(blocks) : -1;
        if (state.block >= 0)
            advice.seek_to = blocks[state.block].start;
        return advice;
    }

    if (position >= block.end - 1 && length - state.elapsed > 1)
        advice.seek_to = block.start;
    return advice;
}

SessionAdvice skip(SessionState& state, const std::vector<Block>& blocks, int position, bool bank)
{
    SessionAdvice advice;
    int count = static_cast<int>(blocks.size());
    bool inside = state.block >= 0 && state.block < count;
    bool working = inside && !blocks[state.block].rest;

    if (inside && !working) {
        if (state.resume_block >= 0 && state.resume_block < count) {
            state.block = state.resume_block;
            state.elapsed = state.resume_elapsed;
            advice.seek_to = state.resume_position;
            state.resume_block = -1;
            state.resume_elapsed = 0;
            state.resume_position = 0;
            return advice;
        }

        for (int i = 0; i < count; ++i) {
            if (!blocks[i].rest && blocks[i].start > position) {
                state.block = i;
                state.elapsed = 0;
                advice.seek_to = blocks[i].start;
                return advice;
            }
        }
        return advice;
    }

    int target = -1;
    for (int i = 0; i < count; ++i) {
        if (blocks[i].rest && blocks[i].start > position) {
            target = i;
            break;
        }
    }
    if (target < 0)
        return advice;

    if (bank && working) {
        state.resume_block = state.block;
        state.resume_elapsed = state.elapsed;
        state.resume_position = position;
    } else {
        advice.block_done = working;
        state.resume_block = -1;
        state.resume_elapsed = 0;
        state.resume_position = 0;
    }

    state.block = target;
    state.elapsed = 0;
    advice.seek_to = blocks[target].start;
    return advice;
}

int remaining(const SessionState& state, const std::vector<Block>& blocks)
{
    if (state.block < 0 || state.block >= static_cast<int>(blocks.size()))
        return 0;

    const Block& block = blocks[state.block];
    double left = (block.end - block.start) - state.elapsed;
    return left > 0 ? static_cast<int>(left) : 0;
}

int sessionVolume(const SessionState& state, const std::vector<Block>& blocks, bool silence_work, int wanted)
{
    if (!silence_work)
        return wanted;
    if (state.block < 0 || state.block >= static_cast<int>(blocks.size()))
        return wanted;
    return blocks[state.block].rest ? wanted : 0;
}

int loopTarget(const std::vector<Block>& blocks)
{
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].rest)
            return static_cast<int>(i);
    }
    return blocks.empty() ? -1 : 0;
}

int fadeStep(int current, int target, int step)
{
    if (step <= 0)
        return target;
    if (current < target)
        return std::min(target, current + step);
    return std::max(target, current - step);
}
