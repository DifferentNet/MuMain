#pragma once

#include <cstdint>
#include <string_view>

// The amounts of zen typed into the vault and trade dialogs (UI/Inventory/StorageInventory.cpp,
// UI/Inventory/Trade.cpp): read from the typed digits and kept to what the move can carry. The packets carry
// 32-bit amounts and a character holds at most 2,000,000,000 zen, so up to ten digits are typed.
namespace UI::Inventory::ZenAmount
{
// The most zen a character carries (the server's inventory money limit; the original's 2000000000 checks).
inline constexpr std::uint32_t kMaxCarriedZen = 2'000'000'000u;
// Digits the dialogs take: enough for kMaxCarriedZen.
inline constexpr int kMaxDigits = 10;

// The typed amount: its digits as a number (other characters skipped), 0 for none. Never overflows: anything
// above kMaxDigits digits worth stays above every limit.
std::uint64_t Parse(std::wstring_view text);

// What can go into the vault: the zen carried.
std::uint32_t DepositLimit(std::uint64_t carried);
// What can come out of the vault: the zen in it, as far as the character can still carry.
std::uint32_t WithdrawLimit(std::uint64_t inVault, std::uint64_t carried);
// What can be offered in a trade: the zen carried. The zen already on the table is not counted: the server
// (OpenMU TradeMoneyAction) checks the new amount against the money left before it gives the earlier offer back,
// and drops a bigger one without an answer.
std::uint32_t TradeLimit(std::uint64_t carried);

// The amount to move: the typed one, at most the limit.
std::uint32_t Clamp(std::uint64_t typed, std::uint32_t limit);
} // namespace UI::Inventory::ZenAmount
