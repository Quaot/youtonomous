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

static std::string describeResume(const SessionState& state)
{
    return "resume_block " + std::to_string(state.resume_block) + ", resume_elapsed " + std::to_string(state.resume_elapsed)
         + ", resume_position " + std::to_string(state.resume_position);
}

static void checkResume(const SessionState& state, int expected_block, double expected_elapsed, int expected_position, const std::string& name)
{
    check(state.resume_block == expected_block && sameTime(state.resume_elapsed, expected_elapsed) && state.resume_position == expected_position,
          name + ": parked block " + std::to_string(expected_block) + ", elapsed " + std::to_string(expected_elapsed) + ", position "
              + std::to_string(expected_position) + ", got " + describeResume(state));
}

static void checkNothingParked(const SessionState& state, const std::string& name)
{
    checkResume(state, -1, 0, 0, name);
}

static void checkSame(const SessionState& got, const SessionState& before, const std::string& name)
{
    check(got.block == before.block && sameTime(got.elapsed, before.elapsed) && got.resume_block == before.resume_block
              && sameTime(got.resume_elapsed, before.resume_elapsed) && got.resume_position == before.resume_position,
          name + ": state stays " + describeState(before) + ", " + describeResume(before) + ", got " + describeState(got) + ", " + describeResume(got));
}

static std::vector<Block> threeBlocks()
{
    return {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true), block("Pomodoro 2", 100, 160, false)};
}

static std::vector<Block> breakBlocks()
{
    return {block("Pomodoro 1", 0, 60, false), block("Guided Break 1", 60, 100, true), block("Pomodoro 2", 100, 160, false),
            block("Guided Break 2", 160, 200, true), block("Pomodoro 3", 200, 260, false)};
}

static std::vector<Block> realVideoBlocks()
{
    return blocksOf(videoWith(10303, {mark(0, "Intro", true), mark(67, "Pomodoro 1", true), mark(3067, "Guided Break 1", true),
                                      mark(3667, "Pomodoro 2", true), mark(6667, "Guided Break 2", true), mark(7265, "Pomodoro 3", true),
                                      mark(10196, "Break 3", true)}));
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

static RunResult playBlock(SessionState& state, const std::vector<Block>& blocks, int index, double rate, double tick, bool loop = false)
{
    RunResult result;
    double position = blocks[index].start;
    double length = blocks[index].end - blocks[index].start;
    for (int i = 0; i < 100000; ++i) {
        double owed = length - state.elapsed;
        SessionAdvice advice = advance(state, blocks, static_cast<int>(position), tick, true, loop);
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
    check(blocks.size() == 3, "a 50/10 Pomodoro video ending on a break gives 3 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 3000, false, "50/10 Pomodoro");
    checkBlock(blocks, 1, "Guided Break 1", 3000, 3600, true, "50/10 Pomodoro");
    checkBlock(blocks, 2, "Pomodoro 2", 3600, 6600, false, "50/10 Pomodoro");

    std::vector<Block> ends_on_work = blocksOf(videoWith(7200, {mark(0, "Pomodoro 1", true), mark(3000, "Guided Break 1", true),
                                                                mark(3600, "Pomodoro 2", true), mark(6600, "Guided Break 2", true),
                                                                mark(7000, "Pomodoro 3", true)}));
    check(ends_on_work.size() == 5, "a Pomodoro video ending on work keeps all 5 blocks, got " + describe(ends_on_work));
    checkBlock(ends_on_work, 3, "Guided Break 2", 6600, 7000, true, "50/10 Pomodoro ending on work");
    checkBlock(ends_on_work, 4, "Pomodoro 3", 7000, 7200, false, "50/10 Pomodoro ending on work");
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
    check(mixed.size() == 1, "user marks between chapters do not make blocks, got " + describe(mixed));
    checkBlock(mixed, 0, "Pomodoro 1", 0, 1500, false, "user marks mixed in");

    std::vector<Block> non_chapter_intro = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(600, "Intro thought", false)}));
    check(non_chapter_intro.size() == 1, "a non-chapter intro mark changes nothing, got " + describe(non_chapter_intro));
    checkBlock(non_chapter_intro, 0, "Pomodoro 1", 0, 1200, false, "non-chapter intro mark");
}

