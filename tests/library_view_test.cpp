#include <climits>
#include <string>
#include <vector>

#include "check.h"
#include "library_view.h"

static Video makeVideo(const std::string& title, int duration, int position)
{
    Video video;
    video.id = "id" + std::to_string(duration) + "x" + std::to_string(position);
    video.url = "https://www.youtube.com/watch?v=" + video.id;
    video.title = title;
    video.file = "C:\\Videos\\Youtonomous\\" + video.id + ".mp4";
    video.duration = duration;
    video.position = position;
    return video;
}

static Video titled(const std::string& title)
{
    return makeVideo(title, 600, 0);
}

static std::string describe(const std::vector<int>& rows)
{
    std::string result = "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += std::to_string(rows[i]);
    }
    return result + "]";
}

static void expectRows(const std::vector<int>& got, const std::vector<int>& expected, const std::string& name)
{
    check(got == expected, name + ": expected " + describe(expected) + ", got " + describe(got));
}

static void expectPercent(int duration, int position, int expected, const std::string& name)
{
    int got = watchedPercent(makeVideo("Video", duration, position));
    check(got == expected, name + ": expected " + std::to_string(expected) + ", got " + std::to_string(got));
}

static std::vector<Video> realLibrary()
{
    return {
        makeVideo("The Feynman Technique", 121, 31),
        makeVideo("3-HOUR STUDY WITH ME | Hyper Efficient, Doctor, Focus Music, Pomodoro 50-10", 10303, 7300),
        makeVideo("1.5-HOUR STUDY WITH ME | Hyper Efficient, Doctor, Focus Music, Deep Work, Pomodoro 42-6-42", 5480, 0),
    };
}

static void testWatchedPercentBasics()
{
    expectPercent(100, 0, 0, "position 0 is 0 percent");
    expectPercent(100, 50, 50, "halfway is 50 percent");
    expectPercent(100, 100, 100, "the end is 100 percent");
    expectPercent(200, 1, 0, "half a percent rounds down to 0");
    expectPercent(3, 1, 33, "a third rounds down to 33");
    expectPercent(3, 2, 66, "two thirds rounds down to 66");
    expectPercent(1, 0, 0, "a one second video at 0 is 0 percent");
    expectPercent(1, 1, 100, "a one second video at 1 is 100 percent");
    expectPercent(1000, 999, 99, "one second short of the end is 99 percent");
}

static void testWatchedPercentBadDuration()
{
    expectPercent(0, 0, 0, "zero duration is 0 percent");
    expectPercent(0, 50, 0, "zero duration with a position is 0 percent");
    expectPercent(-10, 5, 0, "negative duration is 0 percent");
    expectPercent(-10, -20, 0, "negative duration and position is 0 percent");
    expectPercent(INT_MIN, INT_MAX, 0, "the most negative duration is 0 percent");
}

static void testWatchedPercentClamped()
{
    expectPercent(100, -1, 0, "a negative position is 0 percent");
    expectPercent(100, -500, 0, "a large negative position is 0 percent");
    expectPercent(100, INT_MIN, 0, "the most negative position is 0 percent");
    expectPercent(100, 101, 100, "a position just past the end is 100 percent");
    expectPercent(100, 5000, 100, "a position far past the end is 100 percent");
    expectPercent(1, INT_MAX, 100, "the largest position on a one second video is 100 percent");
}

static void testWatchedPercentNoOverflow()
{
    expectPercent(10303, 10302, 99, "one second short of a long video is 99 percent");
    expectPercent(10303, 7300, 70, "7300 of 10303 is 70 percent");
    expectPercent(2000000000, 1999999999, 99, "one second short of a huge video is 99 percent");
    expectPercent(2000000000, 1000000000, 50, "halfway through a huge video is 50 percent");
    expectPercent(2000000000, 2000000000, 100, "the end of a huge video is 100 percent");
    expectPercent(INT_MAX, INT_MAX - 1, 99, "one second short of the largest duration is 99 percent");
    expectPercent(INT_MAX, INT_MAX, 100, "the end of the largest duration is 100 percent");
    expectPercent(30000000, 29999999, 99, "a position that overflows times 100 is 99 percent");
    expectPercent(30000000, 15000000, 50, "halfway where times 100 overflows is 50 percent");
}

