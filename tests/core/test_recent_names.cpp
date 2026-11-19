#include "doctest.h"

#include "Core/Utilities/RecentNames.h"

#include <string>
#include <vector>

using Core::Utilities::RecentNames;

namespace
{
std::vector<std::string> List(std::initializer_list<const char*> names)
{
    return {names.begin(), names.end()};
}
} // namespace

TEST_CASE("RecentNames keeps the most recent name last")
{
    RecentNames names(5);
    CHECK(names.Note("a"));
    CHECK(names.Note("b"));
    CHECK(names.Note("c"));
    CHECK(names.Names() == List({"a", "b", "c"}));

    // A name noted again moves to the end instead of appearing twice.
    CHECK(names.Note("a"));
    CHECK(names.Names() == List({"b", "c", "a"}));

    // Already last, or empty: nothing changes.
    CHECK_FALSE(names.Note("a"));
    CHECK_FALSE(names.Note(""));
    CHECK(names.Names() == List({"b", "c", "a"}));
}

TEST_CASE("RecentNames drops the oldest names beyond its capacity")
{
    RecentNames names(3);
    for (const char* name : {"a", "b", "c", "d", "e"})
        names.Note(name);
    CHECK(names.Names() == List({"c", "d", "e"}));

    // Noting a name it holds keeps the size.
    names.Note("c");
    CHECK(names.Names() == List({"d", "e", "c"}));

    RecentNames none(0);
    CHECK_FALSE(none.Note("a"));
    CHECK(none.Names().empty());
}

TEST_CASE("RecentNames reads back what Join wrote")
{
    RecentNames names(150);
    names.Note("Data/Effect/flare.jpg");
    names.Note("Data/Item/Texture/sword01.tga");
    const std::string text = names.Join();
    CHECK(text == "Data/Effect/flare.jpg\nData/Item/Texture/sword01.tga\n");

    RecentNames loaded(150);
    loaded.Load(text);
    CHECK(loaded.Names() == names.Names());

    RecentNames empty(150);
    CHECK(empty.Join().empty());
    empty.Load("");
    CHECK(empty.Names().empty());
}

TEST_CASE("RecentNames::Load skips empty lines, takes CRLF, and keeps the last lines within the capacity")
{
    RecentNames names(3);
    names.Note("old");
    names.Load("a\r\n\r\nb\na\nc\nd");
    // "a" counts where it comes last; "b" was the oldest beyond 3 once "d" came.
    CHECK(names.Names() == List({"a", "c", "d"}));
}
