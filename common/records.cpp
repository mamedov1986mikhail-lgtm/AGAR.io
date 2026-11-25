#include "records.h"
#include "protocol.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace agar 
{
bool Records::better(const RecordEntry& a, const RecordEntry& b) 
{
    if (a.weight != b.weight) return a.weight > b.weight;
    return a.seconds > b.seconds;
}

void Records::load() 
{
    entries_.clear();
    if (path_.empty()) return;
    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) 
    {
        std::istringstream ss(line);
        RecordEntry e;
        if (ss >> e.name >> e.weight >> e.seconds) entries_.push_back(e);
    }
    std::stable_sort(entries_.begin(), entries_.end(), better);
    if (entries_.size() > static_cast<std::size_t>(kRecordsCount)) entries_.resize(kRecordsCount);
}

void Records::save() const 
{
    if (path_.empty()) return;
    std::ofstream out(path_, std::ios::trunc);
    for (const RecordEntry& e : entries_) out << e.name << ' ' << e.weight << ' ' << e.seconds << '\n';
}

bool Records::qualifies(unsigned weight, int seconds) const 
{
    if (entries_.size() < static_cast<std::size_t>(kRecordsCount)) return true;
    RecordEntry cand;
    cand.weight = weight;
    cand.seconds = seconds;
    return better(cand, entries_.back());
}

int Records::add(const std::string& name, unsigned weight, int seconds) 
{
    if (!qualifies(weight, seconds)) return 0;
    RecordEntry e{name, weight, seconds};
    auto pos = std::upper_bound(entries_.begin(), entries_.end(), e, better);
    pos = entries_.insert(pos, e);
    int place = static_cast<int>(pos - entries_.begin()) + 1;
    if (entries_.size() > static_cast<std::size_t>(kRecordsCount)) entries_.resize(kRecordsCount);
    return place;
}
}  
