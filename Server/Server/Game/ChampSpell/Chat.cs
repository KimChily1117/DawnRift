using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Text;

public sealed class ChatState
{
    public readonly Queue<long> Recent = new Queue<long>();
}

public static class Chat
{
    public const int MaxLength = 150;
    public const int MaxPerWindow = 5;
    public const double WindowSeconds = 4.0;

    public static string Clean(string text)
    {
        if (string.IsNullOrEmpty(text))
            return "";
        var builder = new StringBuilder(Math.Min(text.Length, MaxLength));

        foreach (char c in text)
        {
            if (char.IsControl(c) && c != '\t')
                continue;
            builder.Append(c == '\t' ? ' ' : c);

            if (builder.Length >= MaxLength)
                break;
        }

        return builder.ToString().Trim();
    }

    public static bool Send(GameRoom room, Player sender, CHAT_CHANNEL channel, string text)
    {
        if (room == null || sender?.Info == null || !Progression.IsChampion(sender))
            return false;
        string line = Clean(text);

        if (line.Length == 0)
            return false;
        ChatState state = sender.ChatRate;
        long now = StatusEffects.Now;
        long window = (long)(WindowSeconds * System.Diagnostics.Stopwatch.Frequency);

        while (state.Recent.Count > 0 && now - state.Recent.Peek() > window)
            state.Recent.Dequeue();

        if (state.Recent.Count >= MaxPerWindow)
            return false;
        state.Recent.Enqueue(now);

        if (channel != CHAT_CHANNEL.ChatAll)
            channel = CHAT_CHANNEL.ChatTeam;
        var packet = new S_ChatMessage {
            SenderId = sender.Info.ObjectId, SenderName = sender.Info.Name ?? "", Message = line, Channel = channel,
            Team = sender.Info.TeamId,
        };

        if (channel == CHAT_CHANNEL.ChatAll)
            room.Broadcast(packet);
        else
            room.SendToTeam(sender.Info.TeamId, packet);
        Console.WriteLine(
            $"[Chat] {(channel == CHAT_CHANNEL.ChatAll ? "all" : "team " + sender.Info.TeamId)} {packet.SenderName}: {line}");

        return true;
    }
}
