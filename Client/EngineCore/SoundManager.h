#pragma once
#include "Fmod/fmod.hpp"
#include <deque>

enum class SoundGroup : uint8
{
    Announcer,
    Voice,
    Sfx,
    Ui,
    Ambience,
    Music,
    Count
};

class SoundManager
{
    DECLARE_SINGLE(SoundManager)

public:
    bool Init();
    void Update();

    void Release();

    bool LoadSound(const std::string& name, const std::string& filepath, bool loop = false);
    void PlaySound(const std::string& name, float volume = 1.0f);
    void StopSound(const std::string& name);
    void PauseSound(const std::string& name, bool pause);
    void SetVolume(const std::string& name, float volume);

    bool LoadCues(const std::wstring& manifestPath, const std::wstring& resourceRoot);

    const std::string& GetAudioCapturePath() const
    {
        return _audioCapturePath;
    }

    double GetAudioCaptureStartedMs() const
    {
        return _audioCaptureStartedMs;
    }

    bool HasCue(const std::string& cue) const;
    bool PlayCue(const std::string& cue, float volume = 1.f);
    bool PlayCueAt(const std::string& cue, const Vec3& position, float volume = 1.f);

    void PlayLoop(const std::string& cue, float volume = 1.f);

    void QueueAnnouncer(const std::string& cue);
    bool IsAnnouncerBusy() const;

    void SetListener(const Vec3& position, const Vec3& right);
    void SetGroupVolume(SoundGroup group, float volume);
    float GetGroupVolume(SoundGroup group) const;
    void SetMasterVolume(float volume);

    float GetMasterVolume() const
    {
        return _master;
    }

    static float DistanceGain(float distance);
    static constexpr float NearDistance = 8.f;
    static constexpr float FarDistance = 26.f;
    static constexpr int32 MaxInstancesPerCue = 3;
    static constexpr int32 MaxChannelsPerGroup = 24;

    struct PlayRecord
    {
        std::string cue;
        SoundGroup group = SoundGroup::Sfx;
        bool positional = false;
        Vec3 position = Vec3(0.f);
        float gain = 1.f;
        std::string skipped;
    };

    const std::vector<PlayRecord>& GetPlayLog() const
    {
        return _playLog;
    }

    void RecordSkipped(const std::string& cue, const Vec3& position, const char* reason)
    {
        Record(cue, SoundGroup::Sfx, true, position, 0.f, reason);
    }

    void ClearPlayLog()
    {
        _playLog.clear();
    }

    const std::string& GetLastError() const
    {
        return _lastError;
    }

    FMOD::System* system = nullptr;
    std::unordered_map<std::string, FMOD::Sound*> sounds;
    std::unordered_map<std::string, FMOD::Channel*> channels;

    bool _running = true;
    std::string _audioCapturePath;

    double _audioCaptureStartedMs = 0.0;

private:
    struct Cue
    {
        SoundGroup group = SoundGroup::Sfx;
        std::vector<std::string> files;
        int32 lastPick = -1;
        std::vector<FMOD::Channel*> playing;
    };

    FMOD::Sound* CueSound(const std::string& file, bool stream);
    FMOD::Channel* StartCue(const std::string& name, Cue& cue, float gain, float pan, bool loop);
    void Record(const std::string& cue, SoundGroup group, bool positional, const Vec3& position, float gain,
                const char* skipped);
    static bool IsPlaying(FMOD::Channel* channel);

    std::unordered_map<std::string, Cue> _cues;
    std::unordered_map<std::string, FMOD::Sound*> _cueSounds;
    std::wstring _resourceRoot;
    FMOD::ChannelGroup* _groups[static_cast<int32>(SoundGroup::Count)] = {};
    float _groupVolumes[static_cast<int32>(SoundGroup::Count)] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    float _master = 1.f;
    Vec3 _listener = Vec3(0.f);
    Vec3 _listenerRight = Vec3(1.f, 0.f, 0.f);
    std::deque<std::string> _announcerQueue;
    FMOD::Channel* _announcerChannel = nullptr;
    FMOD::Channel* _loopChannel = nullptr;
    std::string _loopCue;
    std::vector<PlayRecord> _playLog;
    std::string _lastError;
    uint32 _random = 0x9E3779B9u;
};
