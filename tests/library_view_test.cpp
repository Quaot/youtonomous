#include <climits>
#include <map>
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

static Video withId(const std::string& title, const std::string& id, int duration)
{
    Video video = makeVideo(title, duration, 0);
    video.id = id;
    video.url = "https://www.youtube.com/watch?v=" + id;
    video.file = "C:\\Videos\\Youtonomous\\" + id + ".mp4";
    return video;
}

static Video studyVideo(const std::string& title, const std::string& id)
{
    return withId(title, id, 600);
}

static std::vector<Video> realLibrary()
{
    return {
        withId("The Feynman Technique", "tkm0TNFzIeg", 121),
        withId("3-HOUR STUDY WITH ME | Hyper Efficient, Doctor, Focus Music, Pomodoro 50-10", "74cOUSKXMz0", 10303),
        withId("1.5-HOUR STUDY WITH ME | Hyper Efficient, Doctor, Focus Music, Deep Work, Pomodoro 42-6-42", "sdhh7AYzsTY", 5480),
    };
}

static std::map<std::string, int> realStudied()
{
    return {{"74cOUSKXMz0", 275798}, {"sdhh7AYzsTY", 12625}};
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
    std::map<std::string, int> studied = realStudied();
    SortBy sorts[] = {SortBy::added, SortBy::title, SortBy::length, SortBy::studied};
    const char* names[] = {"added", "title", "length", "studied"};
    for (int i = 0; i < 4; ++i) {
        for (bool descending : {false, true}) {
            std::string direction = descending ? " descending" : " ascending";
            expectRows(libraryRows(videos, {}, "", sorts[i], descending), {},
                       std::string("an empty library gives no rows sorted by ") + names[i] + direction);
            expectRows(libraryRows(videos, {}, "feynman", sorts[i], descending), {},
                       std::string("an empty library with a search gives no rows sorted by ") + names[i] + direction);
            expectRows(libraryRows(videos, studied, "", sorts[i], descending), {},
                       std::string("an empty library with study time on record gives no rows sorted by ") + names[i] + direction);
        }
    }
}

static void testSearchMatchingNothing()
{
    std::vector<Video> videos = realLibrary();
    std::map<std::string, int> studied = realStudied();
    SortBy sorts[] = {SortBy::added, SortBy::title, SortBy::length, SortBy::studied};
    const char* names[] = {"added", "title", "length", "studied"};
    for (int i = 0; i < 4; ++i) {
        for (bool descending : {false, true}) {
            std::string direction = descending ? " descending" : " ascending";
            expectRows(libraryRows(videos, {}, "calculus", sorts[i], descending), {},
                       std::string("a search matching nothing gives no rows sorted by ") + names[i] + direction);
            expectRows(libraryRows(videos, studied, "calculus", sorts[i], descending), {},
                       std::string("a search matching nothing gives no rows with study time sorted by ") + names[i] + direction);
        }
    }
}

static void testSingleVideo()
{
    std::vector<Video> videos = {makeVideo("Only", 0, 0)};
    expectRows(libraryRows(videos, {}, "", SortBy::added, false), {0}, "one video sorted by added");
    expectRows(libraryRows(videos, {}, "", SortBy::added, true), {0}, "one video sorted by added descending");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0}, "one video sorted by title descending");
    expectRows(libraryRows(videos, {}, "", SortBy::length, false), {0}, "one video with zero duration sorted by length");
    expectRows(libraryRows(videos, {}, "", SortBy::studied, true), {0}, "one video with no study time sorted by studied descending");
    expectRows(libraryRows(videos, {{"id0x0", 90}}, "", SortBy::studied, false), {0}, "one video with study time sorted by studied");
    expectRows(libraryRows(videos, {{"id0x0", 90}}, "other", SortBy::studied, false), {},
               "one video with study time that does not match gives no rows");
    expectRows(libraryRows(videos, {}, "ONLY", SortBy::added, false), {0}, "one matching video is found");
    expectRows(libraryRows(videos, {}, "other", SortBy::added, false), {}, "one video that does not match gives no rows");
}