static void testWatchedPercentRealLibrary()
{
    std::vector<Video> videos = realLibrary();
    int feynman = watchedPercent(videos[0]);
    int three_hour = watchedPercent(videos[1]);
    int ninety_minute = watchedPercent(videos[2]);
    check(feynman == 25, "The Feynman Technique is 25 percent watched, got " + std::to_string(feynman));
    check(three_hour == 70, "the 3-hour video is 70 percent watched, got " + std::to_string(three_hour));
    check(ninety_minute == 0, "the 1.5-hour video is 0 percent watched, got " + std::to_string(ninety_minute));
}

static void testWatchedPercentIgnoresOtherFields()
{
    Video video = makeVideo("Video", 200, 50);
    video.start = 150;
    Bookmark mark;
    mark.time = 190;
    mark.label = "End";
    video.marks.push_back(mark);
    int got = watchedPercent(video);
    check(got == 25, "start and marks do not change the percent, got " + std::to_string(got));
}

static void testEmptySearchMatchesEverything()
{
    Video video = titled("The Feynman Technique");
    Video untitled = titled("");
    check(matchesSearch(video, ""), "an empty search matches a video");
    check(matchesSearch(untitled, ""), "an empty search matches an untitled video");
    check(matchesSearch(video, " "), "a single space matches a video");
    check(matchesSearch(video, "\t"), "a single tab matches a video");
    check(matchesSearch(video, "   \t  \t "), "spaces and tabs only match a video");
    check(matchesSearch(untitled, " \t "), "spaces and tabs only match an untitled video");
}

static void testSingleWordSearch()
{
    Video video = titled("The Feynman Technique");
    check(matchesSearch(video, "Feynman"), "a whole word in the title matches");
    check(matchesSearch(video, "eynm"), "part of a word matches");
    check(matchesSearch(video, "n Tech"), "a search cannot hold a space, so this is two words that both match");
    check(matchesSearch(video, "The"), "the first word matches");
    check(matchesSearch(video, "Technique"), "the last word matches");
    check(matchesSearch(video, "The Feynman Technique"), "the whole title matches");
    check(!matchesSearch(video, "Pomodoro"), "a word not in the title does not match");
    check(!matchesSearch(video, "Techniques"), "a word longer than the title word does not match");
    check(!matchesSearch(video, "FeynmanTechnique"), "words joined without the space do not match");
    check(!matchesSearch(titled(""), "a"), "a word does not match an empty title");
}

static void testSearchIgnoresAsciiCase()
{
    Video video = titled("The Feynman Technique");
    check(matchesSearch(video, "feynman"), "lowercase search matches a capitalised title");
    check(matchesSearch(video, "FEYNMAN"), "uppercase search matches a capitalised title");
    check(matchesSearch(video, "fEyNmAn"), "mixed case search matches");
    check(matchesSearch(titled("3-HOUR STUDY WITH ME"), "study"), "lowercase search matches an uppercase title");
    check(matchesSearch(titled("lower case"), "LOWER CASE"), "uppercase search matches a lowercase title");
    check(matchesSearch(titled("[Lo-Fi] @Home_Z"), "[lo-fi] @home_z"), "punctuation is matched as is next to folded letters");
    check(!matchesSearch(titled("a_b"), "a?b"), "case folding does not make punctuation match other punctuation");
    check(!matchesSearch(titled("@"), "`"), "@ and ` are not folded into each other");
    check(!matchesSearch(titled("["), "{"), "[ and { are not folded into each other");
}