static void testChaptersOutOfOrder()
{
    std::vector<Block> blocks = blocksOf(videoWith(7200, {mark(6600, "Guided Break 2", true), mark(0, "Pomodoro 1", true),
                                                          mark(3600, "Pomodoro 2", true), mark(3000, "Guided Break 1", true)}));
    check(blocks.size() == 3, "unsorted chapters still give 3 blocks once the trailing break goes, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 0, 3000, false, "unsorted chapters");
    checkBlock(blocks, 1, "Guided Break 1", 3000, 3600, true, "unsorted chapters");
    checkBlock(blocks, 2, "Pomodoro 2", 3600, 6600, false, "unsorted chapters");
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
    std::vector<Block> blocks = blocksOf(videoWith(1200, {mark(0, "Break 1", true), mark(100, "Pomodoro 1", true),
                                                          mark(200, "Guided Break 2", true), mark(300, "Pomodoro 2", true),
                                                          mark(400, "SHORT BREAK", true), mark(500, "Pomodoro 3", true),
                                                          mark(600, "bReAk time", true), mark(700, "Pomodoro 4", true),
                                                          mark(800, "Deep Work", true), mark(900, "Pomodoro 5", true),
                                                          mark(1000, "Breakfast review", true), mark(1100, "Pomodoro 6", true)}));
    check(blocks.size() == 12, "all twelve chapters give blocks, got " + describe(blocks));
    check(blocks.size() == 12 && blocks[0].rest, "\"Break 1\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && blocks[2].rest, "\"Guided Break 2\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && blocks[4].rest, "\"SHORT BREAK\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && blocks[6].rest, "\"bReAk time\" is a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && !blocks[8].rest, "\"Deep Work\" is not a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && blocks[10].rest, "\"Breakfast review\" contains break so it is a rest block, got " + describe(blocks));
    check(blocks.size() == 12 && !blocks[1].rest && !blocks[3].rest && !blocks[5].rest && !blocks[7].rest && !blocks[9].rest
              && !blocks[11].rest,
          "the plain Pomodoro chapters between them are work blocks, got " + describe(blocks));
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
    check(blocks.size() == 1, "Intro/Pomodoro 1/Break 1/Outro gives exactly one block, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 67, 3067, false, "filler at both ends");

    std::vector<Block> all_filler = blocksOf(videoWith(1200, {mark(0, "Intro", true), mark(600, "Outro", true)}));
    check(all_filler.empty(), "a video of nothing but filler gives no blocks, got " + describe(all_filler));

    std::vector<Block> one_filler = blocksOf(videoWith(1200, {mark(0, "INTRODUCTION", true)}));
    check(one_filler.empty(), "a lone filler chapter gives no blocks, got " + describe(one_filler));
}

static void testTrailingBreakIsNotABlock()
{
    std::vector<Block> one = blocksOf(videoWith(1200, {mark(0, "Pomodoro 1", true), mark(600, "Break 1", true)}));
    check(one.size() == 1, "a single trailing break is dropped, got " + describe(one));
    checkBlock(one, 0, "Pomodoro 1", 0, 600, false, "one trailing break");

    std::vector<Block> two = blocksOf(videoWith(1800, {mark(0, "Pomodoro 1", true), mark(600, "Break 1", true),
                                                       mark(1200, "Break 2", true)}));
    check(two.size() == 1, "two trailing breaks are both dropped, got " + describe(two));
    checkBlock(two, 0, "Pomodoro 1", 0, 600, false, "two trailing breaks");

    std::vector<Block> three = blocksOf(videoWith(2400, {mark(0, "Pomodoro 1", true), mark(600, "Break 1", true),
                                                         mark(1200, "Break 2", true), mark(1800, "Break 3", true)}));
    check(three.size() == 1, "three trailing breaks are all dropped, got " + describe(three));
    checkBlock(three, 0, "Pomodoro 1", 0, 600, false, "three trailing breaks");

    std::vector<Block> only_a_break = blocksOf(videoWith(600, {mark(0, "Break 1", true)}));
    check(only_a_break.empty(), "a single break as the only chapter gives no blocks, got " + describe(only_a_break));

    std::vector<Block> all_breaks = blocksOf(videoWith(1800, {mark(0, "Break 1", true), mark(600, "SHORT BREAK", true),
                                                              mark(1200, "Guided Break 3", true)}));
    check(all_breaks.empty(), "a video of nothing but breaks gives no blocks, got " + describe(all_breaks));

    std::vector<Block> break_then_outro = blocksOf(videoWith(1800, {mark(0, "Pomodoro 1", true), mark(600, "Break 1", true),
                                                                    mark(1200, "Outro", true)}));
    check(break_then_outro.size() == 1, "a break left trailing by a dropped outro goes too, got " + describe(break_then_outro));
    checkBlock(break_then_outro, 0, "Pomodoro 1", 0, 600, false, "break before an outro");

    std::vector<Block> kept = blocksOf(videoWith(1800, {mark(0, "Pomodoro 1", true), mark(600, "Break 1", true),
                                                        mark(1200, "Pomodoro 2", true)}));
    check(kept.size() == 3, "a break with work after it stays, got " + describe(kept));
    checkBlock(kept, 1, "Break 1", 600, 1200, true, "break with work after it");

    std::vector<Block> boundary = blocksOf(videoWith(1800, {mark(0, "Pomodoro 1", true), mark(600, "Pomodoro 2", true),
                                                            mark(1500, "Break 1", true)}));
    check(boundary.size() == 2, "dropping a trailing break leaves two blocks, got " + describe(boundary));
    checkBlock(boundary, 1, "Pomodoro 2", 600, 1500, false, "boundary after a dropped trailing break");
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
    check(blocks.size() == 5, "the real video gives 5 blocks, got " + describe(blocks));
    checkBlock(blocks, 0, "Pomodoro 1", 67, 3067, false, "real video");
    checkBlock(blocks, 1, "Guided Break 1", 3067, 3667, true, "real video");
    checkBlock(blocks, 2, "Pomodoro 2", 3667, 6667, false, "real video");
    checkBlock(blocks, 3, "Guided Break 2", 6667, 7265, true, "real video");
    checkBlock(blocks, 4, "Pomodoro 3", 7265, 10196, false, "real video");
    check(blockAt(blocks, 10196) == -1, "the padding at 10196 is in no block, got " + std::to_string(blockAt(blocks, 10196)));

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
    check(state.block == 0 && blocks.size() == 5 && blocks[state.block].label == "Pomodoro 1",
          "the session begins with Pomodoro 1, got " + describeState(state));
    check(remaining(state, blocks) == 2999, "Pomodoro 1 owes nearly its full 3000s at the start, got " + std::to_string(remaining(state, blocks)));
}

static void testSkipWithNoBlocks()
{
    std::vector<Block> none;

    SessionState fresh;
    SessionState before = fresh;
    SessionAdvice advice = skip(fresh, none, 30, false);
    checkEmptyAdvice(advice, "skipping with no blocks gives empty advice");
    checkSame(fresh, before, "skipping with no blocks leaves the state alone");

    SessionState banked;
    SessionAdvice banked_advice = skip(banked, none, 30, true);
    checkEmptyAdvice(banked_advice, "a banked skip with no blocks gives empty advice");
    checkSame(banked, SessionState(), "a banked skip with no blocks parks nothing");

    SessionState running;
    running.block = 1;
    running.elapsed = 17.5;
    SessionState running_before = running;
    SessionAdvice running_advice = skip(running, none, 90, true);
    checkEmptyAdvice(running_advice, "skipping with no blocks never advises anything");
    checkSame(running, running_before, "skipping with no blocks does not touch a running state");
}

static void testSkipWhileOnABreak()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState on_break;
    on_break.block = 1;
    on_break.elapsed = 12.5;
    SessionAdvice advice = skip(on_break, blocks, 70, false);
    check(advice.seek_to == 100, "skipping while on a break seeks to the next work block, got " + describeAdvice(advice));
    check(!advice.block_done, "skipping while on a break finishes nothing, got " + describeAdvice(advice));
    checkState(on_break, 2, 0, "skipping while on a break lands on the next work block with a fresh clock");
    checkNothingParked(on_break, "skipping while on a break parks nothing");

    SessionState banked;
    banked.block = 1;
    banked.elapsed = 12.5;
    SessionAdvice banked_advice = skip(banked, blocks, 70, true);
    check(banked_advice.seek_to == 100, "a banked skip while on a break seeks to the next work block, got " + describeAdvice(banked_advice));
    check(!banked_advice.block_done, "a banked skip while on a break finishes nothing, got " + describeAdvice(banked_advice));
    checkState(banked, 2, 0, "a banked skip while on a break lands on the next work block");
    checkNothingParked(banked, "a banked skip while on a break parks nothing");

    SessionState later_break;
    later_break.block = 3;
    later_break.elapsed = 1;
    SessionAdvice later = skip(later_break, blocks, 170, true);
    check(later.seek_to == 200, "skipping while on a later break seeks to the work block after it, got " + describeAdvice(later));
    check(!later.block_done, "skipping while on a later break finishes nothing, got " + describeAdvice(later));
    checkState(later_break, 4, 0, "skipping while on a later break lands on the last work block");
    checkNothingParked(later_break, "skipping while on a later break parks nothing");

    SessionState break_before_a_break;
    break_before_a_break.block = 1;
    break_before_a_break.elapsed = 5;
    SessionAdvice pair = skip(break_before_a_break, blocks, 70, false);
    check(pair.seek_to == 100, "a break ahead is never the target when leaving a break, got " + describeAdvice(pair));
    checkState(break_before_a_break, 2, 0, "leaving a break with a break ahead still lands on work");
}

static void testCuttingABreakShortWithAParkedBlock()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState state;
    state.block = 1;
    state.elapsed = 12.5;
    state.resume_block = 0;
    state.resume_elapsed = 25;
    state.resume_position = 25;
    SessionAdvice advice = skip(state, blocks, 70, false);
    check(advice.seek_to == 25, "cutting a break short seeks back to the parked position, got " + describeAdvice(advice));
    check(!advice.block_done, "cutting a break short finishes nothing, got " + describeAdvice(advice));
    checkState(state, 0, 25, "cutting a break short hands the parked block back with its accrued time");
    checkNothingParked(state, "cutting a break short clears the parked fields");
    check(remaining(state, blocks) == 35, "the handed back block owes only what was left, got " + std::to_string(remaining(state, blocks)));

    SessionState banked;
    banked.block = 1;
    banked.elapsed = 12.5;
    banked.resume_block = 0;
    banked.resume_elapsed = 25;
    banked.resume_position = 25;
    SessionAdvice banked_advice = skip(banked, blocks, 70, true);
    check(banked_advice.seek_to == 25, "a banked skip off a break still hands the parked block back, got " + describeAdvice(banked_advice));
    check(!banked_advice.block_done, "a banked skip off a break finishes nothing, got " + describeAdvice(banked_advice));
    checkState(banked, 0, 25, "a banked skip off a break hands the parked block back");
    checkNothingParked(banked, "a banked skip off a break clears the parked fields");

    SessionState ahead;
    ahead.block = 1;
    ahead.elapsed = 1;
    ahead.resume_block = 0;
    ahead.resume_elapsed = 40;
    ahead.resume_position = 40;
    SessionAdvice ahead_advice = skip(ahead, blocks, 70, false);
    check(ahead_advice.seek_to == 40, "the parked block beats a work block starting later, got " + describeAdvice(ahead_advice));
    checkState(ahead, 0, 40, "the parked block is taken over the work block ahead");

    SessionState last_break;
    last_break.block = 3;
    last_break.elapsed = 2;
    last_break.resume_block = 2;
    last_break.resume_elapsed = 30;
    last_break.resume_position = 130;
    SessionAdvice last_advice = skip(last_break, blocks, 170, true);
    check(last_advice.seek_to == 130, "a parked block is handed back from a later break too, got " + describeAdvice(last_advice));
    checkState(last_break, 2, 30, "a parked block is handed back from a later break");
    checkNothingParked(last_break, "the later break clears the parked fields as well");
}

static void testBothBankValuesAgreeOffABreak()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState unbanked;
    unbanked.block = 1;
    unbanked.elapsed = 9;
    SessionState to_bank = unbanked;
    SessionAdvice plain = skip(unbanked, blocks, 70, false);
    SessionAdvice with_bank = skip(to_bank, blocks, 70, true);
    check(plain.seek_to == with_bank.seek_to && plain.block_done == with_bank.block_done,
          "bank makes no difference to the advice off a break, got " + describeAdvice(plain) + " and " + describeAdvice(with_bank));
    checkSame(to_bank, unbanked, "bank makes no difference to the state off a break");

    SessionState parked;
    parked.block = 3;
    parked.elapsed = 4;
    parked.resume_block = 2;
    parked.resume_elapsed = 18;
    parked.resume_position = 118;
    SessionState parked_banked = parked;
    SessionAdvice parked_plain = skip(parked, blocks, 170, false);
    SessionAdvice parked_with_bank = skip(parked_banked, blocks, 170, true);
    check(parked_plain.seek_to == parked_with_bank.seek_to && parked_plain.block_done == parked_with_bank.block_done,
          "bank makes no difference when a parked block comes back, got " + describeAdvice(parked_plain) + " and " + describeAdvice(parked_with_bank));
    checkSame(parked_banked, parked, "bank makes no difference to the state when a parked block comes back");
}

