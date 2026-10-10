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

static std::vector<Block> twoBlocks()
{
    return {block("Focus", 0, 60, false), block("Break", 60, 100, true)};
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

static void testLongPomodoroBlocks()
{
    std::vector<Block> blocks = blocksOf(videoWith(5400, {mark(0, "Focus 42", true), mark(2520, "Break 6", true), mark(2880, "Focus 42 again", true)}));
    check(blocks.size() == 3, "a 42/6/42 video gives 3 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Focus 42", 0, 2520, false, "42/6/42");
    checkBlock(blocks, 1, "Break 6", 2520, 2880, true, "42/6/42");
    checkBlock(blocks, 2, "Focus 42 again", 2880, 5400, false, "42/6/42");
}

static void testNoChapters()
{
    std::vector<Block> none = blocksOf(videoWith(3600, {}));
    check(none.empty(), "a video without marks gives no blocks, got " + describe(none));

    std::vector<Block> user_only = blocksOf(videoWith(3600, {mark(0, "Start here", false), mark(900, "Nice break idea", false), mark(1800, "Hard bit", false)}));
    check(user_only.empty(), "a video with only user marks gives no blocks, got " + describe(user_only));

    std::vector<Block> zero_duration = blocksOf(videoWith(0, {mark(0, "Pomodoro 1", true), mark(3000, "Guided Break 1", true)}));
    check(zero_duration.empty(), "a video with duration 0 gives no blocks, got " + describe(zero_duration));

    std::vector<Block> nothing = blocksOf(videoWith(0, {}));
    check(nothing.empty(), "a video with duration 0 and no marks gives no blocks, got " + describe(nothing));
}

static void testUserMarksMixedIn()
{
    std::vector<Block> blocks = blocksOf(videoWith(3000, {mark(0, "Pomodoro 1", true), mark(300, "Nice break idea", false),
                                                          mark(900, "Come back here", false), mark(1500, "Guided Break 1", true),
                                                          mark(2000, "Lovely music", false)}));
    check(blocks.size() == 2, "user marks between chapters do not make blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 1500, false, "user marks mixed in");
    checkBlock(blocks, 1, "Guided Break 1", 1500, 3000, true, "user marks mixed in");
}

static void testChapterAtZero()
{
    std::vector<Block> at_zero = blocksOf(videoWith(1200, {mark(0, "Intro", true), mark(600, "Pomodoro 1", true)}));
    check(at_zero.size() == 2, "a chapter at 0 gives a block starting at 0, got " + describe(at_zero));
    checkBlock(at_zero, 0, "Intro", 0, 600, false, "chapter at 0");
    checkBlock(at_zero, 1, "Pomodoro 1", 600, 1200, false, "chapter at 0");

    std::vector<Block> late = blocksOf(videoWith(1200, {mark(120, "Pomodoro 1", true), mark(600, "Guided Break 1", true)}));
    check(late.size() == 2, "a first chapter after 0 still gives its own block, got " + describe(late));
    checkBlock(late, 0, "Pomodoro 1", 120, 600, false, "first chapter after 0");
    checkBlock(late, 1, "Guided Break 1", 600, 1200, true, "first chapter after 0");
}

static void testChaptersOutOfOrder()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(3600, "Pomodoro 2", true), mark(0, "Pomodoro 1", true),
                                                          mark(6600, "Guided Break 2", true), mark(3000, "Guided Break 1", true)}));
    check(blocks.size() == 4, "chapters given out of order give 4 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 3000, false, "chapters out of order");
    checkBlock(blocks, 1, "Guided Break 1", 3000, 3600, true, "chapters out of order");
    checkBlock(blocks, 2, "Pomodoro 2", 3600, 6600, false, "chapters out of order");
    checkBlock(blocks, 3, "Guided Break 2", 6600, 7200, true, "chapters out of order");
}

