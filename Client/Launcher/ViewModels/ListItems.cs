// 목록 한 줄을 나타내는 표시 전용 데이터.
using Launcher.Mvvm;
using System.Windows.Input;
using System.Windows.Media;

namespace Launcher.ViewModels
{
    public sealed record RoomItem(string Text, string ButtonText, bool CanJoin, ICommand JoinCommand);

    public enum SlotKind
    {
        Player,
        Me,
        Bot,
        Empty,
    }

    public sealed record SlotItem(SlotKind Kind, string Label, string Name);

    public sealed record FilterItem(string Label, bool IsActive, ICommand Command);

    public sealed record ChampionItem(ImageSource Icon, string Caption, string Tooltip, bool IsPlayable, bool IsSelected, double IconOpacity,
                                      bool CanSelect, ICommand Command);

    public sealed record SkinItem(ImageSource Image, string Caption, bool IsSelected, double ImageOpacity, bool CanSelect, ICommand Command);

    public sealed record PickRow(string Name, ImageSource Icon, double IconOpacity, string Detail, bool IsLocked, IReadOnlyList<ImageSource> SpellIcons);

    public sealed record PickPanel(string Header, bool IsBlueTeam, IReadOnlyList<PickRow> Rows);

    public sealed record SpellSlotItem(ImageSource Icon, string Key, string Tooltip, bool CanChoose, ICommand Command);

    public sealed record SpellChoiceItem(ImageSource Icon, string Label, bool IsSelected, ICommand Command);
}