static void testCuttingABreakShortTargetChoice()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState at_work_start;
    at_work_start.block = 1;
    at_work_start.elapsed = 3;
    SessionAdvice at_start = skip(at_work_start, blocks, 100, false);
    check(at_start.seek_to == 200, "a position exactly at a work block's start is looked past, got " + describeAdvice(at_start));
    checkState(at_work_start, 4, 0, "a position exactly at a work block's start lands on the later work block");

    std::vector<Block> ends_on_a_break = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true)};
    SessionState trailing;
    trailing.block = 1;
    trailing.elapsed = 7;
    SessionState trailing_before = trailing;
    SessionAdvice trailing_advice = skip(trailing, ends_on_a_break, 70, false);
    checkEmptyAdvice(trailing_advice, "a break with no work after it gives empty advice");
    checkSame(trailing, trailing_before, "a break with no work after it leaves the state alone");

    SessionState trailing_banked = trailing_before;
    SessionAdvice trailing_banked_advice = skip(trailing_banked, ends_on_a_break, 70, true);
    checkEmptyAdvice(trailing_banked_advice, "a banked skip off a trailing break gives empty advice");
    checkSame(trailing_banked, trailing_before, "a banked skip off a trailing break leaves the state alone");

    std::vector<Block> two_trailing_breaks = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true), block("Break 2", 100, 140, true)};
    SessionState on_the_last;
    on_the_last.block = 2;
    on_the_last.elapsed = 5;
    SessionState on_the_last_before = on_the_last;
    SessionAdvice on_the_last_advice = skip(on_the_last, two_trailing_breaks, 110, false);
    checkEmptyAdvice(on_the_last_advice, "the last block being a break gives empty advice");
    checkSame(on_the_last, on_the_last_before, "the last block being a break leaves the state alone");

    std::vector<Block> work_ahead = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true), block("Pomodoro 2", 100, 160, false),
                                     block("Pomodoro 3", 160, 220, false), block("Pomodoro 4", 220, 280, false)};
    SessionState nearest;
    nearest.block = 1;
    nearest.elapsed = 6;
    SessionAdvice nearest_advice = skip(nearest, work_ahead, 70, false);
    check(nearest_advice.seek_to == 100, "with several work blocks ahead the nearest one wins, got " + describeAdvice(nearest_advice));
    checkState(nearest, 2, 0, "with several work blocks ahead the nearest one is the target");

    SessionState past_the_first;
    past_the_first.block = 1;
    past_the_first.elapsed = 6;
    SessionAdvice past_advice = skip(past_the_first, work_ahead, 150, false);
    check(past_advice.seek_to == 160, "a work block behind the position is never the target, got " + describeAdvice(past_advice));
    checkState(past_the_first, 3, 0, "the next work block after the position is taken");

    std::vector<Block> breaks_in_between = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true), block("Break 2", 100, 140, true),
                                            block("Pomodoro 2", 140, 200, false)};
    SessionState over_a_break;
    over_a_break.block = 1;
    over_a_break.elapsed = 8;
    SessionAdvice over_advice = skip(over_a_break, breaks_in_between, 70, false);
    check(over_advice.seek_to == 140, "leaving a break looks past another break to find work, got " + describeAdvice(over_advice));
    checkState(over_a_break, 3, 0, "leaving a break lands on work rather than the break after it");
}

static void testCuttingABreakShortHandsBackOnlyOnce()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState state;
    state.block = 1;
    state.elapsed = 10;
    state.resume_block = 0;
    state.resume_elapsed = 25;
    state.resume_position = 25;
    SessionAdvice first = skip(state, blocks, 70, false);
    check(first.seek_to == 25, "the first skip off the break hands the parked block back, got " + describeAdvice(first));
    checkNothingParked(state, "the first skip off the break clears the parked fields");

    state.block = 1;
    state.elapsed = 10;
    SessionAdvice second = skip(state, blocks, 70, false);
    check(second.seek_to == 100, "a second skip off the break does not hand the same block back, got " + describeAdvice(second));
    checkState(state, 2, 0, "a second skip off the break moves on to work");
    checkNothingParked(state, "a second skip off the break parks nothing");
}

static void testCutShortRoundTrip()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 0;

    for (int i = 0; i < 10; ++i)
        advance(state, blocks, i, 1.0, true);
    checkState(state, 0, 10, "ten ticks of the work block accrue ten seconds");

    SessionAdvice banked = skip(state, blocks, 10, true);
    check(banked.seek_to == 60, "the banked skip seeks to the break, got " + describeAdvice(banked));
    check(!banked.block_done, "the banked skip is not a finish, got " + describeAdvice(banked));
    checkResume(state, 0, 10, 10, "the banked skip parks the work block mid-run");

    for (int i = 0; i < 5; ++i)
        advance(state, blocks, 60 + i, 1.0, true);
    checkState(state, 1, 5, "five ticks of the break accrue five seconds");

    SessionAdvice cut = skip(state, blocks, 65, false);
    check(cut.seek_to == 10, "cutting the break short seeks back to where the work block was left, got " + describeAdvice(cut));
    check(!cut.block_done, "cutting the break short finishes nothing, got " + describeAdvice(cut));
    checkState(state, 0, 10, "cutting the break short returns to the work block with its ten seconds");
    checkNothingParked(state, "cutting the break short clears the parked fields");
    check(remaining(state, blocks) == 50, "the resumed work block owes fifty seconds, got " + std::to_string(remaining(state, blocks)));

    RunResult rest_of_it = playBlock(state, blocks, 0, 1.0, 0.25);
    check(rest_of_it.finished, "the resumed work block finishes after the break was cut short");
    check(rest_of_it.real_seconds >= 49.5 && rest_of_it.real_seconds <= 50.5,
          "the resumed work block takes only the owed time, got " + std::to_string(rest_of_it.real_seconds) + "s");
    check(rest_of_it.finish_seek == 60, "the resumed work block moves on to the break after it, got " + std::to_string(rest_of_it.finish_seek));
    checkState(state, 1, 0, "the resumed work block moves on to the next block");
    checkNothingParked(state, "nothing is parked once the cut short round trip is over");

    RunResult the_break = playBlock(state, blocks, 1, 1.0, 0.25);
    check(the_break.finished, "the break after the resumed block finishes");
    check(the_break.finish_seek == 100, "the break after the resumed block moves on normally, got " + std::to_string(the_break.finish_seek));
    checkState(state, 2, 0, "the break after the cut short round trip does not loop back");
}

static void testCutBreakShortOnTheRealVideo()
{
    std::vector<Block> blocks = realVideoBlocks();

    SessionState plain;
    plain.block = 1;
    plain.elapsed = 120;
    SessionAdvice advice = skip(plain, blocks, 3200, false);
    check(advice.seek_to == 3667, "cutting Guided Break 1 short lands on Pomodoro 2 at 3667, got " + describeAdvice(advice));
    check(!advice.block_done, "cutting Guided Break 1 short finishes nothing, got " + describeAdvice(advice));
    check(plain.block == 2 && blocks.size() == 5 && blocks[plain.block].label == "Pomodoro 2",
          "cutting Guided Break 1 short lands on Pomodoro 2, got " + describeState(plain));
    checkState(plain, 2, 0, "cutting Guided Break 1 short starts Pomodoro 2 with a fresh clock");
    checkNothingParked(plain, "cutting Guided Break 1 short parks nothing");

    SessionState banked;
    banked.block = 0;
    banked.elapsed = 1800;
    SessionAdvice out = skip(banked, blocks, 1867, true);
    check(out.seek_to == 3067, "the banked skip out of Pomodoro 1 seeks to 3067, got " + describeAdvice(out));
    checkResume(banked, 0, 1800, 1867, "the banked skip parks Pomodoro 1");

    for (int i = 0; i < 20; ++i)
        advance(banked, blocks, 3067 + i, 1.0, true);
    checkState(banked, 1, 20, "twenty ticks of Guided Break 1 accrue twenty seconds");

    SessionAdvice cut = skip(banked, blocks, 3087, false);
    check(cut.seek_to == 1867, "cutting Guided Break 1 short hands Pomodoro 1 back at 1867, got " + describeAdvice(cut));
    check(!cut.block_done, "cutting Guided Break 1 short after a banked skip finishes nothing, got " + describeAdvice(cut));
    check(banked.block == 0 && blocks.size() == 5 && blocks[banked.block].label == "Pomodoro 1",
          "cutting Guided Break 1 short returns to Pomodoro 1, got " + describeState(banked));
    checkState(banked, 0, 1800, "Pomodoro 1 comes back with its 1800s already spent");
    checkNothingParked(banked, "cutting Guided Break 1 short clears the parked fields");
    check(remaining(banked, blocks) == 1200, "Pomodoro 1 owes 1200s after the break was cut short, got " + std::to_string(remaining(banked, blocks)));

    RunResult owed = playBlock(banked, blocks, 0, 1.0, 0.25);
    check(owed.finished, "the Pomodoro 1 handed back by the cut short break finishes");
    check(owed.real_seconds >= 1199.5 && owed.real_seconds <= 1200.5,
          "the handed back Pomodoro 1 takes only the owed 1200s, got " + std::to_string(owed.real_seconds) + "s");
    check(owed.finish_seek == 3067, "the handed back Pomodoro 1 moves on to Guided Break 1, got " + std::to_string(owed.finish_seek));
    checkState(banked, 1, 0, "the handed back Pomodoro 1 moves on rather than looping back");

    SessionState second_break;
    second_break.block = 3;
    second_break.elapsed = 100;
    SessionAdvice onwards = skip(second_break, blocks, 6800, false);
    check(onwards.seek_to == 7265, "cutting Guided Break 2 short lands on Pomodoro 3 at 7265, got " + describeAdvice(onwards));
    checkState(second_break, 4, 0, "cutting Guided Break 2 short starts Pomodoro 3 with a fresh clock");

    SessionState last_work;
    last_work.block = 4;
    last_work.elapsed = 10;
    SessionAdvice back = skip(last_work, blocks, 10200, true);
    check(back.seek_to == 3067, "skipping on the real video's last work block goes back to Guided Break 1, got " + describeAdvice(back));
    checkState(last_work, 1, 0, "skipping on the real video's last work block starts Guided Break 1");
    checkResume(last_work, 4, 10, 10200, "skipping on the real video's last work block parks Pomodoro 3");
}