static void testSortByAdded()
{
    std::vector<Video> videos = {titled("Delta"), titled("alpha"), titled("Charlie"), titled("bravo")};
    expectRows(libraryRows(videos, {}, "", SortBy::added, false), {0, 1, 2, 3}, "added keeps library order");
    expectRows(libraryRows(videos, {}, "", SortBy::added, true), {3, 2, 1, 0}, "added descending reverses library order");
    expectRows(libraryRows(videos, {}, "a", SortBy::added, false), {0, 1, 2, 3}, "a search matching every video keeps library order");
    expectRows(libraryRows(videos, {}, "l", SortBy::added, false), {0, 1, 2}, "added skips videos that do not match");
    expectRows(libraryRows(videos, {}, "l", SortBy::added, true), {2, 1, 0}, "added descending skips videos that do not match");
    expectRows(libraryRows(videos, {}, "  \t ", SortBy::added, true), {3, 2, 1, 0}, "a whitespace search shows every video");
}

static void testSortByAddedSameTitles()
{
    std::vector<Video> videos = {titled("Same"), titled("Same"), titled("Same")};
    expectRows(libraryRows(videos, {}, "", SortBy::added, false), {0, 1, 2}, "identical titles keep library order");
    expectRows(libraryRows(videos, {}, "", SortBy::added, true), {2, 1, 0}, "identical titles reverse under added descending");
}

static void testSortByTitle()
{
    std::vector<Video> videos = {titled("cherry"), titled("Banana"), titled("apple"), titled("Date")};
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {2, 1, 0, 3}, "title sorts alphabetically ignoring case");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {3, 0, 1, 2}, "title descending sorts in reverse");
    expectRows(libraryRows(videos, {}, "an", SortBy::title, false), {1}, "title sort with a search shows only matches");
    expectRows(libraryRows(videos, {}, "e", SortBy::title, false), {2, 0, 3}, "title sort skips videos that do not match");
    expectRows(libraryRows(videos, {}, "e", SortBy::title, true), {3, 0, 2}, "title descending skips videos that do not match");
}

static void testSortByTitleFoldsToLowercase()
{
    std::vector<Video> videos = {titled("Zeta"), titled("_note"), titled("alpha")};
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {1, 2, 0}, "an underscore sorts before letters once letters fold to lowercase");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0, 2, 1}, "an underscore sorts after letters in descending title order");

    std::vector<Video> brackets = {titled("B"), titled("[x"), titled("a")};
    expectRows(libraryRows(brackets, {}, "", SortBy::title, false), {1, 2, 0}, "[ sorts before folded letters");

    std::vector<Video> braces = {titled("{x"), titled("Z"), titled("a")};
    expectRows(libraryRows(braces, {}, "", SortBy::title, false), {2, 1, 0}, "{ sorts after folded letters");
}

static void testSortByTitlePrefixesAndDigits()
{
    std::vector<Video> videos = {titled("abcd"), titled("ABC"), titled(""), titled("ab"), titled("10"), titled("9"), titled(" lead")};
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {2, 6, 4, 5, 3, 1, 0},
               "the empty title comes first, then space, digits, then shorter prefixes before longer");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0, 1, 3, 5, 4, 6, 2}, "the same titles in descending order");
}

static void testSortByTitleUtf8()
{
    std::vector<Video> videos = {titled("émile"), titled("zed"), titled("Émile"), titled("Apple")};
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {3, 1, 2, 0},
               "UTF-8 titles sort after ASCII by byte and É sorts before é");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0, 2, 1, 3}, "UTF-8 titles in descending order");

    std::vector<Video> accents = {titled("Ébc"), titled("éaa")};
    expectRows(libraryRows(accents, {}, "", SortBy::title, false), {0, 1}, "UTF-8 letters are not folded when sorting");
}

static void testSortByTitleTies()
{
    std::vector<Video> videos = {titled("same"), titled("Other"), titled("SAME"), titled("Same"), titled("other")};
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {1, 4, 0, 2, 3}, "titles equal after folding keep library order");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0, 2, 3, 1, 4}, "titles equal after folding keep library order descending");
    expectRows(libraryRows(videos, {}, "same", SortBy::title, true), {0, 2, 3}, "tied titles keep library order with a search");

    std::vector<Video> identical = {titled("x"), titled("x"), titled("x"), titled("x")};
    expectRows(libraryRows(identical, {}, "", SortBy::title, false), {0, 1, 2, 3}, "identical titles keep library order");
    expectRows(libraryRows(identical, {}, "", SortBy::title, true), {0, 1, 2, 3}, "identical titles keep library order descending");
}

static void testSortByLength()
{
    std::vector<Video> videos = {makeVideo("a", 300, 0), makeVideo("b", 60, 0), makeVideo("c", 7200, 0), makeVideo("d", 1, 0)};
    expectRows(libraryRows(videos, {}, "", SortBy::length, false), {3, 1, 0, 2}, "length sorts shortest first");
    expectRows(libraryRows(videos, {}, "", SortBy::length, true), {2, 0, 1, 3}, "length descending sorts longest first");
}