static void testSearchUtf8()
{
    Video video = titled("Café Étude über Straße");
    check(matchesSearch(video, "Café"), "a UTF-8 word matches exactly");
    check(matchesSearch(video, "café"), "ASCII letters of a UTF-8 word fold, the accent is exact");
    check(matchesSearch(video, "CAF"), "the ASCII part of a UTF-8 word matches in any case");
    check(matchesSearch(video, "Étude"), "a word starting with a UTF-8 capital matches exactly");
    check(matchesSearch(video, "ÉTUDE"), "ASCII letters after a UTF-8 capital fold");
    check(!matchesSearch(video, "étude"), "a UTF-8 lowercase letter does not match its capital");
    check(!matchesSearch(video, "CAFÉ"), "a UTF-8 capital does not match its lowercase");
    check(!matchesSearch(video, "Uber"), "a plain U does not match ü");
    check(!matchesSearch(video, "ÜBER"), "Ü does not match ü");
    check(matchesSearch(video, "straße"), "ß matches itself");
    check(!matchesSearch(video, "strasse"), "ss does not match ß");
    check(matchesSearch(titled("日本語の勉強"), "日本語"), "a CJK word matches");
    check(!matchesSearch(titled("日本語の勉強"), "中文"), "a different CJK word does not match");
    check(matchesSearch(titled("Focus 🎧 Music"), "🎧"), "an emoji matches");
    check(matchesSearch(titled("Focus 🎧 Music"), "🎧 focus"), "an emoji with an ASCII word matches");
}

static void testMultiWordSearch()
{
    Video video = titled("3-HOUR STUDY WITH ME | Hyper Efficient, Doctor, Focus Music, Pomodoro 50-10");
    check(matchesSearch(video, "study pomodoro"), "two words in the title match");
    check(matchesSearch(video, "pomodoro study"), "word order does not matter");
    check(matchesSearch(video, "focus music hyper doctor"), "four words in any order match");
    check(!matchesSearch(video, "study feynman"), "one missing word stops the match");
    check(!matchesSearch(video, "feynman study"), "one missing first word stops the match");
    check(matchesSearch(video, "study study study"), "a repeated word matches");
    check(matchesSearch(video, "pomo modoro"), "overlapping words both match");
    check(matchesSearch(video, "o o o"), "a single letter repeated matches");
    check(matchesSearch(video, "50-10 |"), "punctuation words match");
    check(matchesSearch(video, "efficient,"), "a word with its comma matches");
    check(!matchesSearch(video, "50-10,"), "a comma that is not in the title does not match");
}

static void testSearchWhitespace()
{
    Video video = titled("The Feynman Technique");
    check(matchesSearch(video, "  feynman"), "leading spaces are ignored");
    check(matchesSearch(video, "feynman  "), "trailing spaces are ignored");
    check(matchesSearch(video, "feynman     technique"), "several spaces between words are one gap");
    check(matchesSearch(video, "feynman\ttechnique"), "a tab separates words");
    check(matchesSearch(video, "\tfeynman \t technique\t"), "mixed tabs and spaces separate words");
    check(!matchesSearch(video, "\tfeynman \t pomodoro\t"), "a missing word after tabs stops the match");
    check(matchesSearch(titled("a\tb"), "a"), "a tab in the title does not stop a match");
}

static void testSearchOnlyTitle()
{
    Video video = titled("The Feynman Technique");
    video.id = "abc123XYZ";
    video.url = "https://www.youtube.com/watch?v=abc123XYZ";
    video.file = "C:\\Videos\\Youtonomous\\abc123XYZ.mp4";
    check(!matchesSearch(video, "abc123XYZ"), "the id is not searched");
    check(!matchesSearch(video, "youtube"), "the url is not searched");
    check(!matchesSearch(video, "mp4"), "the file is not searched");
    check(!matchesSearch(video, "Videos"), "the folder is not searched");
    check(!matchesSearch(video, "feynman abc123XYZ"), "a title word with an id word does not match");
}

static void testSearchRealLibrary()
{
    std::vector<Video> videos = realLibrary();
    check(matchesSearch(videos[0], "feynman"), "feynman matches The Feynman Technique");
    check(!matchesSearch(videos[1], "feynman"), "feynman does not match the 3-hour video");
    check(matchesSearch(videos[1], "hour study"), "hour study matches the 3-hour video");
    check(matchesSearch(videos[2], "hour study"), "hour study matches the 1.5-hour video");
    check(!matchesSearch(videos[0], "hour study"), "hour study does not match The Feynman Technique");
    check(matchesSearch(videos[2], "deep work"), "deep work matches the 1.5-hour video");
    check(!matchesSearch(videos[1], "deep work"), "deep work does not match the 3-hour video");
    check(matchesSearch(videos[2], "42-6-42"), "42-6-42 matches the 1.5-hour video");
    check(!matchesSearch(videos[1], "1.5-hour"),"1.5-hour does not match the 3-hour video");
    check(matchesSearch(videos[2], "1.5-hour"), "1.5-hour matches the 1.5-hour video");
}