static void testSkipWithNoBreakAhead()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState last;
    last.block = 2;
    last.elapsed = 20;
    SessionAdvice advice = skip(last, blocks, 130, false);
    check(advice.seek_to == 60, "skipping with no break ahead goes back to the first break, got " + describeAdvice(advice));
    check(advice.block_done, "an unbanked skip with no break ahead ends the work block, got " + describeAdvice(advice));
    checkState(last, 1, 0, "skipping with no break ahead starts the first break with a fresh clock");
    checkNothingParked(last, "an unbanked skip with no break ahead parks nothing");

    SessionState banked;
    banked.block = 2;
    banked.elapsed = 20;
    SessionAdvice banked_advice = skip(banked, blocks, 130, true);
    check(banked_advice.seek_to == 60, "a banked skip with no break ahead goes back to the first break, got " + describeAdvice(banked_advice));
    check(!banked_advice.block_done, "a banked skip with no break ahead is not a finish, got " + describeAdvice(banked_advice));
    checkState(banked, 1, 0, "a banked skip with no break ahead starts the first break");
    checkResume(banked, 2, 20, 130, "a banked skip with no break ahead parks the last work block");

    SessionState at_break_start;
    at_break_start.block = 0;
    at_break_start.elapsed = 10;
    SessionAdvice at_break = skip(at_break_start, blocks, 60, false);
    check(at_break.seek_to == 60, "the only break starting exactly at the position is still reached, got " + describeAdvice(at_break));
    checkState(at_break_start, 1, 0, "the only break starting at the position becomes the block");

    std::vector<Block> all_work = {block("Pomodoro 1", 0, 60, false), block("Pomodoro 2", 60, 120, false)};
    SessionState no_rest;
    no_rest.block = 0;
    no_rest.elapsed = 5;
    SessionState no_rest_before = no_rest;
    SessionAdvice none = skip(no_rest, all_work, 5, true);
    checkEmptyAdvice(none, "skipping in a video without any break gives empty advice");
    checkSame(no_rest, no_rest_before, "skipping in a video without any break leaves the state alone");

    SessionState idle_no_rest;
    SessionAdvice idle_advice = skip(idle_no_rest, all_work, 0, false);
    checkEmptyAdvice(idle_advice, "skipping with no block and no break ahead gives empty advice");
    checkSame(idle_no_rest, SessionState(), "skipping with no block and no break ahead leaves the state alone");
}

static void testSkipFromAWorkBlockUnbanked()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 30;
    SessionAdvice advice = skip(state, blocks, 30, false);
    check(advice.seek_to == 60, "an unbanked skip seeks to the next break's start, got " + describeAdvice(advice));
    check(advice.block_done, "an unbanked skip from a work block reports the block as done, got " + describeAdvice(advice));
    checkState(state, 1, 0, "an unbanked skip moves to the break with a fresh clock");
    checkNothingParked(state, "an unbanked skip parks nothing");
    check(remaining(state, blocks) == 40, "the break owes its full length after a skip, got " + std::to_string(remaining(state, blocks)));

    SessionState fresh_work;
    fresh_work.block = 2;
    fresh_work.elapsed = 0;
    SessionAdvice second = skip(fresh_work, blocks, 100, false);
    check(second.seek_to == 160, "an unbanked skip from the second work block seeks to 160, got " + describeAdvice(second));
    check(second.block_done, "an unbanked skip with nothing accrued still reports the block as done, got " + describeAdvice(second));
    checkState(fresh_work, 3, 0, "an unbanked skip from the second work block moves to the second break");
    checkNothingParked(fresh_work, "an unbanked skip from the second work block parks nothing");
}

static void testSkipFromAWorkBlockBanked()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 30;
    SessionAdvice advice = skip(state, blocks, 42, true);
    check(advice.seek_to == 60, "a banked skip seeks to the next break's start, got " + describeAdvice(advice));
    check(!advice.block_done, "a banked skip does not report the block as done, got " + describeAdvice(advice));
    checkState(state, 1, 0, "a banked skip moves to the break with a fresh clock");
    checkResume(state, 0, 30, 42, "a banked skip parks the abandoned block");

    SessionState second;
    second.block = 2;
    second.elapsed = 12.25;
    SessionAdvice second_advice = skip(second, blocks, 150, true);
    check(second_advice.seek_to == 160, "a banked skip from the second work block seeks to 160, got " + describeAdvice(second_advice));
    check(!second_advice.block_done, "a banked skip from the second work block is not a finish, got " + describeAdvice(second_advice));
    checkState(second, 3, 0, "a banked skip from the second work block moves to the second break");
    checkResume(second, 2, 12.25, 150, "a banked skip parks the second work block");

    SessionState nothing_done;
    nothing_done.block = 0;
    nothing_done.elapsed = 0;
    skip(nothing_done, blocks, 0, true);
    checkResume(nothing_done, 0, 0, 0, "a banked skip with nothing accrued parks a zero elapsed");
}

static void testSkipWithNoActiveBlock()
{
    std::vector<Block> blocks = realVideoBlocks();

    SessionState idle;
    SessionAdvice advice = skip(idle, blocks, 30, false);
    check(advice.seek_to == 3067, "skipping during the intro jumps to the first break, got " + describeAdvice(advice));
    check(!advice.block_done, "skipping with no block active finishes nothing, got " + describeAdvice(advice));
    checkState(idle, 1, 0, "skipping during the intro lands on the first break");
    checkNothingParked(idle, "skipping during the intro parks nothing");

    SessionState banked;
    SessionAdvice banked_advice = skip(banked, blocks, 30, true);
    check(banked_advice.seek_to == 3067, "a banked skip during the intro still jumps to the first break, got " + describeAdvice(banked_advice));
    check(!banked_advice.block_done, "a banked skip with no block active finishes nothing, got " + describeAdvice(banked_advice));
    checkState(banked, 1, 0, "a banked skip during the intro lands on the first break");
    checkNothingParked(banked, "banking is meaningless with no block active and parks nothing");

    std::vector<Block> gapped = {block("Pomodoro 1", 0, 300, false), block("Guided Break 1", 500, 800, true)};
    SessionState in_gap;
    SessionAdvice gap_advice = skip(in_gap, gapped, 400, true);
    check(gap_advice.seek_to == 500, "skipping from inside removed filler jumps to the break, got " + describeAdvice(gap_advice));
    check(!gap_advice.block_done, "skipping from inside removed filler finishes nothing, got " + describeAdvice(gap_advice));
    checkState(in_gap, 1, 0, "skipping from inside removed filler lands on the break");
    checkNothingParked(in_gap, "skipping from inside removed filler parks nothing");

    SessionState past_end;
    past_end.block = 9;
    past_end.elapsed = 5;
    SessionAdvice past_advice = skip(past_end, blocks, 30, true);
    check(past_advice.seek_to == 3067, "a block index past the end counts as no block active, got " + describeAdvice(past_advice));
    check(!past_advice.block_done, "a block index past the end finishes nothing, got " + describeAdvice(past_advice));
    checkState(past_end, 1, 0, "a skip with a block index past the end lands on the first break");
    checkNothingParked(past_end, "a skip with a block index past the end parks nothing");
}

static void testSkipTargetChoice()
{
    std::vector<Block> blocks = breakBlocks();

    SessionState nearest;
    nearest.block = 0;
    nearest.elapsed = 5;
    SessionAdvice nearest_advice = skip(nearest, blocks, 0, false);
    check(nearest_advice.seek_to == 60, "the nearest break ahead wins, got " + describeAdvice(nearest_advice));
    checkState(nearest, 1, 0, "the nearest break ahead is the target");

    SessionState just_short;
    just_short.block = 0;
    just_short.elapsed = 5;
    SessionAdvice short_advice = skip(just_short, blocks, 59, false);
    check(short_advice.seek_to == 60, "a break starting one second ahead is still the target, got " + describeAdvice(short_advice));
    checkState(just_short, 1, 0, "a break starting one second ahead is the target");

    SessionState at_start;
    at_start.block = 0;
    at_start.elapsed = 5;
    SessionAdvice at_start_advice = skip(at_start, blocks, 60, false);
    check(at_start_advice.seek_to == 160, "a position exactly at a break's start looks past it, got " + describeAdvice(at_start_advice));
    checkState(at_start, 3, 0, "a position exactly at a break's start lands on the later break");

    SessionState idle_at_start;
    SessionAdvice idle_advice = skip(idle_at_start, blocks, 60, false);
    check(idle_advice.seek_to == 160, "with no block active a position at a break's start looks past it, got " + describeAdvice(idle_advice));
    checkState(idle_at_start, 3, 0, "with no block active a position at a break's start lands on the later break");

    SessionState inside_a_break_position;
    inside_a_break_position.block = 0;
    inside_a_break_position.elapsed = 5;
    SessionAdvice inside = skip(inside_a_break_position, blocks, 80, false);
    check(inside.seek_to == 160, "a position inside a break looks past that break, got " + describeAdvice(inside));
    checkState(inside_a_break_position, 3, 0, "a position inside a break lands on the next break");

    SessionState behind;
    behind.block = 2;
    behind.elapsed = 5;
    SessionAdvice behind_advice = skip(behind, blocks, 120, true);
    check(behind_advice.seek_to == 160, "a break behind the position is never the target, got " + describeAdvice(behind_advice));
    checkState(behind, 3, 0, "a break behind the position is skipped over");
    checkResume(behind, 2, 5, 120, "a banked skip past an earlier break parks the current block");

    std::vector<Block> three_breaks = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true),
                                       block("Break 2", 100, 140, true), block("Pomodoro 2", 140, 200, false),
                                       block("Break 3", 200, 240, true)};
    SessionState many;
    many.block = 0;
    many.elapsed = 1;
    SessionAdvice many_advice = skip(many, three_breaks, 10, false);
    check(many_advice.seek_to == 60, "with three breaks ahead the first one wins, got " + describeAdvice(many_advice));
    checkState(many, 1, 0, "with three breaks ahead the first one is the target");

    SessionState many_mid;
    many_mid.block = 0;
    many_mid.elapsed = 1;
    SessionAdvice mid_advice = skip(many_mid, three_breaks, 60, false);
    check(mid_advice.seek_to == 100, "the next break after the one at the position is the target, got " + describeAdvice(mid_advice));
    checkState(many_mid, 2, 0, "the next break after the one at the position is taken");

    SessionState last_break;
    last_break.block = 3;
    last_break.elapsed = 1;
    SessionAdvice last_advice = skip(last_break, three_breaks, 140, false);
    check(last_advice.seek_to == 200, "the last break is reachable, got " + describeAdvice(last_advice));
    checkState(last_break, 4, 0, "the last break is reachable");
}