static void testSortByLengthTies()
{
    std::vector<Video> videos = {makeVideo("a", 300, 0), makeVideo("b", 100, 0), makeVideo("c", 300, 0), makeVideo("d", 100, 0),
                                 makeVideo("e", 200, 0)};
    expectRows(libraryRows(videos, {}, "", SortBy::length, false), {1, 3, 4, 0, 2}, "equal lengths keep library order");
    expectRows(libraryRows(videos, {}, "", SortBy::length, true), {0, 2, 4, 1, 3}, "equal lengths keep library order descending");

    std::vector<Video> all_same = {makeVideo("a", 50, 0), makeVideo("b", 50, 0), makeVideo("c", 50, 0)};
    expectRows(libraryRows(all_same, {}, "", SortBy::length, false), {0, 1, 2}, "all equal lengths keep library order");
    expectRows(libraryRows(all_same, {}, "", SortBy::length, true), {0, 1, 2}, "all equal lengths keep library order descending");
}

static void testSortByLengthOddDurations()
{
    std::vector<Video> videos = {makeVideo("a", 0, 0), makeVideo("b", -5, 0), makeVideo("c", 10, 0), makeVideo("d", 0, 0),
                                 makeVideo("e", INT_MAX, 0), makeVideo("f", INT_MIN, 0)};
    expectRows(libraryRows(videos, {}, "", SortBy::length, false), {5, 1, 0, 3, 2, 4}, "zero and negative durations sort by value");
    expectRows(libraryRows(videos, {}, "", SortBy::length, true), {4, 2, 0, 3, 1, 5}, "zero and negative durations sort by value descending");
}

static void testSortByStudied()
{
    std::vector<Video> videos = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd")};
    std::map<std::string, int> studied = {{"va", 300}, {"vb", 60}, {"vc", 7200}, {"vd", 1}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {3, 1, 0, 2}, "studied sorts least studied first");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {2, 0, 1, 3}, "studied descending sorts most studied first");
}

static void testSortByStudiedIgnoresLengthAndTitle()
{
    std::vector<Video> videos = {withId("Zulu", "vz", 10), withId("alpha", "va", 9000), withId("Mike", "vm", 500)};
    std::map<std::string, int> studied = {{"vz", 200}, {"va", 100}, {"vm", 300}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {1, 0, 2}, "studied sorts by seconds studied, not by length or title");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {2, 0, 1},
               "studied descending sorts by seconds studied, not by length or title");

    Video played = studyVideo("e", "ve");
    played.position = 590;
    played.start = 300;
    std::vector<Video> positions = {played, studyVideo("f", "vf")};
    expectRows(libraryRows(positions, {{"vf", 1}}, "", SortBy::studied, false), {0, 1}, "the playback position is not counted as study time");
    expectRows(libraryRows(positions, {{"vf", 1}}, "", SortBy::studied, true), {1, 0},
               "the playback position is not counted as study time descending");
}

static void testSortByStudiedMissingIds()
{
    std::vector<Video> videos = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd")};
    std::map<std::string, int> studied = {{"va", 100}, {"vc", 50}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {1, 3, 2, 0}, "videos missing from the map count as 0 and come first");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {0, 2, 1, 3}, "videos missing from the map count as 0 and come last descending");

    expectRows(libraryRows(videos, {}, "", SortBy::studied, false), {0, 1, 2, 3}, "an empty map ties every video and keeps library order");
    expectRows(libraryRows(videos, {}, "", SortBy::studied, true), {0, 1, 2, 3},
               "an empty map ties every video and keeps library order descending");

    std::map<std::string, int> zero = {{"vb", 0}, {"vd", 5}};
    expectRows(libraryRows(videos, zero, "", SortBy::studied, false), {0, 1, 2, 3}, "a missing id ties with an id stored as 0");
    expectRows(libraryRows(videos, zero, "", SortBy::studied, true), {3, 0, 1, 2}, "a missing id ties with an id stored as 0 descending");
}

