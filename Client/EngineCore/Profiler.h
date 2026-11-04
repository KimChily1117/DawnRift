#pragma once
#include <chrono>
#include <iomanip>

class Profiler
{
public:
    static void Enable(const std::filesystem::path& reportPath)
    {
        _enabled = true;
        _reportPath = reportPath;
        _lastWrite = Now();
    }

    static bool IsEnabled()
    {
        return _enabled;
    }

    static double Now()
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static void Add(const string& name, double ms)
    {
        Section& section = Find(name);
        section.frame += ms;
        section.calls++;
    }

    static void Gauge(const string& name, double value)
    {
        if (!_enabled)
            return;
        Section& section = Find(name);
        section.gauge = true;
        section.frame = value;
    }

    static void EndFrame()
    {
        if (!_enabled)
            return;
        const double now = Now();

        if (_frameStart > 0.0)
            Add("frame", now - _frameStart);
        _frameStart = now;
        _frames++;
        _windowFrames++;
        RecordSpike();

        for (auto& [name, section] : _sections)
        {
            section.total += section.frame;
            section.window += section.frame;
            section.max = (std::max)(section.max, section.frame);
            section.windowMax = (std::max)(section.windowMax, section.frame);
            section.frame = 0.0;
        }

        if (now - _lastWrite >= 2000.0)
        {
            Write();
            _lastWrite = now;
            _windowFrames = 0;

            for (auto& [name, section] : _sections)
                section.window = section.windowMax = 0.0;
        }
    }

private:
    struct Section
    {
        double frame = 0.0, total = 0.0, max = 0.0, window = 0.0, windowMax = 0.0;
        uint64 calls = 0;
        bool gauge = false;
    };

    static void RecordSpike()
    {
        constexpr double SpikeMs = 50.0;
        auto frame = _sections.find("frame");

        if (frame == _sections.end() || frame->second.frame < SpikeMs || _spikes.size() >= 100)
            return;
        std::ostringstream line;
        line << std::fixed << std::setprecision(1) << "frame " << _frames << ": " << frame->second.frame << " ms";

        for (const string& name : _order)
        {
            const Section& s = _sections[name];

            if (!s.gauge && name != "frame" && s.frame >= frame->second.frame * 0.1)
                line << " | " << name << " " << s.frame;
        }

        _spikes.push_back(line.str());
    }

    static Section& Find(const string& name)
    {
        auto it = _sections.find(name);

        if (it == _sections.end())
        {
            _order.push_back(name);
            it = _sections.emplace(name, Section()).first;
        }

        return it->second;
    }

    static void Write()
    {
        std::ofstream out(_reportPath, std::ios::trunc);

        if (!out)
            return;
        const double frames = static_cast<double>(_frames > 0 ? _frames : 1);
        const double windowFrames = static_cast<double>(_windowFrames > 0 ? _windowFrames : 1);
        out << std::fixed << std::setprecision(3);
        out << "frames " << _frames << " (window " << _windowFrames << ")\n";
        out << "section                                   avg ms    max ms   win avg   win max  calls/frame\n";

        for (const string& name : _order)
        {
            const Section& s = _sections[name];
            out << std::left << std::setw(40) << name.substr(0, 40) << std::right << std::setw(10) << s.total / frames
                << std::setw(10) << s.max << std::setw(10) << s.window / windowFrames << std::setw(10) << s.windowMax;

            if (!s.gauge)
                out << std::setw(12) << s.calls / frames;
            else
                out << "       gauge";
            out << "\n";
        }

        out << "\nspikes (frames over 50 ms, sections over 10% of the frame)\n";

        for (const string& spike : _spikes)
            out << spike << "\n";
    }

    static inline bool _enabled = false;
    static inline std::filesystem::path _reportPath;
    static inline unordered_map<string, Section> _sections;
    static inline vector<string> _order;
    static inline vector<string> _spikes;
    static inline uint64 _frames = 0;
    static inline uint64 _windowFrames = 0;
    static inline double _frameStart = 0.0;
    static inline double _lastWrite = 0.0;
};

class ProfileScope
{
public:
    explicit ProfileScope(const char* name) : _name(name), _start(Profiler::IsEnabled() ? Profiler::Now() : 0.0) {}

    explicit ProfileScope(string name)
        : _owned(std::move(name)), _name(nullptr), _start(Profiler::IsEnabled() ? Profiler::Now() : 0.0)
    {
    }

    ~ProfileScope()
    {
        if (_start > 0.0)
            Profiler::Add(_name ? string(_name) : _owned, Profiler::Now() - _start);
    }

private:
    string _owned;
    const char* _name;

    double _start;
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) ProfileScope PROFILE_CONCAT(_profileScope, __LINE__)(name)
