// 실행 인자로 지정한 통합 테스트 단계를 진행한다. 일반 실행에서는 아무 일도 하지 않는다.
using Google.Protobuf.Protocol;
using Launcher.Services;
using System.Globalization;

namespace Launcher.ViewModels
{
    public sealed class AutoRunner
    {
        private readonly StartupOptions _options;
        private readonly LobbySession _session;
        private readonly LobbyViewModel _lobby;
        private readonly IGameLauncher _game;
        private readonly Func<string> _playerName;
        private readonly HashSet<string> _done = new HashSet<string>();
        private DateTime _lastStep = DateTime.Now;
        private DateTime _botsShownAt = DateTime.MinValue;

        public AutoRunner(StartupOptions options, LobbySession session, LobbyViewModel lobby, IGameLauncher game, Func<string> playerName)
        {
            _options = options;
            _session = session;
            _lobby = lobby;
            _game = game;
            _playerName = playerName;
        }

        public double Pace => _options.TryGet("--auto-pace", out string pace) &&
                                      double.TryParse(pace, NumberStyles.Float, CultureInfo.InvariantCulture, out double seconds)
                                  ? seconds
                                  : 0.0;

        private bool Enabled => _options.Has("--auto-name");

        // 방 목록에서 방을 만들거나 열린 방에 참가한다.
        public void RunRoomStep()
        {
            if (!Enabled || _session.Rooms == null || _session.State != null)
                return;
            bool joiner = _options.TryGet("--auto-team", out string team) && team == "2" && !_options.Has("--auto-room");

            if (joiner)
            {
                RoomSummary open = _session.Rooms.Rooms.FirstOrDefault(r => r.Phase == LOBBY_PHASE.Lobby && r.Players < r.MaxPlayers);

                if (open != null && Once("join-" + open.RoomId))
                    _session.Send(LOBBY_ACTION.JoinRoom, open.RoomId);

                return;
            }

            if (Once("create-room"))
            {
                bool named = _options.TryGet("--auto-room", out string room) && room.Length > 0;
                _session.CreateRoom(named ? room : _playerName() + "의 방");
            }
        }

        // 현재 단계에 맞는 다음 입력을 보낸다.
        public void RunStateSteps()
        {
            if (!Enabled || _session.Me == null)
                return;
            double wait = Pace - (DateTime.Now - _lastStep).TotalSeconds;

            if (wait > 0.0)
            {
                Later(wait + 0.05, RunStateSteps);

                return;
            }

            switch (_session.State.Phase)
            {
            case LOBBY_PHASE.Lobby:
                RunLobby(_session.Me);
                break;
            case LOBBY_PHASE.ChampSelect:
                if (!_session.Me.Locked)
                    RunChampSelect(_session.Me);
                break;
            case LOBBY_PHASE.InGame:
                RunInGame(_session.Me);
                break;
            }
        }

        private void RunLobby(LobbyMember me)
        {
            S_LobbyState state = _session.State;

            if (_options.TryGet("--auto-team", out string team) && int.TryParse(team, out int wanted) && me.Team != wanted && Once("team"))
                _session.Send(LOBBY_ACTION.JoinTeam, wanted);

            if (_options.TryGet("--auto-position", out string position) && Once("position"))
            {
                var match = GameCatalog.Positions.FirstOrDefault(p => p.Key == position.ToUpperInvariant());
                _session.Send(LOBBY_ACTION.SetPosition, (int)match.Position);
            }

            if (me.Host && _options.Has("--auto-practice") && !state.Practice && Once("practice"))
                _session.Send(LOBBY_ACTION.TogglePractice, 1);

            if (me.Host && _options.Has("--auto-bots") && Once("bots"))
                _lobby.FillBots();
            bool botsReady = !_options.Has("--auto-bots") || state.BotsBlue + state.BotsRed > 0;

            if (botsReady && _botsShownAt == DateTime.MinValue)
                _botsShownAt = DateTime.Now;

            if (!me.Host || !_options.TryGet("--auto-start-when", out string count) || !int.TryParse(count, out int players) ||
                state.Members.Count < players || !botsReady)
                return;
            double wait = _options.Has("--auto-bots") ? 3.0 - (DateTime.Now - _botsShownAt).TotalSeconds : 0.0;

            if (wait > 0.0)
                Later(wait + 0.1, RunStateSteps);
            else if (Once("start"))
                _session.Send(LOBBY_ACTION.StartChampSelect);
        }

        private void RunChampSelect(LobbyMember me)
        {
            if (_options.TryGet("--auto-champion", out string champion) && int.TryParse(champion, out int type) &&
                me.Champion != (PLAYER_CHAMPION_TYPE)type && Once("champion"))
            {
                _session.Send(LOBBY_ACTION.HoverChampion, type);
            }
            else if (me.Champion != PLAYER_CHAMPION_TYPE.PlayerTypeNone && _options.TryGet("--auto-skin", out string skin) &&
                     int.TryParse(skin, out int skinId) && me.SkinId != skinId && Once("skin"))
            {
                _session.Send(LOBBY_ACTION.SelectSkin, skinId);
            }
            else if (_options.TryGet("--auto-spells", out string spells) && Once("spells"))
            {
                SelectSpells(spells);
            }
            else if (me.Champion != PLAYER_CHAMPION_TYPE.PlayerTypeNone && Once("lock"))
            {
                _session.Send(LOBBY_ACTION.LockIn);
            }
        }

        private void SelectSpells(string spellList)
        {
            string[] wanted = spellList.Split(',');

            for (int slot = 0; slot < Math.Min(2, wanted.Length); ++slot)
            {
                string key = wanted[slot].Trim();
                var match = GameCatalog.Spells.FirstOrDefault(s => string.Equals(s.Key, key, StringComparison.OrdinalIgnoreCase));

                if (match.Spell != SUMMONER_SPELL.SummonerNone)
                    _session.Send(slot == 0 ? LOBBY_ACTION.SelectSpellD : LOBBY_ACTION.SelectSpellF, (int)match.Spell);
            }
        }

        private void RunInGame(LobbyMember me)
        {
            if (_session.State.CanRejoin && !_game.IsRunning && _options.Has("--auto-rejoin") &&
                Once("rejoin-" + DateTime.Now.Ticks / TimeSpan.TicksPerMinute))
                Later(3.0, () => _session.Send(LOBBY_ACTION.RejoinGame));

            if (me.Host && _options.TryGet("--auto-destroy-after", out string after) && double.TryParse(after, out double seconds) && Once("destroy"))
                Later(seconds, () => _session.Send(LOBBY_ACTION.DestroyRoom));
        }

        private bool Once(string step)
        {
            if (!_done.Add(step))
                return false;
            _lastStep = DateTime.Now;

            return true;
        }

        private static async void Later(double seconds, Action action)
        {
            await Task.Delay(TimeSpan.FromSeconds(seconds));
            action();
        }
    }
}
