using MuMain.Tools.InGameTests.Clients;

namespace MuMain.Tools.InGameTests.Scenarios;

/// <summary>
/// Two trades with an item and zen in the window, cancelled: once by the player who typed the zen, once by the other
/// while both have typed zen. Each time both keep exactly what they had: the zen comes back once, the item once.
/// </summary>
internal sealed class TradeCancelScenario : Scenario
{
    private const string First = "first";
    private const string Second = "second";

    private const string LorenciaGate = "Lorencia";
    private const int LorenciaMap = 0;
    private const int MeetingX = 135;
    private const int MeetingY = 128;

    // Numbers that stand out in the screenshots and the server log.
    private const int SecondZen = 123456;
    private const int FirstZen = 54321;

    private static readonly TimeSpan ServerAnswer = TimeSpan.FromSeconds(10);

    // How long a cancelled trade's zen and items have to stay as they were: a second give-back would come by then.
    private static readonly TimeSpan Settle = TimeSpan.FromSeconds(3);

    public override string Name => "trade-cancel";

    public override string Description => "a trade with an item and typed zen in the window, cancelled, gives both back once";

    public override ScenarioCategory Category => ScenarioCategory.PlayerInteractions;

    public override IReadOnlyList<string> Roles => [First, Second];

    public override int StepCount => 4 + 6 + 6;

    public override async Task RunAsync(ScenarioContext context)
    {
        var first = context.Client(First);
        var second = context.Client(Second);
        var firstCharacter = TestAccounts.CancelTraderFirst;
        var secondCharacter = TestAccounts.CancelTraderSecond;

        await context.StepAsync(
            $"The first player logs in as {firstCharacter.Name}",
            $"{firstCharacter.Name} enters the world in the safe zone of its home map.",
            () => first.EnterWorldAsync(firstCharacter.Account, firstCharacter.Password, firstCharacter.Name),
            [First]);
        await context.StepAsync(
            $"The second player logs in as {secondCharacter.Name}",
            $"{secondCharacter.Name} enters the world in the safe zone of its home map.",
            () => second.EnterWorldAsync(secondCharacter.Account, secondCharacter.Password, secondCharacter.Name),
            [Second]);
        await context.StepAsync(
            "The first player walks to the meeting spot",
            $"{firstCharacter.Name} stands at ({MeetingX},{MeetingY}), a free spot in Lorencia's town, or one tile from it.",
            async () =>
            {
                await first.WarpAsync(LorenciaGate, LorenciaMap);
                await Meeting.WalkToAsync(first, MeetingX, MeetingY);
            },
            [First]);
        await context.StepAsync(
            "The second player walks up to the first",
            $"{secondCharacter.Name} stands next to {firstCharacter.Name}: a trade needs the partner at most one tile away.",
            async () =>
            {
                await second.WarpAsync(LorenciaGate, LorenciaMap);
                await Meeting.WalkUpToAsync(second, first, secondCharacter.Name);
            });

        // Round 1: the first offers a jewel, the second zen, and the second cancels.
        var jewel = await FindJewelAsync(first);
        var before = await HoldingsAsync(first, second, jewel.Name);
        await OpenTradeAsync(context, first, second, secondCharacter.Name, "");
        await context.StepAsync(
            $"The first player puts '{jewel.Name}' into the trade window with two clicks",
            "The second player sees it in the first player's offer.",
            () => Trading.OfferItemAsync(first, second, jewel));
        await context.StepAsync(
            $"The second player offers {SecondZen:N0} zen",
            $"The second player types {SecondZen} into the trade's zen box and confirms with Enter. The zen leaves its "
            + $"inventory, and the first player sees {SecondZen:N0} zen in its offer.",
            async () =>
            {
                await Trading.OfferZenAsync(second, first, SecondZen);
                await ExpectZenAsync(second, before.SecondZen - SecondZen, "while the zen is in the trade window");
            });
        await context.StepAsync(
            "The second player cancels the trade",
            "The trade window closes on both sides, as cancelled.",
            () => Trading.CancelAsync(second, first));
        await context.StepAsync(
            "Both keep what they had",
            $"{secondCharacter.Name} has its {SecondZen:N0} zen back ({before.SecondZen:N0}), {firstCharacter.Name} its "
            + $"'{jewel.Name}' in slot {jewel.Slot} ({before.FirstJewels} such jewels) and its zen ({before.FirstZen:N0}); "
            + $"after {Settle.TotalSeconds:0} s still the same, so nothing came back twice.",
            () => ExpectKeptAsync(first, second, jewel, before));

        // Round 2: both offer zen, the first a jewel too, and the first cancels.
        before = await HoldingsAsync(first, second, jewel.Name);
        await OpenTradeAsync(context, first, second, secondCharacter.Name, " again");
        await context.StepAsync(
            $"The first player puts '{jewel.Name}' and {FirstZen:N0} zen into the trade window",
            $"The second player sees the jewel and {FirstZen:N0} zen in the first player's offer.",
            async () =>
            {
                await Trading.OfferItemAsync(first, second, jewel);
                await Trading.OfferZenAsync(first, second, FirstZen);
                await ExpectZenAsync(first, before.FirstZen - FirstZen, "while the zen is in the trade window");
            });
        await context.StepAsync(
            $"The second player offers {SecondZen:N0} zen",
            $"The first player sees {SecondZen:N0} zen in the second player's offer.",
            async () =>
            {
                await Trading.OfferZenAsync(second, first, SecondZen);
                await ExpectZenAsync(second, before.SecondZen - SecondZen, "while the zen is in the trade window");
            });
        await context.StepAsync(
            "The first player cancels the trade",
            "The trade window closes on both sides, as cancelled.",
            () => Trading.CancelAsync(first, second));
        await context.StepAsync(
            "Both keep what they had",
            $"{firstCharacter.Name} has its {FirstZen:N0} zen ({before.FirstZen:N0}) and its '{jewel.Name}' back, "
            + $"{secondCharacter.Name} its {SecondZen:N0} zen ({before.SecondZen:N0}); after {Settle.TotalSeconds:0} s "
            + "still the same.",
            () => ExpectKeptAsync(first, second, jewel, before));
    }

