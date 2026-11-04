#include "pch.h"
#include "SoundManager.h"
#include <chrono>

bool SoundManager::Init()
{
    FMOD_RESULT result = FMOD::System_Create(&system);

    if (result != FMOD_OK)
    {
        std::cerr << "FMOD System Create Failed!" << std::endl;

        return false;
    }

    char* wavPath = nullptr;
    size_t wavLength = 0;

    if (_dupenv_s(&wavPath, &wavLength, "DAWN_AUDIO_WAV") == 0 && wavPath && wavPath[0])
    {
        _audioCapturePath = wavPath;
        system->setOutput(FMOD_OUTPUTTYPE_WAVWRITER);
    }

    free(wavPath);
    result = system->init(512, FMOD_INIT_NORMAL,
                          _audioCapturePath.empty() ? nullptr : const_cast<char*>(_audioCapturePath.c_str()));

    if (!_audioCapturePath.empty())
        _audioCaptureStartedMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();

    if (result != FMOD_OK)
    {
        std::cerr << "FMOD System Init Failed!" << std::endl;

        return false;
    }

    static const char* const GroupNames[] = {"announcer", "voice", "sfx", "ui", "ambience", "music"};

    for (int32 i = 0; i < static_cast<int32>(SoundGroup::Count); ++i)
        system->createChannelGroup(GroupNames[i], &_groups[i]);

    return true;
}

void SoundManager::Release()
{
    for (auto& pair : sounds)
    {
        pair.second->release();
    }

    sounds.clear();

    for (auto& pair : _cueSounds)
        if (pair.second)
            pair.second->release();
    _cueSounds.clear();

    if (system)
    {
        system->close();
        system->release();
        system = nullptr;
    }
}

void SoundManager::Update()
{
    if (!system)
        return;

    // Next announcer line once the current one ended.
    if (!IsPlaying(_announcerChannel))
    {
        _announcerChannel = nullptr;

        while (!_announcerQueue.empty() && !_announcerChannel)
        {
            const std::string next = _announcerQueue.front();
            _announcerQueue.pop_front();
            auto it = _cues.find(next);

            if (it == _cues.end())
            {
                Record(next, SoundGroup::Announcer, false, Vec3(0.f), 0.f, "missing");
                continue;
            }

            _announcerChannel = StartCue(next, it->second, 1.f, 0.f, false);
            Record(next, SoundGroup::Announcer, false, Vec3(0.f), 1.f, _announcerChannel ? "" : "missing");
        }
    }

    system->update();
}

bool SoundManager::LoadSound(const std::string& name, const std::string& filepath, bool loop)
{
    if (sounds.find(name) != sounds.end())
        return true;

    FMOD::Sound* sound = nullptr;
    FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_DEFAULT;
    FMOD_RESULT result = system->createSound(filepath.c_str(), mode, nullptr, &sound);

    if (result != FMOD_OK)
    {
        std::cerr << "Failed to load sound: " << filepath << std::endl;

        return false;
    }

    sounds[name] = sound;

    return true;
}

void SoundManager::PlaySound(const std::string& name, float volume)
{
    if (sounds.find(name) == sounds.end())
        return;

    FMOD::Channel* channel = nullptr;
    system->playSound(sounds[name], _groups[static_cast<int32>(SoundGroup::Music)], false, &channel);

    if (channel)
    {
        channel->setVolume(volume);
        channels[name] = channel;
    }
}

void SoundManager::StopSound(const std::string& name)
{
    if (channels.find(name) == channels.end())
        return;

    channels[name]->stop();
    channels.erase(name);
}

void SoundManager::PauseSound(const std::string& name, bool pause)
{
    if (channels.find(name) == channels.end())
        return;

    channels[name]->setPaused(pause);
}

void SoundManager::SetVolume(const std::string& name, float volume)
{
    if (channels.find(name) == channels.end())
        return;

    channels[name]->setVolume(volume);
}

