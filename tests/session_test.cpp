#include <string>
#include <vector>

#include "check.h"
#include "session.h"

static Bookmark mark(int time, const std::string& label, bool chapter)
{
    Bookmark result;
    result.time = time;
    result.label = label;
    result.chapter = chapter;
    return result;
}

static Video videoWith(int duration, const std::vector<Bookmark>& marks)
{
    Video video;
    video.id = "study1";
    video.url = "https://www.youtube.com/watch?v=study1";
    video.title = "Study With Me";
    video.file = "C:\\Videos\\Youtonomous\\study1.mp4";
    video.duration = duration;
    video.marks = marks;
    return video;
}

static Block block(const std::string& label, int start, int end, bool rest)
{
    Block result;
    result.label = label;
    result.start = start;
    result.end = end;
    result.rest = rest;
    return result;
}

static std::string describe(const std::vector<Block>& blocks)
{
    std::string result;
    for (const Block& b : blocks)
        result += b.label + "[" + std::to_string(b.start) + "," + std::to_string(b.end) + ")" + (b.rest ? "rest " : "work ");
    return result.empty() ? std::string("(no blocks)") : result;
}

static std::string describeState(const SessionState& state)
{
    return "block " + std::to_string(state.block) + ", elapsed " + std::to_string(state.elapsed);
}

static std::string describeAdvice(const SessionAdvice& advice)
{
    return "seek_to " + std::to_string(advice.seek_to) + ", block_done " + (advice.block_done ? "true" : "false");
}

static bool sameTime(double a, double b)
{
    return a - b < 0.000001 && b - a < 0.000001;
}

static void checkBlock(const std::vector<Block>& blocks, size_t index, const std::string& label, int start, int end, bool rest, const std::string& name)
{
    std::string wanted = label + "[" + std::to_string(start) + "," + std::to_string(end) + ")" + (rest ? "rest" : "work");
    if (index >= blocks.size()) {
        check(false, name + ": block " + std::to_string(index) + " is " + wanted + ", got " + describe(blocks));
        return;
    }
    const Block& got = blocks[index];
    check(got.label == label && got.start == start && got.end == end && got.rest == rest,
          name + ": block " + std::to_string(index) + " is " + wanted + ", got " + describe(blocks));
}

static void checkEmptyAdvice(const SessionAdvice& advice, const std::string& name)
{
    check(advice.seek_to == -1 && !advice.block_done, name + ", got " + describeAdvice(advice));
}

static void checkState(const SessionState& state, int expected_block, double expected_elapsed, const std::string& name)
{
    check(state.block == expected_block && sameTime(state.elapsed, expected_elapsed),
          name + ": state is block " + std::to_string(expected_block) + ", elapsed " + std::to_string(expected_elapsed) + ", got " + describeState(state));
}

static std::vector<Block> threeBlocks()
{
    return {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true), block("Pomodoro 2", 100, 160, false)};
}

// Plays through one block, feeding real time in ticks and moving playback at the given rate.
// Follows every seek the session advises. Returns real seconds spent and loops taken.
struct RunResult {
    double real_seconds = 0;
    int loops = 0;
    bool finished = false;
    int finish_seek = -1;
    bool loop_with_done = false;
    double shortest_loop_owed = 1e9;
};

static RunResult playBlock(SessionState& state, const std::vector<Block>& blocks, int index, double rate, double tick)
{
    RunResult result;
    double position = blocks[index].start;
    double length = blocks[index].end - blocks[index].start;
    for (int i = 0; i < 100000; ++i) {
        double owed = length - state.elapsed;
        SessionAdvice advice = advance(state, blocks, static_cast<int>(position), tick, true);
        result.real_seconds += tick;
        if (advice.block_done) {
            result.finished = true;
            result.finish_seek = advice.seek_to;
            break;
        }
        if (advice.seek_to >= 0) {
            ++result.loops;
            if (owed < result.shortest_loop_owed)
                result.shortest_loop_owed = owed;
            position = advice.seek_to;
        } else {
            position += rate * tick;
            if (position > blocks[index].end)
                position = blocks[index].end;
        }
    }
    return result;
}