static void testSortByStudiedExtraIds()
{
    std::vector<Video> videos = {studyVideo("Alpha", "va"), studyVideo("Bravo", "vb")};
    std::map<std::string, int> studied = {{"va", 20}, {"vb", 10}, {"gone", 999999}, {"", 50}, {"Alpha", -100}, {"VA", 1}, {"va ", 1000}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {1, 0}, "ids not in the library are ignored");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {0, 1}, "ids not in the library are ignored descending");

    std::map<std::string, int> only_extra = {{"gone", 999999}, {"other", -5}, {"Bravo", 7}};
    expectRows(libraryRows(videos, only_extra, "", SortBy::studied, false), {0, 1}, "a map holding only other ids ties every video");
    expectRows(libraryRows(videos, only_extra, "", SortBy::studied, true), {0, 1}, "a map holding only other ids ties every video descending");
}

static void testSortByStudiedTies()
{
    std::vector<Video> videos = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd"),
                                 studyVideo("e", "ve")};
    std::map<std::string, int> studied = {{"va", 50}, {"vb", 10}, {"vc", 50}, {"vd", 10}, {"ve", 30}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {1, 3, 4, 0, 2}, "equal study times keep library order");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {0, 2, 4, 1, 3}, "equal study times keep library order descending");

    std::vector<Video> backwards = {studyVideo("z", "vz"), studyVideo("y", "vy"), studyVideo("x", "vx")};
    std::map<std::string, int> same = {{"vz", 40}, {"vy", 40}, {"vx", 40}};
    expectRows(libraryRows(backwards, same, "", SortBy::studied, false), {0, 1, 2}, "all equal study times keep library order");
    expectRows(libraryRows(backwards, same, "", SortBy::studied, true), {0, 1, 2}, "all equal study times keep library order descending");
}

static void testSortByStudiedDuplicateIds()
{
    std::vector<Video> videos = {studyVideo("a", "vx"), studyVideo("b", "vy"), studyVideo("c", "vx"), studyVideo("d", "vz")};
    std::map<std::string, int> studied = {{"vx", 100}, {"vy", 200}, {"vz", 50}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {3, 0, 2, 1}, "two videos with the same id both get its study time");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {1, 0, 2, 3},
               "two videos with the same id both get its study time descending");

    std::vector<Video> missing = {studyVideo("a", "vx"), studyVideo("b", "vy"), studyVideo("c", "vx")};
    expectRows(libraryRows(missing, {{"vy", 5}}, "", SortBy::studied, false), {0, 2, 1}, "two videos with the same missing id both count as 0");
    expectRows(libraryRows(missing, {{"vy", 5}}, "", SortBy::studied, true), {1, 0, 2},
               "two videos with the same missing id both count as 0 descending");

    std::vector<Video> empty_ids = {studyVideo("a", ""), studyVideo("b", "vb"), studyVideo("c", "")};
    std::map<std::string, int> empty_key = {{"", 80}, {"vb", 40}};
    expectRows(libraryRows(empty_ids, empty_key, "", SortBy::studied, false), {1, 0, 2}, "videos with an empty id get the value stored under the empty id");
    expectRows(libraryRows(empty_ids, empty_key, "", SortBy::studied, true), {0, 2, 1},
               "videos with an empty id get the value stored under the empty id descending");
}

static void testSortByStudiedNegativeValues()
{
    std::vector<Video> videos = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd"),
                                 studyVideo("e", "ve")};
    std::map<std::string, int> studied = {{"va", -5}, {"vc", 10}, {"vd", INT_MIN}, {"ve", -5}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {3, 0, 4, 1, 2}, "negative study times sort below a missing id");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {2, 1, 0, 4, 3},
               "negative study times sort below a missing id descending");
}

static void testSortByStudiedLargeValues()
{
    std::vector<Video> videos = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd"),
                                 studyVideo("e", "ve")};
    std::map<std::string, int> studied = {{"va", INT_MAX}, {"vb", INT_MAX - 1}, {"vc", 0}, {"vd", INT_MAX}, {"ve", 2000000000}};
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {2, 4, 1, 0, 3}, "study times near INT_MAX sort by value");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {0, 3, 1, 4, 2},
               "study times near INT_MAX sort by value descending and tie in library order");

    std::vector<Video> spread = {studyVideo("a", "va"), studyVideo("b", "vb"), studyVideo("c", "vc"), studyVideo("d", "vd")};
    std::map<std::string, int> extremes = {{"va", INT_MAX}, {"vb", INT_MIN}, {"vc", 1}, {"vd", -1}};
    expectRows(libraryRows(spread, extremes, "", SortBy::studied, false), {1, 3, 2, 0}, "INT_MAX and INT_MIN together sort without overflow");
    expectRows(libraryRows(spread, extremes, "", SortBy::studied, true), {0, 2, 3, 1},
               "INT_MAX and INT_MIN together sort without overflow descending");
}