static void testZeroLengthBlocks()
{
    std::vector<Block> at_end = blocksOf(videoWith(600, {mark(0, "Pomodoro 1", true), mark(600, "Outro", true)}));
    check(at_end.size() == 1, "a chapter exactly at duration is left out, got " + describe(at_end));
    checkBlock(at_end, 0, "Pomodoro 1", 0, 600, false, "chapter at duration");

    std::vector<Block> past_end = blocksOf(videoWith(600, {mark(900, "Outro", true)}));
    check(past_end.empty(), "a lone chapter past duration gives no blocks, got " + describe(past_end));

    std::vector<Block> same_time = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(400, "Guided Break 1", true), mark(400, "Pomodoro 2", true)}));
    check(same_time.size() == 2, "two chapters at the same time give one block each side, got " + describe(same_time));
    checkBlock(same_time, 0, "Pomodoro 1", 0, 400, false, "two chapters at the same time");
    check(same_time.size() > 1 && same_time[1].start == 400 && same_time[1].end == 1200,
          "the surviving block at a repeated chapter time runs 400 to 1200, got " + describe(same_time));

    std::vector<Block> only_same = blocksOf(videoWith(1200, {mark(300, "A", true), mark(300, "B", true)}));
    check(only_same.size() == 1, "two chapters at the same time and nothing else give one block, got " + describe(only_same));
}

static void testRestLabels()
{
    std::vector<Block> blocks = blocksOf(videoWith(700, {mark(0, "Guided Break 1", true), mark(100, "BREAK", true), mark(200, "short break", true),
                                                         mark(300, "Breakfast routine", true), mark(400, "Pomodoro 1", true), mark(500, "Deep Focus", true),
                                                         mark(600, "Study", true)}));
    check(blocks.size() == 7, "the rest label video gives 7 blocks, got " + describe(blocks));
    if (blocks.size() != 7)
        return;
    check(blocks[0].rest, "\"Guided Break 1\" is a rest block, got " + describe(blocks));
    check(blocks[1].rest, "\"BREAK\" is a rest block, got " + describe(blocks));
    check(blocks[2].rest, "\"short break\" is a rest block, got " + describe(blocks));
    check(blocks[3].rest, "\"Breakfast routine\" is a rest block, got " + describe(blocks));
    check(!blocks[4].rest, "\"Pomodoro 1\" is not a rest block, got " + describe(blocks));
    check(!blocks[5].rest, "\"Deep Focus\" is not a rest block, got " + describe(blocks));
    check(!blocks[6].rest, "\"Study\" is not a rest block, got " + describe(blocks));

    std::vector<Block> unlabelled = blocksOf(videoWith(200, {mark(0, "", true), mark(100, "Break", true)}));
    check(unlabelled.size() == 2, "a chapter with an empty label still gives a block, got " + describe(unlabelled));
    checkBlock(unlabelled, 0, "", 0, 100, false, "empty chapter label");
    checkBlock(unlabelled, 1, "Break", 100, 200, true, "empty chapter label");
}

static void testBlockAt()
{
    std::vector<Block> blocks = twoBlocks();
    check(blockAt(blocks, 0) == 0, "blockAt() at the start of the first block is 0, got " + std::to_string(blockAt(blocks, 0)));
    check(blockAt(blocks, 30) == 0, "blockAt() inside the first block is 0, got " + std::to_string(blockAt(blocks, 30)));
    check(blockAt(blocks, 59) == 0, "blockAt() at end - 1 of the first block is 0, got " + std::to_string(blockAt(blocks, 59)));
    check(blockAt(blocks, 60) == 1, "blockAt() at the end of the first block is 1, got " + std::to_string(blockAt(blocks, 60)));
    check(blockAt(blocks, 99) == 1, "blockAt() at end - 1 of the last block is 1, got " + std::to_string(blockAt(blocks, 99)));
    check(blockAt(blocks, 100) == -1, "blockAt() at the end of the last block is -1, got " + std::to_string(blockAt(blocks, 100)));
    check(blockAt(blocks, 5000) == -1, "blockAt() far past the last block is -1, got " + std::to_string(blockAt(blocks, 5000)));
    check(blockAt(blocks, -1) == -1, "blockAt() at -1 is -1, got " + std::to_string(blockAt(blocks, -1)));
    check(blockAt(blocks, -600) == -1, "blockAt() at -600 is -1, got " + std::to_string(blockAt(blocks, -600)));

    std::vector<Block> empty;
    check(blockAt(empty, 0) == -1, "blockAt() on no blocks is -1, got " + std::to_string(blockAt(empty, 0)));
    check(blockAt(empty, 42) == -1, "blockAt() on no blocks at 42 is -1, got " + std::to_string(blockAt(empty, 42)));
    check(blockAt(empty, -1) == -1, "blockAt() on no blocks at -1 is -1, got " + std::to_string(blockAt(empty, -1)));

    std::vector<Block> gap = {block("Pomodoro 1", 120, 600, false), block("Guided Break 1", 600, 1200, true)};
    check(blockAt(gap, 0) == -1, "blockAt() before the first block start is -1, got " + std::to_string(blockAt(gap, 0)));
    check(blockAt(gap, 119) == -1, "blockAt() one second before the first block start is -1, got " + std::to_string(blockAt(gap, 119)));
    check(blockAt(gap, 120) == 0, "blockAt() at the first block start is 0, got " + std::to_string(blockAt(gap, 120)));
}

