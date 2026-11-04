// 메인 창. 화면 로직은 ViewModel에 있고, 여기서는 창 제어만 맡는다.
using Launcher.Services;
using System.Windows;

namespace Launcher
{
    public partial class MainWindow : Window, IWindowService
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        public void Minimize() => WindowState = WindowState.Minimized;

        public void Restore() => WindowState = WindowState.Normal;

        public bool Confirm(string message, string title) =>
            MessageBox.Show(this, message, title, MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes;
    }
}
