// 앱 시작 지점. 서비스와 ViewModel을 만들어 창에 연결한다.
using Launcher.Services;
using Launcher.ViewModels;
using System.IO;
using System.Windows;

namespace Launcher
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            var window = new MainWindow();
            Action<Action> dispatch = action => Dispatcher.BeginInvoke(action);
            var options = new StartupOptions(e.Args);
            RosterData roster = RosterData.Load(Path.Combine(AppContext.BaseDirectory, "Assets"));
            var viewModel = new MainViewModel(options, roster, () => new LobbyConnection(dispatch), new GameLauncher(dispatch), window);
            window.DataContext = viewModel;
            window.Loaded += async (_, _) => await viewModel.StartAsync();
            window.Closed += (_, _) => viewModel.Dispose();
            window.Show();
        }
    }
}