static void testStudiedOnlyChangesStudiedSort()
{
    std::vector<Video> videos = {withId("Delta", "vd", 300), withId("alpha", "va", 60), withId("Charlie", "vc", 7200),
                                 withId("bravo", "vb", 1)};
    std::map<std::string, int> studied = {{"vd", 5}, {"va", 9000}, {"vc", -40}, {"vb", INT_MAX}, {"gone", 100}};
    SortBy sorts[] = {SortBy::added, SortBy::title, SortBy::length};
    const char* names[] = {"added", "title", "length"};
    for (int i = 0; i < 3; ++i) {
        for (bool descending : {false, true}) {
            std::string direction = descending ? " descending" : " ascending";
            for (const std::string& search : {std::string(""), std::string("a"), std::string("l"), std::string("zzz")}) {
                std::vector<int> without = libraryRows(videos, {}, search, sorts[i], descending);
                std::vector<int> with = libraryRows(videos, studied, search, sorts[i], descending);
                expectRows(with, without, std::string("study time does not change the rows sorted by ") + names[i] + direction
                                              + " for the search \"" + search + "\"");
            }
        }
    }
    expectRows(libraryRows(videos, studied, "", SortBy::added, false), {0, 1, 2, 3}, "added with study time keeps library order");
    expectRows(libraryRows(videos, studied, "", SortBy::title, false), {1, 3, 2, 0}, "title with study time sorts alphabetically");
    expectRows(libraryRows(videos, studied, "", SortBy::length, true), {2, 0, 1, 3}, "length with study time sorts longest first");
    expectRows(libraryRows(videos, studied, "l", SortBy::added, false), {0, 1, 2}, "study time does not bring back videos the search skips");
}

static void testSearchAndSortTogether()
{
    std::vector<Video> videos = {makeVideo("Calculus Lecture 3", 3000, 1500), makeVideo("Piano practice", 900, 900),
                                 makeVideo("calculus lecture 1", 2800, 2800), makeVideo("Lofi beats", 7200, 100),
                                 makeVideo("CALCULUS Lecture 2", 3000, 300)};
    expectRows(libraryRows(videos, {}, "calculus", SortBy::added, false), {0, 2, 4}, "search then added gives matching library order");
    expectRows(libraryRows(videos, {}, "calculus", SortBy::added, true), {4, 2, 0}, "search then added descending");
    expectRows(libraryRows(videos, {}, "lecture calculus", SortBy::title, false), {2, 4, 0}, "search then title sorts lecture numbers");
    expectRows(libraryRows(videos, {}, "LECTURE", SortBy::title, true), {0, 4, 2}, "search then title descending");
    expectRows(libraryRows(videos, {}, "calculus", SortBy::length, false), {2, 0, 4}, "search then length keeps tied lengths in library order");
    expectRows(libraryRows(videos, {}, "calculus", SortBy::length, true), {0, 4, 2}, "search then length descending keeps ties in library order");
    std::map<std::string, int> studied = {{"id3000x1500", 1500}, {"id900x900", 9000}, {"id2800x2800", 2800}, {"id7200x100", 100}};
    expectRows(libraryRows(videos, studied, "calculus", SortBy::studied, false), {4, 0, 2}, "search then studied");
    expectRows(libraryRows(videos, studied, "calculus", SortBy::studied, true), {2, 0, 4}, "search then studied descending");
    expectRows(libraryRows(videos, studied, "  ", SortBy::studied, true), {1, 2, 0, 3, 4}, "a blank search sorts every video by studied descending");
    expectRows(libraryRows(videos, studied, "piano", SortBy::studied, false), {1}, "a search for the most studied video shows only that video");
    expectRows(libraryRows(videos, studied, "lecture 2", SortBy::studied, true), {4},
               "a search for a video with no study time shows only that video");
}