static void testEmptyLibrary()
{
    std::vector<Video> videos;
    SortBy sorts[] = {SortBy::added, SortBy::title, SortBy::length, SortBy::watched};
    const char* names[] = {"added", "title", "length", "watched"};
    for (int i = 0; i < 4; ++i) {
        for (bool descending : {false, true}) {
            std::string direction = descending ? " descending" : " ascending";
            expectRows(libraryRows(videos, "", sorts[i], descending), {},
                       std::string("an empty library gives no rows sorted by ") + names[i] + direction);
            expectRows(libraryRows(videos, "feynman", sorts[i], descending), {},
                       std::string("an empty library with a search gives no rows sorted by ") + names[i] + direction);
        }
    }
}

static void testSearchMatchingNothing()
{
    std::vector<Video> videos = realLibrary();
    SortBy sorts[] = {SortBy::added, SortBy::title, SortBy::length, SortBy::watched};
    const char* names[] = {"added", "title", "length", "watched"};
    for (int i = 0; i < 4; ++i) {
        for (bool descending : {false, true}) {
            std::string direction = descending ? " descending" : " ascending";
            expectRows(libraryRows(videos, "calculus", sorts[i], descending), {},
                       std::string("a search matching nothing gives no rows sorted by ") + names[i] + direction);
        }
    }
}

static void testSingleVideo()
{
    std::vector<Video> videos = {makeVideo("Only", 0, 0)};
    expectRows(libraryRows(videos, "", SortBy::added, false), {0}, "one video sorted by added");
    expectRows(libraryRows(videos, "", SortBy::added, true), {0}, "one video sorted by added descending");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0}, "one video sorted by title descending");
    expectRows(libraryRows(videos, "", SortBy::length, false), {0}, "one video with zero duration sorted by length");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {0}, "one video with zero duration sorted by watched descending");
    expectRows(libraryRows(videos, "ONLY", SortBy::added, false), {0}, "one matching video is found");
    expectRows(libraryRows(videos, "other", SortBy::added, false), {}, "one video that does not match gives no rows");
}

static void testSortByAdded()
{
    std::vector<Video> videos = {titled("Delta"), titled("alpha"), titled("Charlie"), titled("bravo")};
    expectRows(libraryRows(videos, "", SortBy::added, false), {0, 1, 2, 3}, "added keeps library order");
    expectRows(libraryRows(videos, "", SortBy::added, true), {3, 2, 1, 0}, "added descending reverses library order");
    expectRows(libraryRows(videos, "a", SortBy::added, false), {0, 1, 2, 3}, "a search matching every video keeps library order");
    expectRows(libraryRows(videos, "l", SortBy::added, false), {0, 1, 2}, "added skips videos that do not match");
    expectRows(libraryRows(videos, "l", SortBy::added, true), {2, 1, 0}, "added descending skips videos that do not match");
    expectRows(libraryRows(videos, "  \t ", SortBy::added, true), {3, 2, 1, 0}, "a whitespace search shows every video");
}

static void testSortByAddedSameTitles()
{
    std::vector<Video> videos = {titled("Same"), titled("Same"), titled("Same")};
    expectRows(libraryRows(videos, "", SortBy::added, false), {0, 1, 2}, "identical titles keep library order");
    expectRows(libraryRows(videos, "", SortBy::added, true), {2, 1, 0}, "identical titles reverse under added descending");
}

static void testSortByTitle()
{
    std::vector<Video> videos = {titled("cherry"), titled("Banana"), titled("apple"), titled("Date")};
    expectRows(libraryRows(videos, "", SortBy::title, false), {2, 1, 0, 3}, "title sorts alphabetically ignoring case");
    expectRows(libraryRows(videos, "", SortBy::title, true), {3, 0, 1, 2}, "title descending sorts in reverse");
    expectRows(libraryRows(videos, "an", SortBy::title, false), {1}, "title sort with a search shows only matches");
    expectRows(libraryRows(videos, "e", SortBy::title, false), {2, 0, 3}, "title sort skips videos that do not match");
    expectRows(libraryRows(videos, "e", SortBy::title, true), {3, 0, 2}, "title descending skips videos that do not match");
}