static void testSkipClearsElapsed()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 55;
    skip(state, blocks, 55, false);
    check(sameTime(state.elapsed, 0), "a skip clears the clock for the break, got " + describeState(state));
    check(remaining(state, blocks) == 40, "the break owes its whole length after a skip, got " + std::to_string(remaining(state, blocks)));

    SessionState banked;
    banked.block = 0;
    banked.elapsed = 55;
    skip(banked, blocks, 55, true);
    check(sameTime(banked.elapsed, 0), "a banked skip clears the clock for the break, got " + describeState(banked));
    check(remaining(banked, blocks) == 40, "the break owes its whole length after a banked skip, got " + std::to_string(remaining(banked, blocks)));
}

static void testAdvanceResumesAParkedBlock()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState state;
    state.block = 1;
    state.elapsed = 39.9;
    state.resume_block = 0;
    state.resume_elapsed = 25;
    state.resume_position = 25;
    SessionAdvice advice = advance(state, blocks, 70, 0.25, true);
    check(advice.block_done, "finishing a break with a parked block is still a finish, got " + describeAdvice(advice));
    check(advice.seek_to == 25, "finishing a break seeks back to the parked position, got " + describeAdvice(advice));
    checkState(state, 0, 25, "finishing a break returns to the parked block with its accrued time");
    checkNothingParked(state, "a resumed block clears the parked fields");
    check(remaining(state, blocks) == 35, "the resumed block owes only what was left, got " + std::to_string(remaining(state, blocks)));

    SessionState no_park;
    no_park.block = 1;
    no_park.elapsed = 39.9;
    SessionAdvice normal = advance(no_park, blocks, 70, 0.25, true);
    check(normal.block_done, "finishing a break without a parked block is a finish, got " + describeAdvice(normal));
    check(normal.seek_to == 100, "finishing a break without a parked block moves on, got " + describeAdvice(normal));
    checkState(no_park, 2, 0, "finishing a break without a parked block moves to the next one");

    SessionState bad_park;
    bad_park.block = 1;
    bad_park.elapsed = 39.9;
    bad_park.resume_block = 9;
    bad_park.resume_elapsed = 25;
    bad_park.resume_position = 25;
    SessionAdvice bad = advance(bad_park, blocks, 70, 0.25, true);
    check(bad.seek_to == 100, "a parked index past the end is not resumed, got " + describeAdvice(bad));
    checkState(bad_park, 2, 0, "a parked index past the end moves on normally");

    SessionState negative_park;
    negative_park.block = 1;
    negative_park.elapsed = 39.9;
    negative_park.resume_block = -4;
    SessionAdvice negative = advance(negative_park, blocks, 70, 0.25, true);
    check(negative.seek_to == 100, "a negative parked index is not resumed, got " + describeAdvice(negative));
    checkState(negative_park, 2, 0, "a negative parked index moves on normally");

    SessionState after_last;
    after_last.block = 2;
    after_last.elapsed = 59.9;
    after_last.resume_block = 0;
    after_last.resume_elapsed = 10;
    after_last.resume_position = 10;
    SessionAdvice last = advance(after_last, blocks, 130, 0.25, true);
    check(last.block_done, "finishing the last block with a parked block is a finish, got " + describeAdvice(last));
    check(last.seek_to == 10, "finishing the last block returns to the parked block, got " + describeAdvice(last));
    checkState(after_last, 0, 10, "a parked block is resumed even after the last block");
    checkNothingParked(after_last, "resuming after the last block clears the parked fields");
}

static void testParkedBlockResumedOnlyOnce()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 1;
    state.elapsed = 39.9;
    state.resume_block = 0;
    state.resume_elapsed = 50;
    state.resume_position = 50;
    SessionAdvice resumed = advance(state, blocks, 90, 0.25, true);
    check(resumed.seek_to == 50, "the parked block is resumed, got " + describeAdvice(resumed));
    checkState(state, 0, 50, "the parked block is resumed with its accrued time");
    checkNothingParked(state, "the parked fields are cleared on resume");

    state.elapsed = 59.9;
    SessionAdvice onwards = advance(state, blocks, 55, 0.25, true);
    check(onwards.block_done, "the resumed block finishes, got " + describeAdvice(onwards));
    check(onwards.seek_to == 60, "the resumed block moves on to the block after it, got " + describeAdvice(onwards));
    checkState(state, 1, 0, "the resumed block is not parked a second time");
    checkNothingParked(state, "nothing is parked after a resumed block finishes");

    state.elapsed = 39.9;
    SessionAdvice third = advance(state, blocks, 90, 0.25, true);
    check(third.seek_to == 100, "the following break moves on normally, got " + describeAdvice(third));
    checkState(state, 2, 0, "the following break moves on to the next work block");
}

static void testBankedSkipRoundTrip()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 20;

    SessionAdvice advice = skip(state, blocks, 20, true);
    check(advice.seek_to == 60, "the banked skip seeks to the break, got " + describeAdvice(advice));
    check(!advice.block_done, "the banked skip is not a finish, got " + describeAdvice(advice));
    checkResume(state, 0, 20, 20, "the banked skip parks the work block");

    RunResult run = playBlock(state, blocks, 1, 1.0, 0.25);
    check(run.finished, "the break runs to its end after a banked skip");
    check(run.real_seconds >= 39.5 && run.real_seconds <= 40.5,
          "the break takes its full length after a banked skip, got " + std::to_string(run.real_seconds) + "s");
    check(run.finish_seek == 20, "the break hands playback back to the parked position, got " + std::to_string(run.finish_seek));
    checkState(state, 0, 20, "the session returns to the parked work block");
    checkNothingParked(state, "the round trip clears the parked fields");
    check(remaining(state, blocks) == 40, "the resumed work block owes only the time still owed, got " + std::to_string(remaining(state, blocks)));

    RunResult rest_of_it = playBlock(state, blocks, 0, 1.0, 0.25);
    check(rest_of_it.finished, "the resumed work block finishes");
    check(rest_of_it.real_seconds >= 39.5 && rest_of_it.real_seconds <= 40.5,
          "the resumed work block takes only the owed time, got " + std::to_string(rest_of_it.real_seconds) + "s");
    check(rest_of_it.finish_seek == 60, "the resumed work block moves on to the break after it, got " + std::to_string(rest_of_it.finish_seek));
    checkState(state, 1, 0, "the resumed work block moves on to the next block");
    checkNothingParked(state, "nothing is parked once the round trip is over");

    RunResult second_break = playBlock(state, blocks, 1, 1.0, 0.25);
    check(second_break.finished, "the break after the resumed block finishes");
    check(second_break.finish_seek == 100, "the break after the resumed block moves on normally, got " + std::to_string(second_break.finish_seek));
    checkState(state, 2, 0, "the break after the resumed block does not loop back");
}

static void testUnbankedSkipDoesNotComeBack()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 30;

    SessionAdvice advice = skip(state, blocks, 30, false);
    check(advice.block_done, "the unbanked skip reports the abandoned block as done, got " + describeAdvice(advice));
    checkNothingParked(state, "the unbanked skip parks nothing");

    RunResult run = playBlock(state, blocks, 1, 1.0, 0.25);
    check(run.finished, "the break runs to its end after an unbanked skip");
    check(run.finish_seek == 100, "an unbanked skip does not come back to the abandoned block, got " + std::to_string(run.finish_seek));
    checkState(state, 2, 0, "an unbanked skip carries on to the block after the break");
    checkNothingParked(state, "nothing is parked after an unbanked skip's break");
    check(remaining(state, blocks) == 60, "the next work block owes its whole length, got " + std::to_string(remaining(state, blocks)));
}