static void testPomodoroBlocks()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(0, "Pomodoro 1", true), mark(3000, "Guided Break 1", true),
                                                          mark(3600, "Pomodoro 2", true), mark(6600, "Guided Break 2", true)}));
    check(blocks.size() == 4, "a 50/10 Pomodoro video gives 4 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 3000, false, "50/10 Pomodoro");
    checkBlock(blocks, 1, "Guided Break 1", 3000, 3600, true, "50/10 Pomodoro");
    checkBlock(blocks, 2, "Pomodoro 2", 3600, 6600, false, "50/10 Pomodoro");
    checkBlock(blocks, 3, "Guided Break 2", 6600, 7200, true, "50/10 Pomodoro");
}

static void testNoBlocks()
{
    std::vector<Block> none = blocksOf(videoWith(3600, {}));
    check(none.empty(), "a video without marks gives no blocks, got " + describe(none));

    std::vector<Block> zero = blocksOf(videoWith(0, {mark(0, "Pomodoro 1", true), mark(600, "Pomodoro 2", true)}));
    check(zero.empty(), "duration 0 gives no blocks, got " + describe(zero));

    std::vector<Block> negative = blocksOf(videoWith(-1200, {mark(0, "Pomodoro 1", true), mark(600, "Pomodoro 2", true)}));
    check(negative.empty(), "a negative duration gives no blocks, got " + describe(negative));

    std::vector<Block> nothing = blocksOf(videoWith(0, {}));
    check(nothing.empty(), "duration 0 and no marks gives no blocks, got " + describe(nothing));
}

static void testNonChapterMarksIgnored()
{
    std::vector<Block> user_only = blocksOf(videoWith(3600, {mark(0, "Start here", false), mark(900, "Hard bit", false)}));
    check(user_only.empty(), "user marks alone give no blocks, got " + describe(user_only));

    std::vector<Block> mixed = blocksOf(videoWith(3000, {mark(0, "Pomodoro 1", true), mark(300, "Nice break idea", false),
                                                         mark(900, "Come back here", false), mark(1500, "Guided Break 1", true),
                                                         mark(2000, "Lovely music", false)}));
    check(mixed.size() == 2, "user marks between chapters do not make blocks, got " + describe(mixed));
    checkBlock(mixed, 0, "Pomodoro 1", 0, 1500, false, "user marks mixed in");
    checkBlock(mixed, 1, "Guided Break 1", 1500, 3000, true, "user marks mixed in");

    std::vector<Block> non_chapter_intro = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(600, "Intro thought", false)}));
    check(non_chapter_intro.size() == 1, "a non-chapter intro mark changes nothing, got " + describe(non_chapter_intro));
    checkBlock(non_chapter_intro, 0, "Pomodoro 1", 0, 1200, false, "non-chapter intro mark");
}

static void testChaptersOutOfOrder()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(6600, "Guided Break 2", true), mark(0, "Pomodoro 1", true),
                                                          mark(3600, "Pomodoro 2", true), mark(3000, "Guided Break 1", true)}));
    check(blocks.size() == 4, "unsorted chapters still give 4 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 3000, false, "unsorted chapters");
    checkBlock(blocks, 1, "Guided Break 1", 3000, 3600, true, "unsorted chapters");
    checkBlock(blocks, 2, "Pomodoro 2", 3600, 6600, false, "unsorted chapters");
    checkBlock(blocks, 3, "Guided Break 2", 6600, 7200, true, "unsorted chapters");
}

static void testZeroLengthAndInvertedBlocks()
{
    std::vector<Block> duplicate = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(600, "Stray", true),
                                                             mark(600, "Pomodoro 2", true)}));
    check(duplicate.size() == 2, "two chapters at the same time drop the zero-length block, got " + describe(duplicate));
    checkBlock(duplicate, 0, "Pomodoro 1", 0, 600, false, "zero-length block");

    std::vector<Block> at_duration = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(1200, "Pomodoro 2", true)}));
    check(at_duration.size() == 1, "a chapter at the duration gives a zero-length block that is dropped, got " + describe(at_duration));
    checkBlock(at_duration, 0, "Pomodoro 1", 0, 1200, false, "chapter at duration");

    std::vector<Block> past_duration = blocksOf(videoWith(1000, {mark(0, "Pomodoro 1", true), mark(1200, "Pomodoro 2", true)}));
    check(past_duration.size() == 1, "a chapter past the duration gives an inverted block that is dropped, got " + describe(past_duration));
    checkBlock(past_duration, 0, "Pomodoro 1", 0, 1200, false, "chapter past duration");

    std::vector<Block> only_at_duration = blocksOf(videoWith(1200, {mark(1200, "Pomodoro 1", true)}));
    check(only_at_duration.empty(), "a lone chapter at the duration gives no blocks, got " + describe(only_at_duration));
}

