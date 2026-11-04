#pragma once

class PlayerController;

namespace GameSounds
{
void Init();

void Update();

bool PlayAt(const string& cue, const shared_ptr<GameObject>& unit, float volume = 1.f);
bool IsHiddenFromMe(const shared_ptr<GameObject>& unit);

void Cast(const shared_ptr<GameObject>& caster, int32 skillId);

void MissileLaunch(const shared_ptr<GameObject>& caster, const shared_ptr<GameObject>& target, int32 skillId = 0,
                   int32 variant = 0);

void Hit(const shared_ptr<GameObject>& source, int32 skillId, const shared_ptr<GameObject>& target);
void Death(const shared_ptr<GameObject>& unit, bool mine);
// A unit appeared: epic monsters roar, wards chime.
void Spawn(const shared_ptr<GameObject>& unit);
void Summoner(int32 spell, const shared_ptr<GameObject>& caster, const shared_ptr<GameObject>& target);

void Order(PlayerController& me, bool attack);

string ChampionName(int32 champType);
// The first of the candidates the manifest has ("" for none).
string FirstCue(std::initializer_list<string> candidates);
}