static void testNoBlocksAdvance()
{
    std::vector<Block> blocks;
    SessionState state;
    SessionAdvice advice = advance(state, blocks, 0, 1, true);
    checkEmptyAdvice(advice, "advance() with no blocks gives empty advice");
    checkState(state, -1, 0, "advance() with no blocks leaves a fresh state alone");

    SessionState running;
    running.block = 3;
    running.elapsed = 7.5;
    advice = advance(running, blocks, 400, 2, true);
    checkEmptyAdvice(advice, "advance() with no blocks gives empty advice for a running state");
    checkState(running, 3, 7.5, "advance() with no blocks leaves a running state alone");

    advice = advance(running, blocks, 400, 2, false);
    checkEmptyAdvice(advice, "paused advance() with no blocks gives empty advice");
    checkState(running, 3, 7.5, "paused advance() with no blocks leaves the state alone");
}

static void testAdoptBlock()
{
    std::vector<Block> blocks = twoBlocks();

    SessionState fresh;
    SessionAdvice advice = advance(fresh, blocks, 70, 1, true);
    check(fresh.block == 1, "advance() from a fresh state adopts the block containing the position, got " + describeState(fresh));
    check(sameTime(fresh.elapsed, 1), "adopting a block resets elapsed before counting the call, got " + describeState(fresh));
    checkEmptyAdvice(advice, "adopting a block mid-block gives empty advice");

    SessionState stale;
    stale.block = 9;
    stale.elapsed = 55;
    advice = advance(stale, blocks, 10, 3, false);
    check(stale.block == 0, "a paused advance() with an out-of-range block adopts the block at the position, got " + describeState(stale));
    check(sameTime(stale.elapsed, 0), "adopting a block clears a stale elapsed, got " + describeState(stale));
    checkEmptyAdvice(advice, "paused adoption gives empty advice");

    SessionState negative_index;
    negative_index.block = -5;
    negative_index.elapsed = 12;
    advice = advance(negative_index, blocks, 10, 3, false);
    check(negative_index.block == 0, "a block index of -5 is adopted afresh, got " + describeState(negative_index));
    check(sameTime(negative_index.elapsed, 0), "a block index of -5 clears elapsed, got " + describeState(negative_index));
    checkEmptyAdvice(advice, "adopting over a block index of -5 gives empty advice");

    SessionState outside;
    outside.elapsed = 4;
    advice = advance(outside, blocks, 500, 1, false);
    check(outside.block == -1, "a position in no block leaves the block at -1, got " + describeState(outside));
    check(sameTime(outside.elapsed, 0), "a position in no block clears elapsed, got " + describeState(outside));
    checkEmptyAdvice(advice, "a position in no block gives empty advice");

    SessionState before_start;
    before_start.block = 7;
    advice = advance(before_start, blocks, -20, 1, false);
    check(before_start.block == -1, "a negative position leaves the block at -1, got " + describeState(before_start));
    checkEmptyAdvice(advice, "a negative position gives empty advice");

    SessionState playing_outside;
    advice = advance(playing_outside, blocks, 500, 1, true);
    check(playing_outside.block == -1, "a playing call with a position in no block leaves the block at -1, got " + describeState(playing_outside));
    checkEmptyAdvice(advice, "a playing call with a position in no block gives empty advice");

    SessionState valid;
    valid.block = 1;
    valid.elapsed = 20;
    advice = advance(valid, blocks, 10, 1, true);
    check(valid.block == 1, "a valid block is not re-adopted from the position, got " + describeState(valid));
    check(sameTime(valid.elapsed, 21), "a valid block keeps counting its own elapsed, got " + describeState(valid));
    checkEmptyAdvice(advice, "a valid block away from the position gives empty advice");
}