static void testRestLabels()
{
    std::vector<Block> blocks = blocksOf(videoWith(600, {mark(0, "Break 1", true), mark(100, "Guided Break 2", true),
                                                         mark(200, "SHORT BREAK", true), mark(300, "bReAk time", true),
                                                         mark(400, "Deep Work", true), mark(500, "Breakfast review", true)}));
    check(blocks.size() == 6, "all six chapters give blocks, got " + describe(blocks));
    check(blocks.size() == 6 && blocks[0].rest, "\"Break 1\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 6 && blocks[1].rest, "\"Guided Break 2\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 6 && blocks[2].rest, "\"SHORT BREAK\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 6 && blocks[3].rest, "\"bReAk time\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 6 && !blocks[4].rest, "\"Deep Work\" is not a rest block, got " + describe(blocks));
    check(blocks.size() == 6 && blocks[5].rest, "\"Breakfast review\" contains break so it is a rest block, got " + describe(blocks));
}

static void testFillerAtStart()
{
    std::vector<Block> blocks = blocksOf(videoWith(1200, {mark(0, "Intro", true), mark(60, "Pomodoro 1", true)}));
    check(blocks.size() == 1, "an intro at the start is not a block, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 60, 1200, false, "filler at start");
}

static void testFillerAtEnd()
{
    std::vector<Block> blocks = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(1100, "Outro", true)}));
    check(blocks.size() == 1, "an outro at the end is not a block, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 1100, false, "filler at end");
}

static void testFillerInTheMiddle()
{
    std::vector<Block> blocks = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(600, "Intro to part two", true),
                                                          mark(700, "Pomodoro 2", true)}));
    check(blocks.size() == 2, "filler in the middle is not a block, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 600, false, "filler in the middle");
    checkBlock(blocks, 1, "Pomodoro 2", 700, 1200, false, "filler in the middle");
}

static void testFillerEverywhere()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(0, "Intro", true), mark(67, "Pomodoro 1", true),
                                                          mark(3067, "Break 1", true), mark(3667, "Outro", true)}));
    check(blocks.size() == 2, "Intro/Pomodoro 1/Break 1/Outro gives exactly two blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 67, 3067, false, "filler at both ends");
    checkBlock(blocks, 1, "Break 1", 3067, 3667, true, "filler at both ends");

    std::vector<Block> all_filler = blocksOf(videoWith(1200, {mark(0, "Intro", true), mark(600, "Outro", true)}));
    check(all_filler.empty(), "a video of nothing but filler gives no blocks, got " + describe(all_filler));

    std::vector<Block> one_filler = blocksOf(videoWith(1200, {mark(0, "INTRODUCTION", true)}));
    check(one_filler.empty(), "a lone filler chapter gives no blocks, got " + describe(one_filler));
}

