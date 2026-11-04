// 목록에 항목이 추가되면 마지막 줄로 스크롤한다. 코드 비하인드 없이 XAML에서 붙여 쓴다.
using System.Collections.Specialized;
using System.Windows;
using System.Windows.Controls;

namespace Launcher.Behaviors
{
    public static class AutoScroll
    {
        public static readonly DependencyProperty IsEnabledProperty = DependencyProperty.RegisterAttached(
            "IsEnabled", typeof(bool), typeof(AutoScroll), new PropertyMetadata(false, OnChanged));

        public static bool GetIsEnabled(DependencyObject target) => (bool)target.GetValue(IsEnabledProperty);

        public static void SetIsEnabled(DependencyObject target, bool value) => target.SetValue(IsEnabledProperty, value);

        private static void OnChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            if (target is not ListBox list || !(bool)e.NewValue)
                return;
            NotifyCollectionChangedEventHandler handler = (_, _) => ScrollToEnd(list);
            ((INotifyCollectionChanged)list.Items).CollectionChanged += handler;
            list.Unloaded += (_, _) => ((INotifyCollectionChanged)list.Items).CollectionChanged -= handler;
            list.Loaded += (_, _) => ScrollToEnd(list);
        }

        private static void ScrollToEnd(ListBox list)
        {
            if (list.Items.Count > 0)
                list.ScrollIntoView(list.Items[list.Items.Count - 1]);
        }
    }
}