static void testPauseMidBlock()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 25;

    SessionAdvice advice = advance(state, blocks, 25, 5, false);
    checkEmptyAdvice(advice, "a paused call gives empty advice");
    checkState(state, 0, 25, "a paused call does not grow elapsed");

    for (int i = 0; i < 10; ++i)
        advice = advance(state, blocks, 25, 1, false);
    checkEmptyAdvice(advice, "ten paused calls give empty advice");
    checkState(state, 0, 25, "ten paused calls do not grow elapsed");

    SessionState at_end;
    at_end.block = 0;
    at_end.elapsed = 25;
    advice = advance(at_end, blocks, 59, 1, false);
    checkEmptyAdvice(advice, "a paused call with the audio at the block end does not loop");
    checkState(at_end, 0, 25, "a paused call with the audio at the block end leaves elapsed alone");

    SessionState overdue;
    overdue.block = 0;
    overdue.elapsed = 60;
    advice = advance(overdue, blocks, 10, 5, false);
    checkEmptyAdvice(advice, "a paused call does not finish a block whose time is already up");
    checkState(overdue, 0, 60, "a paused call leaves elapsed alone when the block time is already up");

    advice = advance(state, blocks, 25, 1, true);
    checkEmptyAdvice(advice, "resuming after a pause gives empty advice");
    checkState(state, 0, 26, "resuming after a pause counts the call");
}

static void testPlayAtOneTimes()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 0;

    bool clean = true;
    for (int i = 1; i <= 58; ++i) {
        SessionAdvice advice = advance(state, blocks, i - 1, 1, true);
        if (advice.seek_to != -1 || advice.block_done)
            clean = false;
        if (!sameTime(state.elapsed, i) || state.block != 0)
            clean = false;
    }
    check(clean, "58 seconds of 1x playback give empty advice and count up, got " + describeState(state));
    check(remaining(state, blocks) == 2, "after 58 seconds of a 60 second block 2 seconds remain, got " + std::to_string(remaining(state, blocks)));

    SessionAdvice advice = advance(state, blocks, 58, 1, true);
    checkEmptyAdvice(advice, "second 59 of 1x playback still gives empty advice");
    checkState(state, 0, 59, "second 59 of 1x playback counts up");

    advice = advance(state, blocks, 59, 1, true);
    check(advice.block_done, "second 60 of 1x playback finishes the block, got " + describeAdvice(advice));
    check(advice.seek_to == 60, "finishing the first block seeks to the second block start, got " + describeAdvice(advice));
    checkState(state, 1, 0, "finishing the first block moves to the second with elapsed 0");
    check(remaining(state, blocks) == 40, "the new block starts with its whole length remaining, got " + std::to_string(remaining(state, blocks)));
}

static void testLoopAtTwoTimes()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 0;

    int position = 0;
    int loops = 0;
    int finishes = 0;
    int first_loop = -1;
    int loop_seek = -1;
    int finish_seek = -1;
    for (int i = 0; i < 60; ++i) {
        SessionAdvice advice = advance(state, blocks, position, 1, true);
        if (advice.block_done) {
            ++finishes;
            finish_seek = advice.seek_to;
        } else if (advice.seek_to != -1) {
            ++loops;
            if (first_loop < 0) {
                first_loop = i;
                loop_seek = advice.seek_to;
            }
        }
        if (advice.seek_to >= 0)
            position = advice.seek_to;
        else
            position += 2;
    }
    check(loops == 1, "60 seconds of 2x playback of a 60 second block loops once, got " + std::to_string(loops));
    check(loop_seek == 0, "the loop of a 2x block seeks to the block start, got " + std::to_string(loop_seek));
    check(finishes == 1, "60 seconds of 2x playback finishes the block once, got " + std::to_string(finishes));
    check(finish_seek == 60, "finishing a 2x block seeks to the next block start, got " + std::to_string(finish_seek));
    checkState(state, 1, 0, "60 seconds of 2x playback ends on the second block");
}