static void testFillerCaseVariations()
{
    std::vector<Block> blocks = blocksOf(videoWith(700, {mark(0, "Pomodoro 1", true), mark(100, "INTRO", true),
                                                         mark(200, "intro", true), mark(300, "An Intro Chat", true),
                                                         mark(400, "OUTRO", true), mark(500, "outro and goodbye", true),
                                                         mark(600, "Pomodoro 2", true)}));
    check(blocks.size() == 2, "every case of intro and outro is filler, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 100, false, "filler case variations");
    checkBlock(blocks, 1, "Pomodoro 2", 600, 700, false, "filler case variations");
}

static void testFillerDoesNotStretchNeighbours()
{
    std::vector<Block> blocks = blocksOf(videoWith(1000, {mark(0, "Pomodoro 1", true), mark(300, "Outro of part one", true),
                                                          mark(500, "Pomodoro 2", true), mark(800, "Intro to the end", true)}));
    check(blocks.size() == 2, "removed filler leaves gaps, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 300, false, "no stretching over filler");
    checkBlock(blocks, 1, "Pomodoro 2", 500, 800, false, "no stretching over filler");
}

static void testRestAndFillerTogether()
{
    std::vector<Block> blocks = blocksOf(videoWith(900, {mark(0, "Intro break chat", true), mark(100, "Guided Break 1", true),
                                                         mark(500, "Pomodoro 1", true)}));
    check(blocks.size() == 2, "a label with both intro and break is filler, got " + describe(blocks));
    checkBlock(blocks, 0, "Guided Break 1", 100, 500, true, "rest and filler together");
    checkBlock(blocks, 1, "Pomodoro 1", 500, 900, false, "rest and filler together");
}

static void testBlockAt()
{
    std::vector<Block> blocks = threeBlocks();
    check(blockAt(blocks, 0) == 0, "position 0 is in block 0, got " + std::to_string(blockAt(blocks, 0)));
    check(blockAt(blocks, 30) == 0, "position 30 is in block 0, got " + std::to_string(blockAt(blocks, 30)));
    check(blockAt(blocks, 59) == 0, "position 59 is in block 0, got " + std::to_string(blockAt(blocks, 59)));
    check(blockAt(blocks, 60) == 1, "the end of a block belongs to the next one, got " + std::to_string(blockAt(blocks, 60)));
    check(blockAt(blocks, 99) == 1, "position 99 is in block 1, got " + std::to_string(blockAt(blocks, 99)));
    check(blockAt(blocks, 100) == 2, "position 100 is in block 2, got " + std::to_string(blockAt(blocks, 100)));
    check(blockAt(blocks, 159) == 2, "position 159 is in block 2, got " + std::to_string(blockAt(blocks, 159)));
    check(blockAt(blocks, 160) == -1, "the end of the last block is in no block, got " + std::to_string(blockAt(blocks, 160)));
    check(blockAt(blocks, 500) == -1, "a position past the end is in no block, got " + std::to_string(blockAt(blocks, 500)));
    check(blockAt(blocks, -5) == -1, "a negative position is in no block, got " + std::to_string(blockAt(blocks, -5)));

    std::vector<Block> empty;
    check(blockAt(empty, 0) == -1, "no blocks means no block at 0, got " + std::to_string(blockAt(empty, 0)));

    std::vector<Block> offset = {block("Pomodoro 1", 67, 3067, false)};
    check(blockAt(offset, 0) == -1, "a position before the first block is in no block, got " + std::to_string(blockAt(offset, 0)));
    check(blockAt(offset, 66) == -1, "position 66 is before the first block, got " + std::to_string(blockAt(offset, 66)));
    check(blockAt(offset, 67) == 0, "position 67 is the first block's start, got " + std::to_string(blockAt(offset, 67)));

    std::vector<Block> gapped = {block("Pomodoro 1", 0, 300, false), block("Pomodoro 2", 500, 800, false)};
    check(blockAt(gapped, 300) == -1, "a position in removed filler is in no block, got " + std::to_string(blockAt(gapped, 300)));
    check(blockAt(gapped, 400) == -1, "a position in the middle of a gap is in no block, got " + std::to_string(blockAt(gapped, 400)));
    check(blockAt(gapped, 499) == -1, "position 499 is still in the gap, got " + std::to_string(blockAt(gapped, 499)));
    check(blockAt(gapped, 500) == 1, "position 500 starts the second block, got " + std::to_string(blockAt(gapped, 500)));
}

static void testNoBlocksAdvance()
{
    std::vector<Block> none;
    SessionState state;
    SessionAdvice advice = advance(state, none, 30, 0.25, true);
    checkEmptyAdvice(advice, "advance with no blocks gives empty advice");
    checkState(state, -1, 0, "advance with no blocks leaves the state alone");

    SessionState running;
    running.block = 2;
    running.elapsed = 12.5;
    SessionAdvice again = advance(running, none, 30, 0.25, true);
    checkEmptyAdvice(again, "advance with no blocks never advises anything");
    checkState(running, 2, 12.5, "advance with no blocks does not touch a running state");
}

static void testAdoptBlock()
{
    std::vector<Block> blocks = threeBlocks();
    SessionState state;
    SessionAdvice advice = advance(state, blocks, 120, 0.25, true);
    check(state.block == 2, "an initial -1 block is re-derived from the position, got " + describeState(state));
    checkEmptyAdvice(advice, "re-deriving the block advises nothing");

    SessionState wrong;
    wrong.block = 7;
    wrong.elapsed = 40;
    advance(wrong, blocks, 70, 0.25, true);
    check(wrong.block == 1, "a block index past the end is re-derived, got " + describeState(wrong));
    check(sameTime(wrong.elapsed, 0.25), "re-deriving resets elapsed before the tick is added, got " + describeState(wrong));

    SessionState negative_index;
    negative_index.block = -4;
    negative_index.elapsed = 9;
    advance(negative_index, blocks, 10, 0.25, true);
    check(negative_index.block == 0, "a negative block index is re-derived, got " + describeState(negative_index));
}

static void testLatchesOnlyInsideABlock()
{
    std::vector<Block> gapped = {block("Pomodoro 1", 67, 3067, false)};
    SessionState state;
    for (int i = 0; i < 4; ++i) {
        SessionAdvice advice = advance(state, gapped, 10 * i, 0.25, true);
        checkEmptyAdvice(advice, "playback before the first block advises nothing");
    }
    checkState(state, -1, 0, "playback before the first block leaves the state idle");

    SessionAdvice latched = advance(state, gapped, 67, 0.25, true);
    check(state.block == 0, "playback reaching a block latches onto it, got " + describeState(state));
    checkEmptyAdvice(latched, "latching onto a block advises nothing");

    SessionState in_gap;
    std::vector<Block> two = {block("Pomodoro 1", 0, 300, false), block("Pomodoro 2", 500, 800, false)};
    SessionAdvice gap_advice = advance(in_gap, two, 400, 0.25, true);
    checkEmptyAdvice(gap_advice, "playback inside removed filler advises nothing");
    checkState(in_gap, -1, 0, "playback inside removed filler leaves the state idle");
    advance(in_gap, two, 500, 0.25, true);
    check(in_gap.block == 1, "playback leaving the gap latches onto the next block, got " + describeState(in_gap));
}

static void testPauseMidBlock()
{
    std::vector<Block> blocks = threeBlocks();
    SessionState state;
    advance(state, blocks, 10, 0.25, true);
    advance(state, blocks, 10, 0.25, true);
    checkState(state, 0, 0.5, "two playing ticks accrue half a second");

    for (int i = 0; i < 8; ++i) {
        SessionAdvice advice = advance(state, blocks, 10, 0.25, false);
        checkEmptyAdvice(advice, "a paused tick advises nothing");
    }
    checkState(state, 0, 0.5, "paused ticks accrue no time");

    advance(state, blocks, 10, 0.25, true);
    checkState(state, 0, 0.75, "time accrues again once playing resumes");
}

static void testPausedAtTheEndOfTheMedia()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false)};
    SessionState state;
    state.block = 0;
    state.elapsed = 20;
    SessionAdvice advice = advance(state, blocks, 59, 0.25, false);
    checkEmptyAdvice(advice, "a paused tick at the end of the media advises no loop");
    checkState(state, 0, 20, "a paused tick at the end of the media accrues no time");
}