static void testSkipOnTheRealVideo()
{
    std::vector<Block> blocks = realVideoBlocks();
    check(blocks.size() == 5, "the real video gives 5 blocks for skipping, got " + describe(blocks));

    SessionState state;
    state.block = 0;
    state.elapsed = 600;
    SessionAdvice advice = skip(state, blocks, 1000, false);
    check(advice.seek_to == 3067, "skipping from inside Pomodoro 1 lands on Guided Break 1 at 3067, got " + describeAdvice(advice));
    check(state.block == 1 && blocks.size() == 5 && blocks[state.block].label == "Guided Break 1",
          "skipping from inside Pomodoro 1 lands on Guided Break 1, got " + describeState(state));
    checkState(state, 1, 0, "skipping from inside Pomodoro 1 starts the break with a fresh clock");
    checkNothingParked(state, "an unbanked skip on the real video parks nothing");

    SessionState from_second;
    from_second.block = 2;
    from_second.elapsed = 100;
    SessionAdvice second = skip(from_second, blocks, 4000, false);
    check(second.seek_to == 6667, "skipping from inside Pomodoro 2 lands on Guided Break 2 at 6667, got " + describeAdvice(second));
    checkState(from_second, 3, 0, "skipping from inside Pomodoro 2 lands on Guided Break 2");

    SessionState from_third;
    from_third.block = 4;
    from_third.elapsed = 100;
    SessionAdvice third = skip(from_third, blocks, 8000, false);
    check(third.seek_to == 3067, "skipping from inside Pomodoro 3 goes back to Guided Break 1 at 3067, got " + describeAdvice(third));
    checkState(from_third, 1, 0, "skipping from inside Pomodoro 3 starts Guided Break 1");

    SessionState banked_third;
    banked_third.block = 4;
    banked_third.elapsed = 100;
    SessionAdvice banked_third_advice = skip(banked_third, blocks, 8000, true);
    check(banked_third_advice.seek_to == 3067, "a banked skip from inside Pomodoro 3 seeks to 3067, got " + describeAdvice(banked_third_advice));
    checkResume(banked_third, 4, 100, 8000, "a banked skip from inside Pomodoro 3 parks Pomodoro 3");

    RunResult third_break = playBlock(banked_third, blocks, 1, 1.0, 0.25);
    check(third_break.finished && third_break.finish_seek == 8000,
          "the break after Pomodoro 3 hands playback back to 8000, got " + std::to_string(third_break.finish_seek));
    checkState(banked_third, 4, 100, "the banked skip returns to Pomodoro 3 with its 100s already spent");

    SessionState banked;
    banked.block = 0;
    banked.elapsed = 1800;
    SessionAdvice banked_advice = skip(banked, blocks, 1867, true);
    check(banked_advice.seek_to == 3067, "the banked skip on the real video seeks to 3067, got " + describeAdvice(banked_advice));
    check(!banked_advice.block_done, "the banked skip on the real video is not a finish, got " + describeAdvice(banked_advice));
    checkState(banked, 1, 0, "the banked skip on the real video starts Guided Break 1");
    checkResume(banked, 0, 1800, 1867, "the banked skip on the real video parks Pomodoro 1");

    RunResult run = playBlock(banked, blocks, 1, 1.0, 0.25);
    check(run.finished, "Guided Break 1 runs to its end after the banked skip");
    check(run.real_seconds >= 599.5 && run.real_seconds <= 600.5,
          "Guided Break 1 takes its full 600s after the banked skip, got " + std::to_string(run.real_seconds) + "s");
    check(run.finish_seek == 1867, "Guided Break 1 hands playback back to 1867, got " + std::to_string(run.finish_seek));
    check(banked.block == 0 && blocks.size() == 5 && blocks[banked.block].label == "Pomodoro 1",
          "the banked skip returns to Pomodoro 1, got " + describeState(banked));
    checkState(banked, 0, 1800, "the banked skip returns to Pomodoro 1 with its 1800s already spent");
    checkNothingParked(banked, "the real video round trip clears the parked fields");
    check(remaining(banked, blocks) == 1200, "Pomodoro 1 owes 1200s after the banked break, got " + std::to_string(remaining(banked, blocks)));

    RunResult owed = playBlock(banked, blocks, 0, 1.0, 0.25);
    check(owed.finished, "the resumed Pomodoro 1 finishes");
    check(owed.real_seconds >= 1199.5 && owed.real_seconds <= 1200.5,
          "the resumed Pomodoro 1 takes only the owed 1200s, got " + std::to_string(owed.real_seconds) + "s");
    check(owed.finish_seek == 3067, "the resumed Pomodoro 1 moves on to Guided Break 1, got " + std::to_string(owed.finish_seek));
    checkState(banked, 1, 0, "the resumed Pomodoro 1 moves on rather than looping back");
}

static void testSessionVolumeWithoutSilence()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState work;
    work.block = 0;
    check(sessionVolume(work, blocks, false, 70) == 70,
          "without silence a work block plays at the wanted volume, got " + std::to_string(sessionVolume(work, blocks, false, 70)));

    SessionState rest;
    rest.block = 1;
    check(sessionVolume(rest, blocks, false, 70) == 70,
          "without silence a break plays at the wanted volume, got " + std::to_string(sessionVolume(rest, blocks, false, 70)));

    SessionState idle;
    check(sessionVolume(idle, blocks, false, 70) == 70,
          "without silence an idle session plays at the wanted volume, got " + std::to_string(sessionVolume(idle, blocks, false, 70)));

    check(sessionVolume(work, blocks, false, 0) == 0, "without silence a wanted 0 comes back as 0, got " + std::to_string(sessionVolume(work, blocks, false, 0)));
    check(sessionVolume(work, blocks, false, 100) == 100,
          "without silence a wanted 100 comes back as 100, got " + std::to_string(sessionVolume(work, blocks, false, 100)));

    std::vector<Block> none;
    check(sessionVolume(work, none, false, 55) == 55,
          "without silence and with no blocks the wanted volume comes back, got " + std::to_string(sessionVolume(work, none, false, 55)));
}

static void testSessionVolumeSilencesWork()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState first;
    first.block = 0;
    check(sessionVolume(first, blocks, true, 70) == 0, "silence mutes a work block, got " + std::to_string(sessionVolume(first, blocks, true, 70)));

    SessionState second;
    second.block = 2;
    check(sessionVolume(second, blocks, true, 70) == 0,
          "silence mutes the second work block too, got " + std::to_string(sessionVolume(second, blocks, true, 70)));

    first.elapsed = 59.9;
    check(sessionVolume(first, blocks, true, 70) == 0,
          "silence mutes a work block however far into it, got " + std::to_string(sessionVolume(first, blocks, true, 70)));

    check(sessionVolume(first, blocks, true, 0) == 0, "a muted work block with a wanted 0 is still 0, got " + std::to_string(sessionVolume(first, blocks, true, 0)));
    check(sessionVolume(first, blocks, true, 100) == 0,
          "a muted work block with a wanted 100 is still 0, got " + std::to_string(sessionVolume(first, blocks, true, 100)));
}

static void testSessionVolumeOnBreaks()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState rest;
    rest.block = 1;
    check(sessionVolume(rest, blocks, true, 70) == 70, "silence leaves a break alone, got " + std::to_string(sessionVolume(rest, blocks, true, 70)));
    check(sessionVolume(rest, blocks, true, 0) == 0, "a break with a wanted 0 stays 0, got " + std::to_string(sessionVolume(rest, blocks, true, 0)));
    check(sessionVolume(rest, blocks, true, 100) == 100, "a break with a wanted 100 stays 100, got " + std::to_string(sessionVolume(rest, blocks, true, 100)));

    std::vector<Block> every_kind = breakBlocks();
    SessionState later;
    later.block = 3;
    check(sessionVolume(later, every_kind, true, 42) == 42,
          "silence leaves a later break alone, got " + std::to_string(sessionVolume(later, every_kind, true, 42)));
}

static void testSessionVolumeOutsideABlock()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState idle;
    check(sessionVolume(idle, blocks, true, 70) == 70, "an intro plays at normal volume, got " + std::to_string(sessionVolume(idle, blocks, true, 70)));

    SessionState past_end;
    past_end.block = 3;
    check(sessionVolume(past_end, blocks, true, 70) == 70,
          "a block index at the end of the list plays at normal volume, got " + std::to_string(sessionVolume(past_end, blocks, true, 70)));

    SessionState far_past;
    far_past.block = 99;
    check(sessionVolume(far_past, blocks, true, 70) == 70,
          "a block index far past the end plays at normal volume, got " + std::to_string(sessionVolume(far_past, blocks, true, 70)));

    SessionState negative;
    negative.block = -4;
    check(sessionVolume(negative, blocks, true, 70) == 70,
          "a negative block index plays at normal volume, got " + std::to_string(sessionVolume(negative, blocks, true, 70)));

    std::vector<Block> none;
    SessionState on_nothing;
    on_nothing.block = 0;
    check(sessionVolume(on_nothing, none, true, 70) == 70,
          "with no blocks there is nothing to be silent for, got " + std::to_string(sessionVolume(on_nothing, none, true, 70)));
}

static void testSessionVolumeNeverClamps()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState rest;
    rest.block = 1;
    check(sessionVolume(rest, blocks, true, 150) == 150, "a wanted 150 on a break comes back as 150, got " + std::to_string(sessionVolume(rest, blocks, true, 150)));
    check(sessionVolume(rest, blocks, true, -5) == -5, "a wanted -5 on a break comes back as -5, got " + std::to_string(sessionVolume(rest, blocks, true, -5)));

    SessionState work;
    work.block = 0;
    check(sessionVolume(work, blocks, false, 150) == 150, "a wanted 150 without silence comes back as 150, got " + std::to_string(sessionVolume(work, blocks, false, 150)));
    check(sessionVolume(work, blocks, false, -5) == -5, "a wanted -5 without silence comes back as -5, got " + std::to_string(sessionVolume(work, blocks, false, -5)));

    SessionState idle;
    check(sessionVolume(idle, blocks, true, 150) == 150, "a wanted 150 outside a block comes back as 150, got " + std::to_string(sessionVolume(idle, blocks, true, 150)));
}