static void testRealLibraryRows()
{
    std::vector<Video> videos = realLibrary();
    expectRows(libraryRows(videos, {}, "", SortBy::added, false), {0, 1, 2}, "real library by added");
    expectRows(libraryRows(videos, {}, "", SortBy::added, true), {2, 1, 0}, "real library by added descending");
    expectRows(libraryRows(videos, {}, "", SortBy::title, false), {2, 1, 0}, "real library by title");
    expectRows(libraryRows(videos, {}, "", SortBy::title, true), {0, 1, 2}, "real library by title descending");
    expectRows(libraryRows(videos, {}, "", SortBy::length, false), {0, 2, 1}, "real library by length");
    expectRows(libraryRows(videos, {}, "", SortBy::length, true), {1, 2, 0}, "real library by length descending");
    expectRows(libraryRows(videos, {}, "study", SortBy::added, false), {1, 2}, "real library search for study");
    expectRows(libraryRows(videos, {}, "study", SortBy::length, false), {2, 1}, "real library search for study by length");
    expectRows(libraryRows(videos, {}, "deep work", SortBy::added, false), {2}, "real library search for deep work");
    expectRows(libraryRows(videos, {}, "50-10", SortBy::title, false), {1}, "real library search for 50-10");
    expectRows(libraryRows(videos, {}, "technique", SortBy::added, true), {0}, "real library search for technique");
    expectRows(libraryRows(videos, {}, "the", SortBy::added, false), {0}, "the matches only The Feynman Technique");
    expectRows(libraryRows(videos, {}, "e", SortBy::title, false), {2, 1, 0}, "a single letter matching every title");
    expectRows(libraryRows(videos, {}, "feynman study", SortBy::added, false), {}, "words from different videos match nothing");
}

static void testRealLibraryStudied()
{
    std::vector<Video> videos = realLibrary();
    std::map<std::string, int> studied = realStudied();
    expectRows(libraryRows(videos, studied, "", SortBy::studied, false), {0, 2, 1}, "real library by studied");
    expectRows(libraryRows(videos, studied, "", SortBy::studied, true), {1, 2, 0}, "real library by studied descending");
    expectRows(libraryRows(videos, studied, "study", SortBy::studied, false), {2, 1}, "real library search for study by studied");
    expectRows(libraryRows(videos, studied, "POMODORO focus", SortBy::studied, true), {1, 2},
               "real library search for pomodoro focus by studied descending");
    expectRows(libraryRows(videos, studied, "feynman", SortBy::studied, true), {0}, "real library search for feynman by studied");
    expectRows(libraryRows(videos, studied, "deep work", SortBy::studied, false), {2}, "real library search for deep work by studied");
    expectRows(libraryRows(videos, studied, "feynman study", SortBy::studied, false), {}, "words from different videos match nothing by studied");
    expectRows(libraryRows(videos, {}, "", SortBy::studied, false), {0, 1, 2}, "real library with no study time keeps library order");
    expectRows(libraryRows(videos, {}, "", SortBy::studied, true), {0, 1, 2}, "real library with no study time keeps library order descending");
    expectRows(libraryRows(videos, studied, "", SortBy::added, false), {0, 1, 2}, "real library by added with study time");
    expectRows(libraryRows(videos, studied, "", SortBy::title, false), {2, 1, 0}, "real library by title with study time");
    expectRows(libraryRows(videos, studied, "", SortBy::length, true), {1, 2, 0}, "real library by length descending with study time");

    std::map<std::string, int> feynman_first = studied;
    feynman_first["tkm0TNFzIeg"] = 300000;
    expectRows(libraryRows(videos, feynman_first, "", SortBy::studied, true), {0, 1, 2},
               "real library with Feynman studied the most puts it first descending");
}

static void testRowsDoNotChangeLibrary()
{
    std::vector<Video> videos = realLibrary();
    std::map<std::string, int> studied = realStudied();
    libraryRows(videos, {}, "study", SortBy::title, true);
    libraryRows(videos, studied, "study", SortBy::studied, true);
    check(videos[0].title == "The Feynman Technique", "libraryRows leaves the first video in place");
    check(videos[2].duration == 5480, "libraryRows leaves the last video in place");
    check(studied == realStudied(), "libraryRows leaves the studied map unchanged, got " + std::to_string(studied.size()) + " entries");
    expectRows(libraryRows(videos, {}, "", SortBy::added, false), {0, 1, 2}, "a second call sees the same library order");
}

int main()
{
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
    testSortByStudied();
    testSortByStudiedIgnoresLengthAndTitle();
    testSortByStudiedMissingIds();
    testSortByStudiedExtraIds();
    testSortByStudiedTies();
    testSortByStudiedDuplicateIds();
    testSortByStudiedNegativeValues();
    testSortByStudiedLargeValues();
    testStudiedOnlyChangesStudiedSort();
    testSearchAndSortTogether();
    testRealLibraryRows();
    testRealLibraryStudied();
    testRowsDoNotChangeLibrary();
    return report();
}