static void testZeroAndNegativeSeconds()
{
    std::vector<Block> blocks = threeBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 10;

    SessionAdvice zero = advance(state, blocks, 10, 0, true);
    checkEmptyAdvice(zero, "a zero-second tick advises nothing");
    checkState(state, 0, 10, "a zero-second tick accrues no time");

    advance(state, blocks, 10, -5, true);
    check(state.elapsed >= 10 - 0.000001, "a negative tick must not reduce elapsed, got " + describeState(state));

    advance(state, blocks, 10, -0.25, true);
    check(state.elapsed >= 10 - 0.000001, "a small negative tick must not reduce elapsed, got " + describeState(state));
}

static void testLoopAdviceAtTheEndOfTheMedia()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 100, 160, false)};
    SessionState state;
    state.block = 0;
    state.elapsed = 10;

    SessionAdvice middle = advance(state, blocks, 130, 0.25, true);
    checkEmptyAdvice(middle, "mid-block playback advises no loop");

    state.elapsed = 10;
    SessionAdvice at_end = advance(state, blocks, 160, 0.25, true);
    check(at_end.seek_to == 100, "playback at the block's end is sent back to its start, got " + describeAdvice(at_end));
    check(!at_end.block_done, "a loop is not a finish, got " + describeAdvice(at_end));

    state.elapsed = 10;
    SessionAdvice near_end = advance(state, blocks, 159, 0.25, true);
    check(near_end.seek_to == 100, "playback within a second of the end counts as reaching it, got " + describeAdvice(near_end));

    state.elapsed = 10;
    SessionAdvice two_short = advance(state, blocks, 158, 0.25, true);
    checkEmptyAdvice(two_short, "playback two seconds short of the end advises no loop");
}

