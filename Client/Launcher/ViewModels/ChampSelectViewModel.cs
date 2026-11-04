// 챔피언 선택 화면. 챔피언, 스킨, 소환사 주문의 선택 상태를 계산한다.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.IO;
using System.Windows.Input;
using System.Windows.Media;

namespace Launcher.ViewModels
{
    public sealed class ChampSelectViewModel : ObservableObject
    {
        private readonly LobbySession _session;
        private readonly RosterData _roster;
        private readonly Action<ImageSource> _setBackdrop;
        private readonly Dictionary<SUMMONER_SPELL, ImageSource> _spellIcons = new Dictionary<SUMMONER_SPELL, ImageSource>();
        private readonly RelayCommand _lockIn;
        private string _positionFilter = "";
        private string _headline = "챔피언을 선택하세요";
        private string _timer = "";
        private string _message = "";
        private string _lockText = "확정";
        private bool _isSpellPopupOpen;
        private PickPanel _myPicks;
        private PickPanel _enemyPicks;
        private IReadOnlyList<FilterItem> _filters = Array.Empty<FilterItem>();
        private IReadOnlyList<ChampionItem> _champions = Array.Empty<ChampionItem>();
        private IReadOnlyList<SkinItem> _skins = Array.Empty<SkinItem>();
        private IReadOnlyList<SpellSlotItem> _spellSlots = Array.Empty<SpellSlotItem>();
        private IReadOnlyList<SpellChoiceItem> _spellChoices = Array.Empty<SpellChoiceItem>();

        public ChampSelectViewModel(LobbySession session, RosterData roster, ChatViewModel chat, Action<ImageSource> setBackdrop)
        {
            _session = session;
            _roster = roster;
            _setBackdrop = setBackdrop;
            Chat = chat;
            _lockIn = new RelayCommand(() => _session.Send(LOBBY_ACTION.LockIn), CanLockIn);

            foreach (var (spell, key, _) in GameCatalog.Spells)
                _spellIcons[spell] = RosterData.Image(Path.Combine(AppContext.BaseDirectory, "Assets", "Summoners", key + ".png"));
        }

        public ChatViewModel Chat { get; }

        public ICommand LockInCommand => _lockIn;

        public string Headline
        {
            get => _headline;
            private set => Set(ref _headline, value);
        }

        public string Timer
        {
            get => _timer;
            private set => Set(ref _timer, value);
        }

        public string Message
        {
            get => _message;
            set => Set(ref _message, value);
        }

        public string LockText
        {
            get => _lockText;
            private set => Set(ref _lockText, value);
        }

        public bool IsSpellPopupOpen
        {
            get => _isSpellPopupOpen;
            set => Set(ref _isSpellPopupOpen, value);
        }

        public PickPanel MyPicks
        {
            get => _myPicks;
            private set => Set(ref _myPicks, value);
        }

        public PickPanel EnemyPicks
        {
            get => _enemyPicks;
            private set => Set(ref _enemyPicks, value);
        }

        public IReadOnlyList<FilterItem> Filters
        {
            get => _filters;
            private set => Set(ref _filters, value);
        }

        public IReadOnlyList<ChampionItem> Champions
        {
            get => _champions;
            private set => Set(ref _champions, value);
        }

        public IReadOnlyList<SkinItem> Skins
        {
            get => _skins;
            private set => Set(ref _skins, value);
        }

        public IReadOnlyList<SpellSlotItem> SpellSlots
        {
            get => _spellSlots;
            private set => Set(ref _spellSlots, value);
        }

        public IReadOnlyList<SpellChoiceItem> SpellChoices
        {
            get => _spellChoices;
            private set => Set(ref _spellChoices, value);
        }

        // 서버 상태를 화면 값으로 옮긴다.
        public void Update()
        {
            LobbyMember me = _session.Me;

            if (me == null)
                return;
            S_LobbyState state = _session.State;
            Timer = ((int)Math.Ceiling(state.SecondsLeft)).ToString();
            Headline = me.Locked ? "다른 소환사를 기다리는 중" : "챔피언을 선택하세요";
            MyPicks = BuildPicks(me.Team, true);
            EnemyPicks = BuildPicks(me.Team == 1 ? 2 : 1, false);
            Filters = GameCatalog.Positions.Select(BuildFilter).ToList();
            Champions = BuildChampions(me);

            RosterData.Champion mine = _roster.Find(me.Champion);
            Skins = mine == null ? Array.Empty<SkinItem>() : mine.Skins.Select(s => BuildSkin(s, me)).ToList();

            if (mine?.Skins.FirstOrDefault(s => s.Id == me.SkinId)?.Splash is ImageSource splash)
                _setBackdrop(splash);
            SpellSlots = BuildSpellSlots(me);
            LockText = me.Locked ? "확정됨" : "확정";
            _lockIn.Refresh();
        }

