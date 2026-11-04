// 포지션과 소환사 주문의 표시 이름. 게임 규칙과 무관한 화면용 데이터다.
using Google.Protobuf.Protocol;

namespace Launcher.Services
{
    public static class GameCatalog
    {
        public static readonly (LOBBY_POSITION Position, string Key, string Label)[] Positions = {
            (LOBBY_POSITION.PositionNone, "", "없음"),
            (LOBBY_POSITION.PositionTop, "TOP", "탑"),
            (LOBBY_POSITION.PositionJungle, "JUNGLE", "정글"),
            (LOBBY_POSITION.PositionMid, "MID", "미드"),
            (LOBBY_POSITION.PositionBottom, "BOTTOM", "원딜"),
            (LOBBY_POSITION.PositionSupport, "SUPPORT", "서포터"),
        };

        public static readonly (SUMMONER_SPELL Spell, string Key, string Label)[] Spells = {
            (SUMMONER_SPELL.SummonerFlash, "Flash", "점멸"),
            (SUMMONER_SPELL.SummonerSmite, "Smite", "강타"),
            (SUMMONER_SPELL.SummonerIgnite, "Ignite", "점화"),
            (SUMMONER_SPELL.SummonerHeal, "Heal", "회복"),
            (SUMMONER_SPELL.SummonerGhost, "Ghost", "유체화"),
        };

        public static string DifficultyLabel(BOT_DIFFICULTY difficulty) =>
            difficulty == BOT_DIFFICULTY.Easy ? "쉬움" : "보통";
    }
}
