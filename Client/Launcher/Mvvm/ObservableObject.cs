// 속성 변경을 화면에 알리는 ViewModel 기반 클래스.
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace Launcher.Mvvm
{
    public abstract class ObservableObject : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        // 값이 바뀐 경우에만 저장하고 알린다.
        protected bool Set<T>(ref T field, T value, [CallerMemberName] string name = null)
        {
            if (EqualityComparer<T>.Default.Equals(field, value))
                return false;
            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

            return true;
        }
    }
}
