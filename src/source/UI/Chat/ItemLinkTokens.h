#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// MU Moldova, items shown in the chat: an item put into the chat line stands there as a token "[Name +13]". A
// token is one piece: Backspace or Delete at its edge takes it out whole, an edit inside it turns it into plain
// text. Positions are indices into the field's wide text.
namespace UI::Chat::ItemLinkTokens
{
struct Token
{
    std::size_t start = 0;
    std::size_t length = 0;
    // Which item the caller keeps for it.
    int key = 0;
};

// The text the field must show instead, and its caret, after a token went out whole.
struct Fix
{
    std::wstring text;
    std::size_t caret = 0;
};

// Follows one edit of the field from `before` to `after` (caretAfter: the caret after it): tokens past the edit
// move, tokens it touched go (whole when one character at a token's edge was deleted -- then the fix says what
// the field shows -- else as plain text). A touched token whose whole text stands in `after` again, in its order
// among the tokens, stays there: edits on both of its sides read as one edit across it. Tokens are in order and
// describe `before`.
std::optional<Fix> ApplyEdit(std::wstring_view before, std::wstring_view after, std::size_t caretAfter,
                             std::vector<Token>& tokens);

// Drops each token whose text no longer reads "[…]" in `text`.
void DropBroken(std::wstring_view text, std::vector<Token>& tokens);

// Where a token goes for a caret: never inside another token, then at that token's end.
std::size_t InsertPoint(const std::vector<Token>& tokens, std::size_t caret);

// Records a token of `length` put in at `at` (the tokens after it move on).
void AddToken(std::vector<Token>& tokens, std::size_t at, std::size_t length, int key);

// The field reports its caret in characters; a PC's wide text holds a character past U+FFFF in two units.
std::size_t IndexOfCharacter(std::wstring_view text, std::size_t characters);
std::size_t CharacterOfIndex(std::wstring_view text, std::size_t index);
} // namespace UI::Chat::ItemLinkTokens