bool SoundManager::LoadCues(const std::wstring& manifestPath, const std::wstring& resourceRoot)
{
    std::ifstream file(manifestPath);
    json data = json::parse(file, nullptr, false);

    if (data.is_discarded() || !data.contains("events"))
        return false;
    static const std::unordered_map<std::string, SoundGroup> Groups = {{"announcer", SoundGroup::Announcer},
                                                                       {"voice", SoundGroup::Voice},
                                                                       {"sfx", SoundGroup::Sfx},
                                                                       {"ui", SoundGroup::Ui},
                                                                       {"ambience", SoundGroup::Ambience},
                                                                       {"music", SoundGroup::Music}};
    _resourceRoot = resourceRoot;

    for (const auto& [name, value] : data["events"].items())
    {
        Cue cue;
        auto group = Groups.find(value.value("group", "sfx"));
        cue.group = group != Groups.end() ? group->second : SoundGroup::Sfx;

        for (const auto& f : value["files"])
            cue.files.push_back(f.get<std::string>());

        if (!cue.files.empty())
            _cues[name] = std::move(cue);
    }

    return !_cues.empty();
}

bool SoundManager::HasCue(const std::string& cue) const
{
    return _cues.count(cue) > 0;
}

bool SoundManager::IsPlaying(FMOD::Channel* channel)
{
    bool playing = false;

    return channel && channel->isPlaying(&playing) == FMOD_OK && playing;
}

FMOD::Sound* SoundManager::CueSound(const std::string& file, bool stream)
{
    auto it = _cueSounds.find(file);

    if (it != _cueSounds.end())
        return it->second;
    std::filesystem::path path = std::filesystem::path(_resourceRoot) / file;
    FMOD::Sound* sound = nullptr;
    const FMOD_MODE mode = (stream ? FMOD_CREATESTREAM | FMOD_LOOP_NORMAL : FMOD_CREATESAMPLE) | FMOD_2D;

    if (FMOD_RESULT result = system->createSound(path.string().c_str(), mode, nullptr, &sound); result != FMOD_OK)
    {
        _lastError = "createSound " + path.string() + ": " + std::to_string(static_cast<int32>(result));
        sound = nullptr;
    }

    _cueSounds[file] = sound;

    return sound;
}

FMOD::Channel* SoundManager::StartCue(const std::string& name, Cue& cue, float gain, float pan, bool loop)
{
    if (!system || cue.files.empty())
        return nullptr;
    int32 pick = 0;

    if (cue.files.size() > 1)
    {
        _random = _random * 1664525u + 1013904223u;
        pick = static_cast<int32>((_random >> 8) % (cue.files.size() - 1));

        if (pick >= cue.lastPick && cue.lastPick >= 0)
            ++pick;
    }

    cue.lastPick = pick;
    FMOD::Sound* sound = CueSound(cue.files[pick], loop);

    if (!sound)
        return nullptr;
    FMOD::Channel* channel = nullptr;

    if (FMOD_RESULT result = system->playSound(sound, _groups[static_cast<int32>(cue.group)], true, &channel);
        result != FMOD_OK || !channel)
    {
        _lastError = "playSound " + name + ": " + std::to_string(static_cast<int32>(result));

        return nullptr;
    }

    channel->setVolume(gain);
    channel->setPan(pan);
    channel->setPaused(false);
    cue.playing.push_back(channel);

    return channel;
}

void SoundManager::Record(const std::string& cue, SoundGroup group, bool positional, const Vec3& position, float gain,
                          const char* skipped)
{
    if (_playLog.size() > 20000)
        _playLog.erase(_playLog.begin(), _playLog.begin() + 10000);
    _playLog.push_back({cue, group, positional, position, gain, skipped});
}

float SoundManager::DistanceGain(float distance)
{
    if (distance <= NearDistance)
        return 1.f;

    if (distance >= FarDistance)
        return 0.f;
    const float t = (distance - NearDistance) / (FarDistance - NearDistance);

    return (1.f - t) * (1.f - t);
}

