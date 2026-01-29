#ifndef IroonRook_LEARNING_H
#define IroonRook_LEARNING_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Prometheus {
namespace Search {

struct ExperienceEntry {
  uint64_t key;
  int16_t score;
  uint8_t depth;
  uint8_t confidence;
  uint8_t last_generation;
};

class ExperienceCache {
public:
  ExperienceCache();
  ~ExperienceCache();

  void load(const std::string &filename);
  void save(const std::string &filename);

  bool probe(uint64_t key, int &score, int &depth);
  void record(uint64_t key, int score, int depth);
  void decay();

private:
  std::unordered_map<uint64_t, ExperienceEntry> entries;
  std::mutex mutex;
  bool modified = false;
};

extern ExperienceCache GlobalExperience;

} // namespace Search
} // namespace Prometheus

#endif