static void testSortByTitleFoldsToLowercase()
{
    std::vector<Video> videos = {titled("Zeta"), titled("_note"), titled("alpha")};
    expectRows(libraryRows(videos, "", SortBy::title, false), {1, 2, 0}, "an underscore sorts before letters once letters fold to lowercase");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0, 2, 1}, "an underscore sorts after letters in descending title order");

    std::vector<Video> brackets = {titled("B"), titled("[x"), titled("a")};
    expectRows(libraryRows(brackets, "", SortBy::title, false), {1, 2, 0}, "[ sorts before folded letters");

    std::vector<Video> braces = {titled("{x"), titled("Z"), titled("a")};
    expectRows(libraryRows(braces, "", SortBy::title, false), {2, 1, 0}, "{ sorts after folded letters");
}

static void testSortByTitlePrefixesAndDigits()
{
    std::vector<Video> videos = {titled("abcd"), titled("ABC"), titled(""), titled("ab"), titled("10"), titled("9"), titled(" lead")};
    expectRows(libraryRows(videos, "", SortBy::title, false), {2, 6, 4, 5, 3, 1, 0},
               "the empty title comes first, then space, digits, then shorter prefixes before longer");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0, 1, 3, 5, 4, 6, 2}, "the same titles in descending order");
}

static void testSortByTitleUtf8()
{
    std::vector<Video> videos = {titled("émile"), titled("zed"), titled("Émile"), titled("Apple")};
    expectRows(libraryRows(videos, "", SortBy::title, false), {3, 1, 2, 0},
               "UTF-8 titles sort after ASCII by byte and É sorts before é");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0, 2, 1, 3}, "UTF-8 titles in descending order");

    std::vector<Video> accents = {titled("Ébc"), titled("éaa")};
    expectRows(libraryRows(accents, "", SortBy::title, false), {0, 1}, "UTF-8 letters are not folded when sorting");
}

static void testSortByTitleTies()
{
    std::vector<Video> videos = {titled("same"), titled("Other"), titled("SAME"), titled("Same"), titled("other")};
    expectRows(libraryRows(videos, "", SortBy::title, false), {1, 4, 0, 2, 3}, "titles equal after folding keep library order");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0, 2, 3, 1, 4}, "titles equal after folding keep library order descending");
    expectRows(libraryRows(videos, "same", SortBy::title, true), {0, 2, 3}, "tied titles keep library order with a search");

    std::vector<Video> identical = {titled("x"), titled("x"), titled("x"), titled("x")};
    expectRows(libraryRows(identical, "", SortBy::title, false), {0, 1, 2, 3}, "identical titles keep library order");
    expectRows(libraryRows(identical, "", SortBy::title, true), {0, 1, 2, 3}, "identical titles keep library order descending");
}

static void testSortByLength()
{
    std::vector<Video> videos = {makeVideo("a", 300, 0), makeVideo("b", 60, 0), makeVideo("c", 7200, 0), makeVideo("d", 1, 0)};
    expectRows(libraryRows(videos, "", SortBy::length, false), {3, 1, 0, 2}, "length sorts shortest first");
    expectRows(libraryRows(videos, "", SortBy::length, true), {2, 0, 1, 3}, "length descending sorts longest first");
}

static void testSortByLengthTies()
{
    std::vector<Video> videos = {makeVideo("a", 300, 0), makeVideo("b", 100, 0), makeVideo("c", 300, 0), makeVideo("d", 100, 0),
                                 makeVideo("e", 200, 0)};
    expectRows(libraryRows(videos, "", SortBy::length, false), {1, 3, 4, 0, 2}, "equal lengths keep library order");
    expectRows(libraryRows(videos, "", SortBy::length, true), {0, 2, 4, 1, 3}, "equal lengths keep library order descending");

    std::vector<Video> all_same = {makeVideo("a", 50, 0), makeVideo("b", 50, 0), makeVideo("c", 50, 0)};
    expectRows(libraryRows(all_same, "", SortBy::length, false), {0, 1, 2}, "all equal lengths keep library order");
    expectRows(libraryRows(all_same, "", SortBy::length, true), {0, 1, 2}, "all equal lengths keep library order descending");
}