bool SoundManager::PlayCue(const std::string& name, float volume)
{
    auto it = _cues.find(name);

    if (it == _cues.end())
    {
        Record(name, SoundGroup::Sfx, false, Vec3(0.f), 0.f, "missing");

        return false;
    }

    Cue& cue = it->second;
    cue.playing.erase(
        std::remove_if(cue.playing.begin(), cue.playing.end(), [](FMOD::Channel* c) { return !IsPlaying(c); }),
        cue.playing.end());

    if (static_cast<int32>(cue.playing.size()) >= MaxInstancesPerCue)
    {
        Record(name, cue.group, false, Vec3(0.f), volume, "instances");

        return false;
    }

    FMOD::Channel* channel = StartCue(name, cue, volume, 0.f, false);
    Record(name, cue.group, false, Vec3(0.f), volume, channel ? "" : "missing");

    return channel != nullptr;
}

bool SoundManager::PlayCueAt(const std::string& name, const Vec3& position, float volume)
{
    auto it = _cues.find(name);

    if (it == _cues.end())
    {
        Record(name, SoundGroup::Sfx, true, position, 0.f, "missing");

        return false;
    }

    Cue& cue = it->second;
    Vec3 offset = position - _listener;
    offset.y = 0.f;
    const float gain = DistanceGain(offset.Length()) * volume;

    if (gain <= 0.01f)
    {
        Record(name, cue.group, true, position, gain, "distance");

        return false;
    }

    cue.playing.erase(
        std::remove_if(cue.playing.begin(), cue.playing.end(), [](FMOD::Channel* c) { return !IsPlaying(c); }),
        cue.playing.end());

    if (static_cast<int32>(cue.playing.size()) >= MaxInstancesPerCue)
    {
        Record(name, cue.group, true, position, gain, "instances");

        return false;
    }

    int32 inGroup = 0;

    if (FMOD::ChannelGroup* group = _groups[static_cast<int32>(cue.group)])
        group->getNumChannels(&inGroup);

    if (inGroup >= MaxChannelsPerGroup)
    {
        Record(name, cue.group, true, position, gain, "group");

        return false;
    }

    const float side = offset.Dot(_listenerRight) / FarDistance;
    const float pan = std::clamp(side, -1.f, 1.f) * 0.7f;
    FMOD::Channel* channel = StartCue(name, cue, gain, pan, false);
    Record(name, cue.group, true, position, gain, channel ? "" : "missing");

    return channel != nullptr;
}

void SoundManager::PlayLoop(const std::string& name, float volume)
{
    if (name == _loopCue && IsPlaying(_loopChannel))
        return;

    if (_loopChannel)
        _loopChannel->stop();
    _loopChannel = nullptr;
    _loopCue = name;
    auto it = _cues.find(name);

    if (it == _cues.end())
        return;
    _loopChannel = StartCue(name, it->second, volume, 0.f, true);
    Record(name, it->second.group, false, Vec3(0.f), volume, _loopChannel ? "" : "missing");
}

void SoundManager::QueueAnnouncer(const std::string& cue)
{
    if (_announcerQueue.size() < 4)
        _announcerQueue.push_back(cue);
}

bool SoundManager::IsAnnouncerBusy() const
{
    return IsPlaying(_announcerChannel) || !_announcerQueue.empty();
}

void SoundManager::SetListener(const Vec3& position, const Vec3& right)
{
    _listener = position;
    Vec3 r(right.x, 0.f, right.z);

    if (r.LengthSquared() > 1e-6f)
    {
        r.Normalize();
        _listenerRight = r;
    }
}

void SoundManager::SetGroupVolume(SoundGroup group, float volume)
{
    const int32 i = static_cast<int32>(group);
    _groupVolumes[i] = std::clamp(volume, 0.f, 1.f);

    if (_groups[i])
        _groups[i]->setVolume(_groupVolumes[i]);
}

float SoundManager::GetGroupVolume(SoundGroup group) const
{
    return _groupVolumes[static_cast<int32>(group)];
}

void SoundManager::SetMasterVolume(float volume)
{
    _master = std::clamp(volume, 0.f, 1.f);
    FMOD::ChannelGroup* master = nullptr;

    if (system && system->getMasterChannelGroup(&master) == FMOD_OK && master)
        master->setVolume(_master);
}