static void testSessionVolumeThroughARun()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    std::vector<int> volumes;
    for (int i = 0; i < 5; ++i) {
        volumes.push_back(sessionVolume(state, blocks, true, 80));
        playBlock(state, blocks, i, 1.0, 0.25);
        if (state.block != i + 1 && i < 4)
            break;
    }
    check(volumes.size() == 5, "a full run visits all five blocks, got " + std::to_string(volumes.size()));
    if (volumes.size() == 5) {
        check(volumes[0] == 0, "the first work block is silent, got " + std::to_string(volumes[0]));
        check(volumes[1] == 80, "the first break plays, got " + std::to_string(volumes[1]));
        check(volumes[2] == 0, "the second work block is silent, got " + std::to_string(volumes[2]));
        check(volumes[3] == 80, "the second break plays, got " + std::to_string(volumes[3]));
        check(volumes[4] == 0, "the third work block is silent, got " + std::to_string(volumes[4]));
    }
    check(sessionVolume(state, blocks, true, 80) == 80, "the volume comes back once the session ends, got " + std::to_string(sessionVolume(state, blocks, true, 80)));
}

static void testSessionVolumeAfterASkip()
{
    std::vector<Block> blocks = breakBlocks();
    SessionState state;
    state.block = 0;
    state.elapsed = 30;
    check(sessionVolume(state, blocks, true, 90) == 0, "the work block before a skip is silent, got " + std::to_string(sessionVolume(state, blocks, true, 90)));

    skip(state, blocks, 30, true);
    check(sessionVolume(state, blocks, true, 90) == 90, "a skip to a break brings the music back, got " + std::to_string(sessionVolume(state, blocks, true, 90)));

    playBlock(state, blocks, 1, 1.0, 0.25);
    check(sessionVolume(state, blocks, true, 90) == 0, "returning to the parked work block goes silent again, got " + std::to_string(sessionVolume(state, blocks, true, 90)));

    SessionState idle;
    skip(idle, blocks, 0, false);
    check(sessionVolume(idle, blocks, true, 90) == 90, "a skip from the intro to a break plays, got " + std::to_string(sessionVolume(idle, blocks, true, 90)));
}

static void testLoopTargetOnEveryShapeOfList()
{
    std::vector<Block> none;
    check(loopTarget(none) == -1, "an empty list has no loop target, got " + std::to_string(loopTarget(none)));

    std::vector<Block> one_work = {block("Pomodoro 1", 0, 60, false)};
    check(loopTarget(one_work) == 0, "a lone work block is its own loop target, got " + std::to_string(loopTarget(one_work)));

    std::vector<Block> one_rest = {block("Break 1", 0, 40, true)};
    check(loopTarget(one_rest) == 0, "a lone rest block is the loop target, got " + std::to_string(loopTarget(one_rest)));

    std::vector<Block> all_work = {block("Pomodoro 1", 0, 60, false), block("Pomodoro 2", 60, 120, false),
                                   block("Pomodoro 3", 120, 180, false)};
    check(loopTarget(all_work) == 0, "with no rest block the loop target is the first block, got " + std::to_string(loopTarget(all_work)));

    std::vector<Block> rest_first = {block("Break 1", 0, 40, true), block("Pomodoro 1", 40, 100, false)};
    check(loopTarget(rest_first) == 0, "a rest block first is the loop target, got " + std::to_string(loopTarget(rest_first)));

    check(loopTarget(threeBlocks()) == 1, "the loop target of the three block list is 1, got " + std::to_string(loopTarget(threeBlocks())));
    check(loopTarget(breakBlocks()) == 1, "the loop target of the break list is 1, got " + std::to_string(loopTarget(breakBlocks())));

    std::vector<Block> rest_late = {block("Pomodoro 1", 0, 60, false), block("Pomodoro 2", 60, 120, false),
                                    block("Break 1", 120, 160, true), block("Pomodoro 3", 160, 220, false),
                                    block("Break 2", 220, 260, true)};
    check(loopTarget(rest_late) == 2, "the first rest block wins however late it comes, got " + std::to_string(loopTarget(rest_late)));

    std::vector<Block> real = realVideoBlocks();
    check(loopTarget(real) == 1, "the real video loops back to Guided Break 1 at index 1, got " + std::to_string(loopTarget(real)));
    check(loopTarget(real) == 1 && real.size() == 5 && real[loopTarget(real)].label == "Guided Break 1",
          "the real video's loop target is Guided Break 1, got " + describe(real));
}

static void testLoopingOffEndsTheSession()
{
    std::vector<Block> blocks = threeBlocks();

    SessionState state;
    state.block = 2;
    state.elapsed = 59.9;
    SessionAdvice advice = advance(state, blocks, 130, 0.25, true, false);
    check(advice.block_done, "the last block still finishes with looping off, got " + describeAdvice(advice));
    check(advice.seek_to == -1, "with looping off there is nowhere to seek after the last block, got " + describeAdvice(advice));
    checkState(state, -1, 0, "with looping off the session ends after the last block");

    SessionState omitted;
    omitted.block = 2;
    omitted.elapsed = 59.9;
    SessionAdvice old_style = advance(omitted, blocks, 130, 0.25, true);
    check(old_style.block_done && old_style.seek_to == -1, "omitting the loop argument means no looping, got " + describeAdvice(old_style));
    checkState(omitted, -1, 0, "omitting the loop argument ends the session");
}

static void testLoopingAtTheEnd()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false), block("Pomodoro 2", 60, 120, false),
                                 block("Break 1", 120, 160, true), block("Pomodoro 3", 160, 220, false)};

    SessionState last;
    last.block = 3;
    last.elapsed = 59.9;
    SessionAdvice advice = advance(last, blocks, 200, 0.25, true, true);
    check(advice.block_done, "finishing the last block with looping on is still a finish, got " + describeAdvice(advice));
    check(advice.seek_to == 120, "looping seeks to the loop target's start, got " + describeAdvice(advice));
    checkState(last, 2, 0, "looping goes to the loop target with a fresh clock");
    check(remaining(last, blocks) == 40, "the looped-to block owes its whole length, got " + std::to_string(remaining(last, blocks)));

    SessionState middle;
    middle.block = 0;
    middle.elapsed = 59.9;
    SessionAdvice onwards = advance(middle, blocks, 30, 0.25, true, true);
    check(onwards.block_done, "a middle block still finishes with looping on, got " + describeAdvice(onwards));
    check(onwards.seek_to == 60, "looping does not divert a middle block, got " + describeAdvice(onwards));
    checkState(middle, 1, 0, "finishing a middle block with looping on moves to the next one");

    SessionState before_the_target;
    before_the_target.block = 1;
    before_the_target.elapsed = 59.9;
    SessionAdvice to_rest = advance(before_the_target, blocks, 90, 0.25, true, true);
    check(to_rest.seek_to == 120, "the block before the rest block moves on normally, got " + describeAdvice(to_rest));
    checkState(before_the_target, 2, 0, "the block before the rest block moves to it as usual");

    std::vector<Block> all_work = {block("Pomodoro 1", 100, 160, false), block("Pomodoro 2", 160, 220, false)};
    SessionState no_rest;
    no_rest.block = 1;
    no_rest.elapsed = 59.9;
    SessionAdvice to_first = advance(no_rest, all_work, 200, 0.25, true, true);
    check(to_first.block_done, "finishing the last block of an all-work list is a finish, got " + describeAdvice(to_first));
    check(to_first.seek_to == 100, "with no rest block looping goes back to the first block at 100, got " + describeAdvice(to_first));
    checkState(no_rest, 0, 0, "with no rest block looping goes back to block 0");

    std::vector<Block> alone = {block("Pomodoro 1", 100, 160, false)};
    SessionState only;
    only.block = 0;
    only.elapsed = 59.9;
    SessionAdvice again = advance(only, alone, 150, 0.25, true, true);
    check(again.block_done, "a one-block list finishes its block, got " + describeAdvice(again));
    check(again.seek_to == 100, "a one-block list loops back onto itself at 100, got " + describeAdvice(again));
    checkState(only, 0, 0, "a one-block list loops back onto itself with a fresh clock");

    std::vector<Block> rest_alone = {block("Guided Break 1", 60, 100, true)};
    SessionState rest_only;
    rest_only.block = 0;
    rest_only.elapsed = 39.9;
    SessionAdvice rest_again = advance(rest_only, rest_alone, 90, 0.25, true, true);
    check(rest_again.seek_to == 60, "a one-rest-block list loops back onto itself at 60, got " + describeAdvice(rest_again));
    checkState(rest_only, 0, 0, "a one-rest-block list loops back onto itself");

    std::vector<Block> empty;
    SessionState nothing;
    SessionAdvice no_advice = advance(nothing, empty, 0, 0.25, true, true);
    checkEmptyAdvice(no_advice, "looping with no blocks advises nothing");
    checkState(nothing, -1, 0, "looping with no blocks leaves the state alone");
}