static void testSortByLengthOddDurations()
{
    std::vector<Video> videos = {makeVideo("a", 0, 0), makeVideo("b", -5, 0), makeVideo("c", 10, 0), makeVideo("d", 0, 0),
                                 makeVideo("e", INT_MAX, 0), makeVideo("f", INT_MIN, 0)};
    expectRows(libraryRows(videos, "", SortBy::length, false), {5, 1, 0, 3, 2, 4}, "zero and negative durations sort by value");
    expectRows(libraryRows(videos, "", SortBy::length, true), {4, 2, 0, 3, 1, 5}, "zero and negative durations sort by value descending");
}

static void testSortByWatched()
{
    std::vector<Video> videos = {makeVideo("a", 100, 40), makeVideo("b", 100, 90), makeVideo("c", 100, 10), makeVideo("d", 100, 60)};
    expectRows(libraryRows(videos, "", SortBy::watched, false), {2, 0, 3, 1}, "watched sorts least watched first");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {1, 3, 0, 2}, "watched descending sorts most watched first");
}

static void testSortByWatchedUsesPercentNotPosition()
{
    std::vector<Video> videos = {makeVideo("a", 10000, 1000), makeVideo("b", 100, 50), makeVideo("c", 1000, 900)};
    expectRows(libraryRows(videos, "", SortBy::watched, false), {0, 1, 2}, "watched sorts by percent, not seconds watched");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {2, 1, 0}, "watched descending sorts by percent, not seconds watched");
}

static void testSortByWatchedTies()
{
    std::vector<Video> videos = {makeVideo("a", 200, 100), makeVideo("b", 100, 10), makeVideo("c", 100, 50), makeVideo("d", 300, 151),
                                 makeVideo("e", 1000, 100)};
    expectRows(libraryRows(videos, "", SortBy::watched, false), {1, 4, 0, 2, 3}, "equal percents keep library order");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {0, 2, 3, 1, 4}, "equal percents keep library order descending");
}

static void testSortByWatchedClampedTies()
{
    std::vector<Video> videos = {makeVideo("a", 100, 500), makeVideo("b", 0, 50), makeVideo("c", 100, 100), makeVideo("d", 100, -30),
                                 makeVideo("e", -10, 5), makeVideo("f", 100, 0), makeVideo("g", 100, 50)};
    expectRows(libraryRows(videos, "", SortBy::watched, false), {1, 3, 4, 5, 6, 0, 2},
               "clamped and zero duration percents tie and keep library order");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {0, 2, 6, 1, 3, 4, 5},
               "clamped and zero duration percents tie and keep library order descending");
}

static void testSortByWatchedLargeValues()
{
    std::vector<Video> videos = {makeVideo("a", 2000000000, 1999999999), makeVideo("b", 2000000000, 2000000000),
                                 makeVideo("c", 2000000000, 1000000000)};
    expectRows(libraryRows(videos, "", SortBy::watched, false), {2, 0, 1}, "huge videos sort by percent without overflow");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {1, 0, 2}, "huge videos sort by percent descending without overflow");
}

static void testSearchAndSortTogether()
{
    std::vector<Video> videos = {makeVideo("Calculus Lecture 3", 3000, 1500), makeVideo("Piano practice", 900, 900),
                                 makeVideo("calculus lecture 1", 2800, 2800), makeVideo("Lofi beats", 7200, 100),
                                 makeVideo("CALCULUS Lecture 2", 3000, 300)};
    expectRows(libraryRows(videos, "calculus", SortBy::added, false), {0, 2, 4}, "search then added gives matching library order");
    expectRows(libraryRows(videos, "calculus", SortBy::added, true), {4, 2, 0}, "search then added descending");
    expectRows(libraryRows(videos, "lecture calculus", SortBy::title, false), {2, 4, 0}, "search then title sorts lecture numbers");
    expectRows(libraryRows(videos, "LECTURE", SortBy::title, true), {0, 4, 2}, "search then title descending");
    expectRows(libraryRows(videos, "calculus", SortBy::length, false), {2, 0, 4}, "search then length keeps tied lengths in library order");
    expectRows(libraryRows(videos, "calculus", SortBy::length, true), {0, 4, 2}, "search then length descending keeps ties in library order");
    expectRows(libraryRows(videos, "calculus", SortBy::watched, false), {4, 0, 2}, "search then watched");
    expectRows(libraryRows(videos, "calculus", SortBy::watched, true), {2, 0, 4}, "search then watched descending");
    expectRows(libraryRows(videos, "  ", SortBy::watched, true), {1, 2, 0, 4, 3}, "a blank search sorts every video by watched descending");
}