        private bool CanLockIn()
        {
            LobbyMember me = _session.Me;

            return me != null && !me.Locked && me.Champion != PLAYER_CHAMPION_TYPE.PlayerTypeNone;
        }

        private FilterItem BuildFilter((LOBBY_POSITION Position, string Key, string Label) position)
        {
            string filter = position.Key;

            return new FilterItem(filter == "" ? "전체" : position.Label, filter == _positionFilter, new RelayCommand(() =>
            {
                _positionFilter = filter;
                Update();
            }));
        }

        private PickPanel BuildPicks(int team, bool mine)
        {
            var rows = new List<PickRow>();

            foreach (LobbyMember member in _session.State.Members.Where(m => m.Team == team))
            {
                RosterData.Champion champion = _roster.Find(member.Champion);
                RosterData.Skin skin = champion?.Skins.FirstOrDefault(s => s.Id == member.SkinId);
                string detail = champion == null ? "선택 중..." : (skin?.Name ?? champion.Name) + (member.Locked ? "  ✔" : "");
                var spells = member.Spells.Select(s => _spellIcons.GetValueOrDefault(s)).ToList();
                rows.Add(new PickRow(member.Name, champion?.Icon, member.Locked ? 1.0 : 0.55, detail, member.Locked, spells));
            }

            return new PickPanel(mine ? "아군" : "상대", team == 1, rows);
        }

        private List<ChampionItem> BuildChampions(LobbyMember me)
        {
            var taken = _session.State.Members.Where(m => m.Team == me.Team && m.AccountId != _session.AccountId)
                .Select(m => m.Champion)
                .ToHashSet();
            var items = new List<ChampionItem>();

            foreach (RosterData.Champion champion in _roster.Champions.Where(c => _positionFilter == "" || c.Positions.Contains(_positionFilter)))
            {
                bool selectable = champion.Playable && !taken.Contains(champion.Type) && !me.Locked;
                bool selected = me.Champion == champion.Type;
                string caption = champion.Playable ? champion.Name : champion.Name + "\n준비 중";
                int type = (int)champion.Type;
                items.Add(new ChampionItem(champion.Icon, caption, champion.Title, champion.Playable, selected, selectable || selected ? 1.0 : 0.35, selectable,
                                           new RelayCommand(() => _session.Send(LOBBY_ACTION.HoverChampion, type))));
            }

            return items;
        }

        private SkinItem BuildSkin(RosterData.Skin skin, LobbyMember me)
        {
            string caption = skin.HasModel ? skin.Name : skin.Name + "\n(모델 준비 중)";
            int id = skin.Id;

            return new SkinItem(skin.LoadScreen, caption, me.SkinId == skin.Id, skin.HasModel ? 1.0 : 0.4, skin.HasModel && !me.Locked,
                                new RelayCommand(() => _session.Send(LOBBY_ACTION.SelectSkin, id)));
        }

        private List<SpellSlotItem> BuildSpellSlots(LobbyMember me)
        {
            var slots = new List<SpellSlotItem>(2);

            for (int slot = 0; slot < 2; ++slot)
            {
                SUMMONER_SPELL spell = me.Spells.Count > slot ? me.Spells[slot] : SUMMONER_SPELL.SummonerNone;
                int chosenSlot = slot;
                string label = GameCatalog.Spells.FirstOrDefault(s => s.Spell == spell).Label;
                slots.Add(new SpellSlotItem(_spellIcons.GetValueOrDefault(spell), slot == 0 ? "D" : "F", label, !me.Locked,
                                            new RelayCommand(() => OpenSpellChoices(chosenSlot))));
            }

            return slots;
        }

        private void OpenSpellChoices(int slot)
        {
            LobbyMember me = _session.Me;

            if (me == null)
                return;
            SpellChoices = GameCatalog.Spells.Select(s =>
            {
                bool selected = me.Spells.Count > slot && me.Spells[slot] == s.Spell;
                int value = (int)s.Spell;

                return new SpellChoiceItem(_spellIcons.GetValueOrDefault(s.Spell), s.Label, selected, new RelayCommand(() =>
                {
                    IsSpellPopupOpen = false;
                    _session.Send(slot == 0 ? LOBBY_ACTION.SelectSpellD : LOBBY_ACTION.SelectSpellF, value);
                }));
            }).ToList();
            IsSpellPopupOpen = true;
        }
    }
}