    private static async Task OpenTradeAsync(ScenarioContext context, GameClient first, GameClient second, string secondName, string again)
    {
        await context.StepAsync(
            $"The first player asks the second for a trade{again}",
            "The second player gets the request: a dialog asks whether to trade.",
            () => Trading.RequestAsync(first, second, secondName),
            [Second]);
        await context.StepAsync(
            "The second player accepts with Enter",
            "The trade window opens for both characters.",
            () => Trading.AcceptAsync(context, first, second));
    }

    private static async Task<Holdings> HoldingsAsync(GameClient first, GameClient second, string jewelName)
        => new(await first.ZenAsync(), await second.ZenAsync(), await first.CountAsync(jewelName));

    private static async Task ExpectKeptAsync(GameClient first, GameClient second, ItemSlot jewel, Holdings before)
    {
        await first.OpenInventoryAsync();
        await ExpectZenAsync(second, before.SecondZen, "after the cancel");
        await ExpectZenAsync(first, before.FirstZen, "after the cancel");
        var jewels = 0;
        await Expect.EventuallyAsync(
            async () => (jewels = await first.CountAsync(jewel.Name)) == before.FirstJewels,
            ServerAnswer,
            () => $"the first player has {jewels} '{jewel.Name}' after the cancel, not {before.FirstJewels}");
        Expect.That(
            ItemSlots.At(ItemSlots.Of(await first.StateAsync(), "inventory"), jewel.Slot)?.Name == jewel.Name,
            $"'{jewel.Name}' is not back in the first player's slot {jewel.Slot}");
        await Expect.StillAfterAsync(
            async () => await first.ZenAsync() == before.FirstZen
                        && await second.ZenAsync() == before.SecondZen
                        && await first.CountAsync(jewel.Name) == before.FirstJewels,
            Settle,
            $"{Settle.TotalSeconds:0} s after the cancel the zen or the jewels changed: something came back twice");
    }

    private static async Task ExpectZenAsync(GameClient client, long expected, string when)
    {
        var zen = 0L;
        await Expect.EventuallyAsync(
            async () => (zen = await client.ZenAsync()) == expected,
            ServerAnswer,
            () => $"the {client.Role} player has {zen:N0} zen {when}, not {expected:N0}");
    }

    private static async Task<ItemSlot> FindJewelAsync(GameClient client)
    {
        var inventory = ItemSlots.Of(await client.StateAsync(), "inventory");
        return inventory.FirstOrDefault(item => item.Name.StartsWith("Jewel of", StringComparison.Ordinal))
               ?? throw new ScenarioFailedException("the first player has no jewel to offer; the OpenMU test data gives test8Dl some");
    }

    private sealed record Holdings(long FirstZen, long SecondZen, int FirstJewels);
}