static void testLoopAdvice()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 30;

    SessionAdvice advice = advance(state, blocks, 59, 1, true);
    check(advice.seek_to == 0 && !advice.block_done, "audio at end - 1 with time left seeks back to the block start, got " + describeAdvice(advice));
    check(sameTime(state.elapsed, 31), "a loop still counts the call, got " + describeState(state));
    check(state.block == 0, "a loop stays on the same block, got " + describeState(state));

    SessionState past;
    past.block = 0;
    past.elapsed = 30;
    advice = advance(past, blocks, 60, 1, true);
    check(advice.seek_to == 0 && !advice.block_done, "audio past the block end with time left seeks back to the block start, got " + describeAdvice(advice));

    SessionState inside;
    inside.block = 0;
    inside.elapsed = 30;
    advice = advance(inside, blocks, 58, 1, true);
    checkEmptyAdvice(advice, "audio two seconds before the block end does not loop");

    SessionState second;
    second.block = 1;
    second.elapsed = 10;
    advice = advance(second, blocks, 99, 1, true);
    check(second.block == 1 && advice.seek_to == 60 && !advice.block_done,
          "the last block loops back to its own start, got " + describeAdvice(advice) + " and " + describeState(second));

    SessionState both;
    both.block = 0;
    both.elapsed = 59;
    advice = advance(both, blocks, 59, 1, true);
    check(advice.block_done && advice.seek_to == 60,
          "a block whose time is up at the audio end moves on instead of looping, got " + describeAdvice(advice));
    checkState(both, 1, 0, "a block done at the audio end moves to the next block");
}

static void testPlayAtHalfTimes()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 0;

    bool clean = true;
    for (int i = 0; i < 59; ++i) {
        SessionAdvice advice = advance(state, blocks, i / 2, 1, true);
        if (advice.seek_to != -1 || advice.block_done)
            clean = false;
    }
    check(clean, "0.5x playback never loops while the block has time left, got " + describeState(state));
    checkState(state, 0, 59, "59 seconds of 0.5x playback counts up to 59");

    SessionAdvice advice = advance(state, blocks, 29, 1, true);
    check(advice.block_done && advice.seek_to == 60,
          "a 0.5x block finishes on real time with the audio only half played, got " + describeAdvice(advice));
    checkState(state, 1, 0, "a 0.5x block that runs out of time moves to the next block");
}

static void testExactBoundary()
{
    std::vector<Block> blocks = twoBlocks();

    SessionState whole;
    whole.block = 0;
    whole.elapsed = 59;
    SessionAdvice advice = advance(whole, blocks, 10, 1, true);
    check(advice.block_done && advice.seek_to == 60, "elapsed landing exactly on the length finishes the block, got " + describeAdvice(advice));
    checkState(whole, 1, 0, "elapsed landing exactly on the length resets to the next block");

    SessionState fractional;
    fractional.block = 0;
    fractional.elapsed = 59.5;
    advice = advance(fractional, blocks, 10, 0.5, true);
    check(advice.block_done && advice.seek_to == 60, "a fractional call landing exactly on the length finishes the block, got " + describeAdvice(advice));
    checkState(fractional, 1, 0, "a fractional call landing exactly on the length resets to the next block");

    SessionState just_short;
    just_short.block = 0;
    just_short.elapsed = 59;
    advice = advance(just_short, blocks, 10, 0.9, true);
    checkEmptyAdvice(advice, "stopping just short of the length does not finish the block");
    checkState(just_short, 0, 59.9, "stopping just short of the length keeps counting the same block");

    SessionState past;
    past.block = 0;
    past.elapsed = 55;
    advice = advance(past, blocks, 10, 20, true);
    check(advice.block_done && advice.seek_to == 60, "a call that overshoots the length finishes the block, got " + describeAdvice(advice));
    checkState(past, 1, 0, "a call that overshoots the length resets elapsed to 0");
}

