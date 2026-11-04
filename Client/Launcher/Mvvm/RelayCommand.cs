// 버튼 동작과 실행 가능 여부를 ViewModel 메서드에 연결한다.
using System.Windows.Input;

namespace Launcher.Mvvm
{
    public sealed class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool> _canExecute;

        public RelayCommand(Action execute, Func<bool> canExecute = null)
        {
            _execute = execute;
            _canExecute = canExecute;
        }

        public event EventHandler CanExecuteChanged;

        public bool CanExecute(object parameter) => _canExecute?.Invoke() ?? true;

        public void Execute(object parameter) => _execute();

        // 상태가 바뀌면 버튼 활성화를 다시 계산하게 한다.
        public void Refresh() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}
