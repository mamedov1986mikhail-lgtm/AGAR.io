#pragma once
#include <string>
#include <utility>
#include <vector>

namespace agar 
{
struct RecordEntry 
{
    std::string name;
    unsigned weight = 0;
    int seconds = 0;
};

class Records 
{
public:
    explicit Records(std::string path = "") : path_(std::move(path)) {}

    void load();
    void save() const;
    bool qualifies(unsigned weight, int seconds) const;
    int add(const std::string& name, unsigned weight, int seconds);

    const std::vector<RecordEntry>& entries() const { return entries_; }

private:
    static bool better(const RecordEntry& a, const RecordEntry& b);

    std::string path_;
    std::vector<RecordEntry> entries_;
};
}
