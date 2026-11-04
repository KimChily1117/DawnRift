// 로그인 화면의 입력과 상태.
using Launcher.Mvvm;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class LoginViewModel : ObservableObject
    {
        private readonly RelayCommand _loginCommand;
        private string _name = "";
        private string _server = "127.0.0.1:8080";
        private string _message = "";
        private bool _isBusy;

        public LoginViewModel(Func<Task> login)
        {
            _loginCommand = new RelayCommand(async () => await login(), () => !_isBusy);
        }

        public ICommand LoginCommand => _loginCommand;

        public string Name
        {
            get => _name;
            set => Set(ref _name, value);
        }

        public string Server
        {
            get => _server;
            set => Set(ref _server, value);
        }

        public string Message
        {
            get => _message;
            set => Set(ref _message, value);
        }

        public bool IsBusy
        {
            get => _isBusy;
            set
            {
                if (Set(ref _isBusy, value))
                    _loginCommand.Refresh();
            }
        }
    }
}