static void testNoPointlessLoop()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false)};

    SessionState almost;
    almost.block = 0;
    almost.elapsed = 59.5;
    SessionAdvice advice = advance(almost, blocks, 59, 0.25, true);
    checkEmptyAdvice(advice, "no loop is advised when under a second of real time is owed");

    SessionState one_owed;
    one_owed.block = 0;
    one_owed.elapsed = 59;
    SessionAdvice at_one = advance(one_owed, blocks, 59, 0.25, true);
    checkEmptyAdvice(at_one, "no loop is advised when exactly a second of real time is owed");

    SessionState plenty;
    plenty.block = 0;
    plenty.elapsed = 30;
    SessionAdvice loop = advance(plenty, blocks, 59, 0.25, true);
    check(loop.seek_to == 0, "a loop is advised when plenty of real time is owed, got " + describeAdvice(loop));

    SessionState next_tick;
    next_tick.block = 0;
    next_tick.elapsed = 59.75;
    SessionAdvice done = advance(next_tick, blocks, 59, 0.25, true);
    check(done.block_done, "the next tick finishes the block normally, got " + describeAdvice(done));
    check(done.seek_to == -1, "there is nowhere to seek after the last block, got " + describeAdvice(done));
    check(next_tick.block == -1, "the state ends after the last block, got " + describeState(next_tick));
}

static void testFinishingABlock()
{
    std::vector<Block> blocks = threeBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 59.9;
    SessionAdvice advice = advance(state, blocks, 30, 0.25, true);
    check(advice.block_done, "a block is done once its length of real time is spent, got " + describeAdvice(advice));
    check(advice.seek_to == 60, "finishing a block seeks to the next block's start, got " + describeAdvice(advice));
    checkState(state, 1, 0, "finishing a block moves to the next one with a fresh clock");

    SessionState exact;
    exact.block = 1;
    exact.elapsed = 39.75;
    SessionAdvice at_exact = advance(exact, blocks, 70, 0.25, true);
    check(at_exact.block_done, "reaching the block's length exactly finishes it, got " + describeAdvice(at_exact));
    check(at_exact.seek_to == 100, "the exact finish seeks to the next block, got " + describeAdvice(at_exact));
    checkState(exact, 2, 0, "the exact finish moves to the next block");

    SessionState last;
    last.block = 2;
    last.elapsed = 59.9;
    SessionAdvice at_last = advance(last, blocks, 130, 0.25, true);
    check(at_last.block_done, "the last block finishes too, got " + describeAdvice(at_last));
    check(at_last.seek_to == -1, "there is no seek after the last block, got " + describeAdvice(at_last));
    checkState(last, -1, 0, "the session ends after the last block");
}

static void testPlayAtOneTimes()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false)};
    SessionState state;
    state.block = 0;
    RunResult run = playBlock(state, blocks, 0, 1.0, 0.25);
    check(run.finished, "a block played at 1x finishes");
    check(run.loops == 0, "a block played at 1x never loops, got " + std::to_string(run.loops) + " loops");
    check(run.real_seconds >= 59.5 && run.real_seconds <= 60.5,
          "a block played at 1x takes its own length in real time, got " + std::to_string(run.real_seconds) + "s");
}

static void testLoopAtTwoTimes()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false)};
    SessionState state;
    state.block = 0;
    RunResult run = playBlock(state, blocks, 0, 2.0, 0.25);
    check(run.finished, "a block played at 2x finishes");
    check(run.loops >= 1, "a block played at 2x loops its media, got " + std::to_string(run.loops) + " loops");
    check(run.real_seconds >= 59.5 && run.real_seconds <= 60.5,
          "a block played at 2x still takes its full length in real time, got " + std::to_string(run.real_seconds) + "s");
    check(run.shortest_loop_owed > 1.0,
          "no loop is advised with a second or less owed, shortest owed was " + std::to_string(run.shortest_loop_owed) + "s");
}

static void testLoopAtThreeTimes()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 100, 1000, false)};
    SessionState state;
    state.block = 0;
    RunResult run = playBlock(state, blocks, 0, 3.0, 0.25);
    check(run.finished, "a long block played at 3x finishes");
    check(run.loops >= 2, "a long block played at 3x loops more than once, got " + std::to_string(run.loops) + " loops");
    check(run.real_seconds >= 899.5 && run.real_seconds <= 900.5,
          "a long block played at 3x takes 900s of real time, got " + std::to_string(run.real_seconds) + "s");
}