static void testLoopingWithAParkedBlock()
{
    std::vector<Block> blocks = {block("Pomodoro 1", 0, 60, false), block("Break 1", 60, 100, true),
                                 block("Pomodoro 2", 100, 160, false)};

    SessionState parked;
    parked.block = 2;
    parked.elapsed = 59.9;
    parked.resume_block = 0;
    parked.resume_elapsed = 25;
    parked.resume_position = 25;
    SessionAdvice advice = advance(parked, blocks, 130, 0.25, true, true);
    check(advice.block_done, "finishing the last block with a parked block is a finish while looping, got " + describeAdvice(advice));
    check(advice.seek_to == 25, "a parked block beats the loop target, got " + describeAdvice(advice));
    checkState(parked, 0, 25, "the parked block comes back with its accrued time rather than the loop target");
    checkNothingParked(parked, "handing the parked block back while looping clears the parked fields");

    SessionState bad_park;
    bad_park.block = 2;
    bad_park.elapsed = 59.9;
    bad_park.resume_block = 9;
    bad_park.resume_elapsed = 25;
    bad_park.resume_position = 25;
    SessionAdvice bad = advance(bad_park, blocks, 130, 0.25, true, true);
    check(bad.seek_to == 60, "a parked index past the end falls through to the loop target, got " + describeAdvice(bad));
    checkState(bad_park, 1, 0, "a parked index past the end loops instead");

    SessionState negative_park;
    negative_park.block = 2;
    negative_park.elapsed = 59.9;
    negative_park.resume_block = -4;
    SessionAdvice negative = advance(negative_park, blocks, 130, 0.25, true, true);
    check(negative.seek_to == 60, "a negative parked index falls through to the loop target, got " + describeAdvice(negative));
    checkState(negative_park, 1, 0, "a negative parked index loops instead");

    SessionState after_the_loop;
    after_the_loop.block = 2;
    after_the_loop.elapsed = 59.9;
    after_the_loop.resume_block = 0;
    after_the_loop.resume_elapsed = 50;
    after_the_loop.resume_position = 50;
    advance(after_the_loop, blocks, 130, 0.25, true, true);
    after_the_loop.elapsed = 59.9;
    SessionAdvice second = advance(after_the_loop, blocks, 55, 0.25, true, true);
    check(second.seek_to == 60, "the block handed back while looping moves on normally, got " + describeAdvice(second));
    checkState(after_the_loop, 1, 0, "the block handed back while looping is not parked again");
}

static void testLoopingNeverEnds()
{
    std::vector<Block> blocks = realVideoBlocks();
    check(blocks.size() == 5, "the looping run has 5 blocks to work with, got " + describe(blocks));

    SessionState state;
    state.block = 4;
    std::string visited;
    bool ended = false;
    for (int step = 0; step < 9; ++step) {
        int index = state.block;
        if (index < 0 || index >= static_cast<int>(blocks.size())) {
            ended = true;
            break;
        }
        visited += blocks[index].label + " ";
        RunResult run = playBlock(state, blocks, index, 1.0, 0.25, true);
        check(run.finished, "block " + std::to_string(index) + " of the looping run finishes");
        if (state.block < 0) {
            ended = true;
            break;
        }
        check(run.finish_seek == blocks[state.block].start,
              "the looping run seeks to the start of the block it moves to, got " + std::to_string(run.finish_seek));
    }
    check(!ended, "a looping session never ends, visited " + visited);
    check(visited == "Pomodoro 3 Guided Break 1 Pomodoro 2 Guided Break 2 Pomodoro 3 Guided Break 1 Pomodoro 2 Guided Break 2 Pomodoro 3 ",
          "looping runs Pomodoro 3, Guided Break 1, Pomodoro 2, Guided Break 2 round and round, visited " + visited);
    checkState(state, 1, 0, "the looping run is back on Guided Break 1 after two rounds");
    checkNothingParked(state, "the looping run parks nothing");
}

static void checkFadeWalk(int from, int target, int step, const std::string& name)
{
    int current = from;
    bool overshot = false;
    bool wrong_way = false;
    int steps = 0;
    for (; steps < 100000; ++steps) {
        int next = fadeStep(current, target, step);
        if (from < target) {
            if (next > target)
                overshot = true;
            if (next < current)
                wrong_way = true;
        }
        if (from > target) {
            if (next < target)
                overshot = true;
            if (next > current)
                wrong_way = true;
        }
        if (next == current)
            break;
        current = next;
    }
    check(!overshot, name + " never overshoots the target, stopped at " + std::to_string(current));
    check(!wrong_way, name + " never moves away from the target, stopped at " + std::to_string(current));
    check(current == target, name + " reaches the target exactly, got " + std::to_string(current));
    check(fadeStep(current, target, step) == target, name + " stays at the target once it arrives, got " + std::to_string(fadeStep(current, target, step)));
    check(steps < 100000, name + " arrives in a finite number of steps");
}

static void testFadeStep()
{
    check(fadeStep(0, 100, 0) == 100, "a step of 0 goes straight to the target, got " + std::to_string(fadeStep(0, 100, 0)));
    check(fadeStep(100, 0, 0) == 0, "a step of 0 goes straight down to the target, got " + std::to_string(fadeStep(100, 0, 0)));
    check(fadeStep(0, 100, -5) == 100, "a negative step goes straight to the target, got " + std::to_string(fadeStep(0, 100, -5)));
    check(fadeStep(70, 70, 0) == 70, "a step of 0 at the target keeps it, got " + std::to_string(fadeStep(70, 70, 0)));

    check(fadeStep(0, 100, 10) == 10, "a fade up moves one step, got " + std::to_string(fadeStep(0, 100, 10)));
    check(fadeStep(95, 100, 10) == 100, "a fade up with less than a step left lands on the target, got " + std::to_string(fadeStep(95, 100, 10)));
    check(fadeStep(90, 100, 10) == 100, "a fade up with exactly a step left lands on the target, got " + std::to_string(fadeStep(90, 100, 10)));
    check(fadeStep(99, 100, 1) == 100, "a fade up of one lands on the target, got " + std::to_string(fadeStep(99, 100, 1)));

    check(fadeStep(100, 0, 10) == 90, "a fade down moves one step, got " + std::to_string(fadeStep(100, 0, 10)));
    check(fadeStep(5, 0, 10) == 0, "a fade down with less than a step left lands on the target, got " + std::to_string(fadeStep(5, 0, 10)));
    check(fadeStep(10, 0, 10) == 0, "a fade down with exactly a step left lands on the target, got " + std::to_string(fadeStep(10, 0, 10)));

    check(fadeStep(50, 50, 10) == 50, "a current already at the target stays, got " + std::to_string(fadeStep(50, 50, 10)));
    check(fadeStep(0, 0, 7) == 0, "a current and target of 0 stay at 0, got " + std::to_string(fadeStep(0, 0, 7)));

    check(fadeStep(-20, 0, 5) == -15, "a fade up from a negative current works, got " + std::to_string(fadeStep(-20, 0, 5)));
    check(fadeStep(-5, -100, 1000) == -100, "a huge step down to a negative target lands on it, got " + std::to_string(fadeStep(-5, -100, 1000)));
    check(fadeStep(140, 120, 7) == 133, "a fade down from above 100 works, got " + std::to_string(fadeStep(140, 120, 7)));
    check(fadeStep(200, 300, 1000) == 300, "a huge step up past 100 lands on the target, got " + std::to_string(fadeStep(200, 300, 1000)));
    check(fadeStep(100, 150, 10) == 110, "a fade up past 100 is not clamped, got " + std::to_string(fadeStep(100, 150, 10)));
    check(fadeStep(0, -50, 10) == -10, "a fade down below 0 is not clamped, got " + std::to_string(fadeStep(0, -50, 10)));

    for (int step : {1, 2, 3, 7, 10, 33, 100}) {
        checkFadeWalk(0, 100, step, "a walk up from 0 to 100 by " + std::to_string(step));
        checkFadeWalk(100, 0, step, "a walk down from 100 to 0 by " + std::to_string(step));
        checkFadeWalk(42, 73, step, "a walk up from 42 to 73 by " + std::to_string(step));
        checkFadeWalk(73, 42, step, "a walk down from 73 to 42 by " + std::to_string(step));
        checkFadeWalk(55, 55, step, "a walk that is already at 55 by " + std::to_string(step));
        checkFadeWalk(-50, -10, step, "a walk up from -50 to -10 by " + std::to_string(step));
        checkFadeWalk(150, 0, step, "a walk down from 150 to 0 by " + std::to_string(step));
        checkFadeWalk(0, 150, step, "a walk up from 0 to 150 by " + std::to_string(step));
    }
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
    testTrailingBreakIsNotABlock();
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
    testSkipWithNoBlocks();
    testSkipWhileOnABreak();
    testCuttingABreakShortWithAParkedBlock();
    testBothBankValuesAgreeOffABreak();
    testCuttingABreakShortTargetChoice();
    testCuttingABreakShortHandsBackOnlyOnce();
    testCutShortRoundTrip();
    testCutBreakShortOnTheRealVideo();
    testSkipWithNoBreakAhead();
    testSkipFromAWorkBlockUnbanked();
    testSkipFromAWorkBlockBanked();
    testSkipWithNoActiveBlock();
    testSkipTargetChoice();
    testSkipClearsElapsed();
    testAdvanceResumesAParkedBlock();
    testParkedBlockResumedOnlyOnce();
    testBankedSkipRoundTrip();
    testUnbankedSkipDoesNotComeBack();
    testSkipOnTheRealVideo();
    testSessionVolumeWithoutSilence();
    testSessionVolumeSilencesWork();
    testSessionVolumeOnBreaks();
    testSessionVolumeOutsideABlock();
    testSessionVolumeNeverClamps();
    testSessionVolumeThroughARun();
    testSessionVolumeAfterASkip();
    testLoopTargetOnEveryShapeOfList();
    testLoopingOffEndsTheSession();
    testLoopingAtTheEnd();
    testLoopingWithAParkedBlock();
    testLoopingNeverEnds();
    testFadeStep();
    return report();
}
