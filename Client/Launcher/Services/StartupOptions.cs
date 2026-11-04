// 실행 인자를 읽는다. 서버 주소 지정과 통합 테스트 자동 실행에 쓴다.
namespace Launcher.Services
{
    public sealed class StartupOptions
    {
        private readonly Dictionary<string, string> _values = new Dictionary<string, string>();

        public StartupOptions(string[] args)
        {
            for (int i = 0; i < args.Length; ++i)
            {
                if (!args[i].StartsWith("--"))
                    continue;
                bool takesValue = args[i] == "--client-args" || (i + 1 < args.Length && !args[i + 1].StartsWith("--"));
                _values[args[i]] = takesValue && i + 1 < args.Length ? args[++i] : "";
            }
        }

        public bool Has(string key) => _values.ContainsKey(key);

        public bool TryGet(string key, out string value) => _values.TryGetValue(key, out value);
    }
}
