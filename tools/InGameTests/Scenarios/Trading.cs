using System.Globalization;
using System.Text.Json;
using MuMain.Tools.InGameTests.Clients;
using MuMain.Tools.InGameTests.Control;

namespace MuMain.Tools.InGameTests.Scenarios;

/// <summary>The steps of a trade between two clients that every trade scenario takes.</summary>
internal static class Trading
{
    private static readonly TimeSpan ServerAnswer = TimeSpan.FromSeconds(10);
    // After an offer changes, the confirm button waits about 150 frames: some
    // seconds normally, half a minute at five frames a second.
    private static readonly TimeSpan CooldownWait = TimeSpan.FromSeconds(120);
    private static readonly TimeSpan ConfirmWait = TimeSpan.FromSeconds(20);

    /// <summary>
    /// Setup: the request is a direct command. Waits until <paramref name="partner"/>'s client
    /// shows the request's dialog, which accepting it with Enter needs.
    /// </summary>
    public static async Task RequestAsync(GameClient requester, GameClient partner, string partnerName)
    {
        var partnerSequence = await partner.LastEventSequenceAsync();
        // The requester's client learns where the partner stands from the server, a
        // moment after the partner's own; until then it refuses the request as too far.
        await Expect.EventuallyAsync(
            async () =>
            {
                try
                {
                    await requester.SendAsync("trade", new { action = "request", target = partnerName });
                    return true;
                }
                catch (ControlException exception) when (exception.Error == "not_allowed")
                {
                    return false;
                }
            },
            ServerAnswer,
            $"the {requester.Role}'s client does not see the {partner.Role} next to it");
        await partner.WaitForEventAsync("trade", new Dictionary<string, string> { ["change"] = "requested" }, partnerSequence, ServerAnswer);
        // Enter only reaches the dialog once it is on screen; before that it opens the chat.
        await Expect.EventuallyAsync(
            async () => (await partner.OpenWindowsAsync()).Contains("message_box"),
            ServerAnswer,
            $"the {partner.Role} sees no trade request dialog");
    }

    /// <summary>The partner accepts with Enter; the trade window opens on both sides.</summary>
    public static async Task AcceptAsync(ScenarioContext context, GameClient requester, GameClient partner)
    {
        var requesterSequence = await requester.LastEventSequenceAsync();
        var partnerSequence = await partner.LastEventSequenceAsync();
        await Keys.PressUntilAsync(
            context,
            partner,
            "enter",
            async () => !(await partner.OpenWindowsAsync()).Contains("message_box"),
            $"the {partner.Role}'s trade request dialog does not take Enter");
        await requester.WaitForEventAsync("trade", new Dictionary<string, string> { ["change"] = "opened" }, requesterSequence, ServerAnswer);
        await partner.WaitForEventAsync("trade", new Dictionary<string, string> { ["change"] = "opened" }, partnerSequence, ServerAnswer);
    }

    /// <summary>
    /// Presses the confirm button. After an offer changes the button ignores clicks for some
    /// frames; how long that takes depends on the frame rate, so it waits for the button,
    /// then presses it until the press counts.
    /// </summary>
    public static async Task ConfirmAsync(GameClient client)
    {
        await Expect.EventuallyAsync(
            async () => (await client.StateAsync()).GetProperty("trade") is not { ValueKind: JsonValueKind.Object } trade
                        || trade.GetProperty("my_confirm_wait").GetInt32() <= 0,
            CooldownWait,
            $"the {client.Role}'s confirm button does not take clicks again");
        await Expect.EventuallyAsync(
            async () =>
            {
                await client.ClickElementAsync("trade.confirm");
                var trade = (await client.StateAsync()).GetProperty("trade");
                return trade.ValueKind != JsonValueKind.Object || trade.GetProperty("my_confirmed").GetBoolean();
            },
            ConfirmWait,
            $"the {client.Role}'s confirm button does not stay pressed");
    }

    /// <summary>
    /// Puts the item of an inventory slot into the first square of <paramref name="offerer"/>'s offer with two clicks;
    /// waits until <paramref name="partner"/> sees it in the partner's half.
    /// </summary>
    public static async Task OfferItemAsync(GameClient offerer, GameClient partner, ItemSlot item)
    {
        await offerer.OpenInventoryAsync();
        await offerer.MoveItemAsync("inventory", item.Slot, "trade", 0);
        await Expect.EventuallyAsync(
            async () => ItemSlots.OfTrade(await partner.StateAsync(), "partner_items").Any(offered => offered.Name == item.Name),
            ServerAnswer,
            $"the {partner.Role} does not see '{item.Name}' in the {offerer.Role}'s offer");
    }

    /// <summary>
    /// Clicks the zen button of the trade window, types <paramref name="zen"/> into the box that opens and confirms it
    /// with Enter; waits until <paramref name="partner"/> sees that zen in the partner's half.
    /// </summary>
    public static async Task OfferZenAsync(GameClient offerer, GameClient partner, int zen)
    {
        await offerer.ClickElementAsync("trade.zen");
        await Expect.EventuallyAsync(
            async () => (await offerer.OpenWindowsAsync()).Contains("message_box"),
            ServerAnswer,
            $"the {offerer.Role}'s zen box does not open");
        // The box focuses its field a frame after it shows.
        await Expect.EventuallyAsync(offerer.IsTypingAsync, ServerAnswer, $"the {offerer.Role}'s zen box's field does not take the keys");
        // Enter goes with the text: the focused field keeps a separate Enter key to itself, and the box reads it from
        // the key state.
        await offerer.SendAsync("type", new { text = zen.ToString(CultureInfo.InvariantCulture), enter = true });
        await Expect.EventuallyAsync(
            async () => !(await offerer.OpenWindowsAsync()).Contains("message_box"),
            ServerAnswer,
            $"the {offerer.Role}'s zen box does not take Enter");
        var seen = 0;
        await Expect.EventuallyAsync(
            async () => (seen = Zen(await partner.StateAsync(), "partner_zen")) == zen,
            ServerAnswer,
            () => $"the {partner.Role} sees {seen} zen in the {offerer.Role}'s offer, not {zen}");
    }

    /// <summary><paramref name="canceller"/> cancels the open trade; it closes as cancelled on both sides.</summary>
    public static async Task CancelAsync(GameClient canceller, GameClient partner)
    {
        var cancellerSequence = await canceller.LastEventSequenceAsync();
        var partnerSequence = await partner.LastEventSequenceAsync();
        await canceller.SendAsync("trade", new { action = "cancel" });
        var cancelled = new Dictionary<string, string> { ["change"] = "closed", ["result"] = "cancelled" };
        await canceller.WaitForEventAsync("trade", cancelled, cancellerSequence, ServerAnswer);
        await partner.WaitForEventAsync("trade", cancelled, partnerSequence, ServerAnswer);
    }

    /// <summary>The zen of one side of the open trade (<c>my_zen</c> or <c>partner_zen</c>); 0 without a trade.</summary>
    public static int Zen(JsonElement state, string side)
        => state.GetProperty("trade") is { ValueKind: JsonValueKind.Object } trade ? trade.GetProperty(side).GetInt32() : 0;
}