static void testRealLibraryRows()
{
    std::vector<Video> videos = realLibrary();
    expectRows(libraryRows(videos, "", SortBy::added, false), {0, 1, 2}, "real library by added");
    expectRows(libraryRows(videos, "", SortBy::added, true), {2, 1, 0}, "real library by added descending");
    expectRows(libraryRows(videos, "", SortBy::title, false), {2, 1, 0}, "real library by title");
    expectRows(libraryRows(videos, "", SortBy::title, true), {0, 1, 2}, "real library by title descending");
    expectRows(libraryRows(videos, "", SortBy::length, false), {0, 2, 1}, "real library by length");
    expectRows(libraryRows(videos, "", SortBy::length, true), {1, 2, 0}, "real library by length descending");
    expectRows(libraryRows(videos, "", SortBy::watched, false), {2, 0, 1}, "real library by watched");
    expectRows(libraryRows(videos, "", SortBy::watched, true), {1, 0, 2}, "real library by watched descending");
    expectRows(libraryRows(videos, "study", SortBy::added, false), {1, 2}, "real library search for study");
    expectRows(libraryRows(videos, "study", SortBy::length, false), {2, 1}, "real library search for study by length");
    expectRows(libraryRows(videos, "POMODORO focus", SortBy::watched, true), {1, 2}, "real library search for pomodoro focus by watched descending");
    expectRows(libraryRows(videos, "deep work", SortBy::added, false), {2}, "real library search for deep work");
    expectRows(libraryRows(videos, "50-10", SortBy::title, false), {1}, "real library search for 50-10");
    expectRows(libraryRows(videos, "technique", SortBy::added, true), {0}, "real library search for technique");
    expectRows(libraryRows(videos, "the", SortBy::added, false), {0}, "the matches only The Feynman Technique");
    expectRows(libraryRows(videos, "e", SortBy::title, false), {2, 1, 0}, "a single letter matching every title");
    expectRows(libraryRows(videos, "feynman study", SortBy::added, false), {}, "words from different videos match nothing");
}

static void testRowsDoNotChangeLibrary()
{
    std::vector<Video> videos = realLibrary();
    libraryRows(videos, "study", SortBy::title, true);
    check(videos[0].title == "The Feynman Technique", "libraryRows leaves the first video in place");
    check(videos[2].duration == 5480, "libraryRows leaves the last video in place");
    expectRows(libraryRows(videos, "", SortBy::added, false), {0, 1, 2}, "a second call sees the same library order");
}

int main()
{
    testWatchedPercentBasics();
    testWatchedPercentBadDuration();
    testWatchedPercentClamped();
    testWatchedPercentNoOverflow();
    testWatchedPercentRealLibrary();
    testWatchedPercentIgnoresOtherFields();
    testEmptySearchMatchesEverything();
    testSingleWordSearch();
    testSearchIgnoresAsciiCase();
    testSearchUtf8();
    testMultiWordSearch();
    testSearchWhitespace();
    testSearchOnlyTitle();
    testSearchRealLibrary();
    testEmptyLibrary();
    testSearchMatchingNothing();
    testSingleVideo();
    testSortByAdded();
    testSortByAddedSameTitles();
    testSortByTitle();
    testSortByTitleFoldsToLowercase();
    testSortByTitlePrefixesAndDigits();
    testSortByTitleUtf8();
    testSortByTitleTies();
    testSortByLength();
    testSortByLengthTies();
    testSortByLengthOddDurations();
    testSortByWatched();
    testSortByWatchedUsesPercentNotPosition();
    testSortByWatchedTies();
    testSortByWatchedClampedTies();
    testSortByWatchedLargeValues();
    testSearchAndSortTogether();
    testRealLibraryRows();
    testRowsDoNotChangeLibrary();
    return report();
}