static void testPlayAtHalfTimes()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false)};
    SessionState state;
    state.block = 0;
    RunResult run = playBlock(state, blocks, 0, 0.5, 0.25);
    check(run.finished, "a block played at half speed finishes");
    check(run.loops == 0, "a block played at half speed never loops, got " + std::to_string(run.loops) + " loops");
    check(run.real_seconds >= 59.5 && run.real_seconds <= 60.5,
          "a block played at half speed takes its own length in real time, got " + std::to_string(run.real_seconds) + "s");
}

static void testFullRunThroughBlocks()
{
    std::vector<Block> blocks = threeBlocks();
    SessionState state;
    state.block = 0;
    std::vector<int> seeks;
    double total = 0;
    for (int i = 0; i < 3; ++i) {
        RunResult run = playBlock(state, blocks, i, 2.0, 0.25);
        check(run.finished, "block " + std::to_string(i) + " finishes in a full run");
        total += run.real_seconds;
        seeks.push_back(run.finish_seek);
        if (state.block != (i < 2 ? i + 1 : -1))
            break;
    }
    check(seeks.size() == 3 && seeks[0] == 60, "finishing block 0 seeks to 60, got " + std::to_string(seeks.empty() ? -2 : seeks[0]));
    check(seeks.size() == 3 && seeks[1] == 100, "finishing block 1 seeks to 100, got " + std::to_string(seeks.size() < 2 ? -2 : seeks[1]));
    check(seeks.size() == 3 && seeks[2] == -1, "finishing the last block seeks nowhere, got " + std::to_string(seeks.size() < 3 ? -2 : seeks[2]));
    checkState(state, -1, 0, "a full run ends with an idle state");
    check(total >= 159 && total <= 161, "a full run takes 160s of real time, got " + std::to_string(total) + "s");
}

static void testRestBlockStillTakesItsTime()
{
    std::vector<Block> blocks = {block("Guided Break 1", 0, 40, true)};
    SessionState state;
    state.block = 0;
    RunResult run = playBlock(state, blocks, 0, 2.0, 0.25);
    check(run.finished, "a rest block finishes");
    check(run.real_seconds >= 39.5 && run.real_seconds <= 40.5,
          "a rest block occupies its full wall-clock time, got " + std::to_string(run.real_seconds) + "s");
}

static void testRemaining()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState start;
    start.block = 0;
    start.elapsed = 0;
    check(remaining(start, blocks) == 60, "a fresh block owes its whole length, got " + std::to_string(remaining(start, blocks)));

    SessionState middle;
    middle.block = 0;
    middle.elapsed = 10.5;
    check(remaining(middle, blocks) == 49, "remaining truncates toward zero, got " + std::to_string(remaining(middle, blocks)));

    SessionState nearly;
    nearly.block = 0;
    nearly.elapsed = 59.5;
    check(remaining(nearly, blocks) == 0, "half a second owed truncates to 0, got " + std::to_string(remaining(nearly, blocks)));

    SessionState at_end;
    at_end.block = 0;
    at_end.elapsed = 60;
    check(remaining(at_end, blocks) == 0, "nothing is owed at the end of a block, got " + std::to_string(remaining(at_end, blocks)));

    SessionState over;
    over.block = 0;
    over.elapsed = 75;
    check(remaining(over, blocks) == 0, "remaining is never negative, got " + std::to_string(remaining(over, blocks)));

    SessionState idle;
    check(remaining(idle, blocks) == 0, "an idle state owes nothing, got " + std::to_string(remaining(idle, blocks)));

    SessionState past;
    past.block = 9;
    past.elapsed = 5;
    check(remaining(past, blocks) == 0, "a block index past the end owes nothing, got " + std::to_string(remaining(past, blocks)));

    SessionState rest;
    rest.block = 1;
    rest.elapsed = 0.25;
    check(remaining(rest, blocks) == 39, "a rest block owes its length too, got " + std::to_string(remaining(rest, blocks)));

    std::vector<Block> none;
    SessionState any;
    any.block = 0;
    check(remaining(any, none) == 0, "with no blocks nothing is owed, got " + std::to_string(remaining(any, none)));
}

