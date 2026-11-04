// 지난 경기 팝업.
using Launcher.Mvvm;
using Launcher.Services;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class HistoryViewModel : ObservableObject
    {
        private readonly IGameLauncher _game;
        private IReadOnlyList<string> _lines = Array.Empty<string>();
        private bool _isOpen;

        public HistoryViewModel(IGameLauncher game)
        {
            _game = game;
            OpenCommand = new RelayCommand(Open);
        }

        public ICommand OpenCommand { get; }

        public IReadOnlyList<string> Lines
        {
            get => _lines;
            private set => Set(ref _lines, value);
        }

        public bool IsOpen
        {
            get => _isOpen;
            set => Set(ref _isOpen, value);
        }

        private void Open()
        {
            Lines = MatchHistoryReader.Read(_game.MatchHistoryFolder);
            IsOpen = true;
        }
    }
}