static void testLastBlockEnds()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 1;
    state.elapsed = 39;

    SessionAdvice advice = advance(state, blocks, 80, 1, true);
    check(advice.block_done, "the last block finishing reports block_done, got " + describeAdvice(advice));
    check(advice.seek_to == -1, "the last block finishing gives no seek, got " + describeAdvice(advice));
    checkState(state, -1, 0, "the last block finishing leaves no current block");
    check(remaining(state, blocks) == 0, "nothing remains after the last block, got " + std::to_string(remaining(state, blocks)));

    std::vector<Block> single = {block("Pomodoro 1", 0, 30, false)};
    SessionState only;
    only.block = 0;
    only.elapsed = 29.5;
    advice = advance(only, single, 10, 0.5, true);
    check(advice.block_done && advice.seek_to == -1, "the only block finishing reports block_done with no seek, got " + describeAdvice(advice));
    checkState(only, -1, 0, "the only block finishing leaves no current block");
}

static void testNegativeSeconds()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 10;

    SessionAdvice advice = advance(state, blocks, 10, -5, true);
    checkEmptyAdvice(advice, "a call with -5 seconds gives empty advice");
    checkState(state, 0, 10, "a call with -5 seconds does not move time backwards");

    advice = advance(state, blocks, 10, -0.5, true);
    checkEmptyAdvice(advice, "a call with -0.5 seconds gives empty advice");
    checkState(state, 0, 10, "a call with -0.5 seconds does not move time backwards");

    advice = advance(state, blocks, 10, 0, true);
    checkEmptyAdvice(advice, "a call with 0 seconds gives empty advice");
    checkState(state, 0, 10, "a call with 0 seconds does not change elapsed");

    for (int i = 0; i < 20; ++i)
        advance(state, blocks, 10, -100, true);
    checkState(state, 0, 10, "twenty calls with -100 seconds leave elapsed alone");

    SessionState looping;
    looping.block = 0;
    looping.elapsed = 10;
    advice = advance(looping, blocks, 59, -5, true);
    check(advice.seek_to == 0 && !advice.block_done, "a call with -5 seconds still loops the audio at the block end, got " + describeAdvice(advice));
    checkState(looping, 0, 10, "a looping call with -5 seconds leaves elapsed alone");
}