static void testRemainingCountsDown()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 10, false)};
    SessionState state;
    state.block = 0;
    int last = remaining(state, blocks);
    check(last == 10, "the count starts at the block's length, got " + std::to_string(last));
    bool monotone = true;
    for (int i = 0; i < 40; ++i) {
        advance(state, blocks, 0, 0.25, true);
        if (state.block != 0)
            break;
        int now = remaining(state, blocks);
        if (now > last)
            monotone = false;
        last = now;
    }
    check(monotone, "remaining never rises while a block plays");
    check(last == 0, "remaining reaches 0 by the end of the block, got " + std::to_string(last));
}

static void testTenThousandSecondVideo()
{
    Video video = videoWith(10303, {mark(0, "Intro", true), mark(67, "Pomodoro 1", true), mark(3067, "Guided Break 1", true),
                                    mark(3667, "Pomodoro 2", true), mark(6667, "Guided Break 2", true),
                                    mark(7265, "Pomodoro 3", true), mark(10196, "Break 3", true)});
    std::vector<Block> blocks = blocksOf(video);
    check(blocks.size() == 6, "the real video gives 6 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 67, 3067, false, "real video");
    checkBlock(blocks, 1, "Guided Break 1", 3067, 3667, true, "real video");
    checkBlock(blocks, 2, "Pomodoro 2", 3667, 6667, false, "real video");
    checkBlock(blocks, 3, "Guided Break 2", 6667, 7265, true, "real video");
    checkBlock(blocks, 4, "Pomodoro 3", 7265, 10196, false, "real video");
    checkBlock(blocks, 5, "Break 3", 10196, 10303, true, "real video");

    check(blockAt(blocks, 0) == -1, "position 0 of the real video is inside the intro, got " + std::to_string(blockAt(blocks, 0)));
    check(blockAt(blocks, 66) == -1, "position 66 is still the intro, got " + std::to_string(blockAt(blocks, 66)));
    check(blockAt(blocks, 67) == 0, "position 67 starts Pomodoro 1, got " + std::to_string(blockAt(blocks, 67)));

    SessionState state;
    bool looped_in_intro = false;
    bool done_in_intro = false;
    for (int tick = 0; tick < 240; ++tick) {
        SessionAdvice advice = advance(state, blocks, tick / 4, 0.25, true);
        if (advice.seek_to != -1)
            looped_in_intro = true;
        if (advice.block_done)
            done_in_intro = true;
    }
    check(!looped_in_intro, "playback inside the intro is never sent back");
    check(!done_in_intro, "playback inside the intro never finishes a block");
    checkState(state, -1, 0, "playback inside the intro is not treated as a block");

    advance(state, blocks, 67, 0.25, true);
    check(state.block == 0, "the session begins once playback reaches 67, got " + describeState(state));
    check(state.block == 0 && blocks.size() == 6 && blocks[state.block].label == "Pomodoro 1",
          "the session begins with Pomodoro 1, got " + describeState(state));
    check(remaining(state, blocks) == 2999, "Pomodoro 1 owes nearly its full 3000s at the start, got " + std::to_string(remaining(state, blocks)));
}

int main()
{
    testPomodoroBlocks();
    testNoBlocks();
    testNonChapterMarksIgnored();
    testChaptersOutOfOrder();
    testZeroLengthAndInvertedBlocks();
    testRestLabels();
    testFillerAtStart();
    testFillerAtEnd();
    testFillerInTheMiddle();
    testFillerEverywhere();
    testFillerCaseVariations();
    testFillerDoesNotStretchNeighbours();
    testRestAndFillerTogether();
    testBlockAt();
    testNoBlocksAdvance();
    testAdoptBlock();
    testLatchesOnlyInsideABlock();
    testPauseMidBlock();
    testPausedAtTheEndOfTheMedia();
    testZeroAndNegativeSeconds();
    testLoopAdviceAtTheEndOfTheMedia();
    testNoPointlessLoop();
    testFinishingABlock();
    testPlayAtOneTimes();
    testLoopAtTwoTimes();
    testLoopAtThreeTimes();
    testPlayAtHalfTimes();
    testFullRunThroughBlocks();
    testRestBlockStillTakesItsTime();
    testRemaining();
    testRemainingCountsDown();
    testTenThousandSecondVideo();
    return report();
}