static void testRemaining()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;

    state.elapsed = 0;
    check(remaining(state, blocks) == 60, "a fresh 60 second block has 60 seconds remaining, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 1;
    check(remaining(state, blocks) == 59, "one second into a 60 second block 59 remain, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 0.4;
    check(remaining(state, blocks) == 59, "0.4 seconds in, the remaining 59.6 rounds down to 59, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 2.999;
    check(remaining(state, blocks) == 57, "2.999 seconds in, the remaining 57.001 rounds down to 57, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 10.5;
    check(remaining(state, blocks) == 49, "10.5 seconds in, the remaining 49.5 rounds down to 49, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 59.5;
    check(remaining(state, blocks) == 0, "59.5 seconds in, the remaining 0.5 rounds down to 0, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 60;
    check(remaining(state, blocks) == 0, "a block whose time is exactly up has 0 remaining, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 61.75;
    check(remaining(state, blocks) == 0, "a block past its length never reports less than 0, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 5000;
    check(remaining(state, blocks) == 0, "a block long past its length reports 0, got " + std::to_string(remaining(state, blocks)));

    state.block = 1;
    state.elapsed = 0;
    check(remaining(state, blocks) == 40, "a fresh 40 second block has 40 seconds remaining, got " + std::to_string(remaining(state, blocks)));
    state.elapsed = 39.2;
    check(remaining(state, blocks) == 0, "39.2 seconds into a 40 second block, the remaining 0.8 rounds down to 0, got " + std::to_string(remaining(state, blocks)));

    SessionState fresh;
    check(remaining(fresh, blocks) == 0, "remaining() with no current block is 0, got " + std::to_string(remaining(fresh, blocks)));
    SessionState past_end;
    past_end.block = 2;
    past_end.elapsed = 3;
    check(remaining(past_end, blocks) == 0, "remaining() with an out-of-range block is 0, got " + std::to_string(remaining(past_end, blocks)));
    past_end.block = 99;
    check(remaining(past_end, blocks) == 0, "remaining() with a block index of 99 is 0, got " + std::to_string(remaining(past_end, blocks)));
    past_end.block = -4;
    check(remaining(past_end, blocks) == 0, "remaining() with a block index of -4 is 0, got " + std::to_string(remaining(past_end, blocks)));

    std::vector<Block> empty;
    SessionState any;
    any.block = 0;
    check(remaining(any, empty) == 0, "remaining() with no blocks is 0, got " + std::to_string(remaining(any, empty)));
}

static void testRemainingCountsDown()
{
    std::vector<Block> blocks = twoBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 0;

    bool counting = true;
    for (int i = 1; i <= 50; ++i) {
        advance(state, blocks, 10, 1, true);
        if (remaining(state, blocks) != 60 - i)
            counting = false;
    }
    check(counting, "remaining() counts down one per second of 1x playback, got " + std::to_string(remaining(state, blocks)) + " after 50 seconds");

    bool halving = true;
    SessionState fractional;
    fractional.block = 1;
    fractional.elapsed = 0;
    for (int i = 1; i <= 20; ++i) {
        advance(fractional, blocks, 70, 0.5, true);
        int expected = 40 - (i + 1) / 2;
        if (remaining(fractional, blocks) != expected)
            halving = false;
    }
    check(halving, "remaining() rounds down between half second calls, got " + std::to_string(remaining(fractional, blocks)) + " after 10 seconds");
}

static void testRemainingAcrossBlocks()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(0, "Pomodoro 1", true), mark(3000, "Guided Break 1", true),
                                                          mark(3600, "Pomodoro 2", true), mark(6600, "Guided Break 2", true)}));
    check(blocks.size() == 4, "the Pomodoro video still gives 4 blocks for the remaining() walk, got " + describe(blocks));
    if (blocks.size() != 4)
        return;

    SessionState state;
    state.block = 0;
    state.elapsed = 0;
    check(remaining(state, blocks) == 3000, "the first Pomodoro block has 3000 seconds remaining, got " + std::to_string(remaining(state, blocks)));

    state.elapsed = 2999;
    SessionAdvice advice = advance(state, blocks, 100, 1, true);
    check(advice.block_done && advice.seek_to == 3000, "the first Pomodoro block hands over to the break, got " + describeAdvice(advice));
    check(remaining(state, blocks) == 600, "the break block has 600 seconds remaining, got " + std::to_string(remaining(state, blocks)));

    state.elapsed = 600;
    advice = advance(state, blocks, 3100, 0, true);
    check(advice.block_done && advice.seek_to == 3600, "the break hands over to the second Pomodoro, got " + describeAdvice(advice));
    check(remaining(state, blocks) == 3000, "the second Pomodoro block has 3000 seconds remaining, got " + std::to_string(remaining(state, blocks)));
}

int main()
{
    testPomodoroBlocks();
    testLongPomodoroBlocks();
    testNoChapters();
    testUserMarksMixedIn();
    testChapterAtZero();
    testChaptersOutOfOrder();
    testZeroLengthBlocks();
    testRestLabels();
    testBlockAt();
    testNoBlocksAdvance();
    testAdoptBlock();
    testPauseMidBlock();
    testPlayAtOneTimes();
    testLoopAtTwoTimes();
    testLoopAdvice();
    testPlayAtHalfTimes();
    testExactBoundary();
    testLastBlockEnds();
    testNegativeSeconds();
    testRemaining();
    testRemainingCountsDown();
    testRemainingAcrossBlocks();

    return report();
}
