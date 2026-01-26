#include "learning.h"
#include <fstream>
#include <iostream>

namespace IroonRook {
namespace Search {

ExperienceCache GlobalExperience;

ExperienceCache::ExperienceCache() {}

ExperienceCache::~ExperienceCache() {
  // We could auto-save here, but better to do it explicitly via UCI
}

void ExperienceCache::load(const std::string &filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    return;

  std::lock_guard<std::mutex> lock(mutex);
  ExperienceEntry entry;
  while (in.read(reinterpret_cast<char *>(&entry), sizeof(ExperienceEntry))) {
    entries[entry.key] = entry;
  }
  std::cout << "info string Loaded " << entries.size() << " experience entries"
            << std::endl;
}

void ExperienceCache::save(const std::string &filename) {
  if (!modified)
    return;

  std::ofstream out(filename, std::ios::binary);
  if (!out)
    return;

  std::lock_guard<std::mutex> lock(mutex);
  for (auto const &[key, entry] : entries) {
    out.write(reinterpret_cast<const char *>(&entry), sizeof(ExperienceEntry));
  }
  modified = false;
  std::cout << "info string Saved " << entries.size() << " experience entries"
            << std::endl;
}

bool ExperienceCache::probe(uint64_t key, int &score, int &depth) {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = entries.find(key);
  if (it != entries.end()) {
    score = it->second.score;
    depth = it->second.depth;
    return true;
  }
  return false;
}

void ExperienceCache::record(uint64_t key, int score, int depth) {
  // Only record significant depths to avoid bloating
  if (depth < 8)
    return;

  std::lock_guard<std::mutex> lock(mutex);
  auto it = entries.find(key);
  if (it == entries.end()) {
    entries[key] = {key, (int16_t)score, (uint8_t)depth, 1, 0};
    modified = true;
  } else {
    if (depth >= it->second.depth) {
      it->second.score = (int16_t)score;
      it->second.depth = (uint8_t)depth;
      if (it->second.confidence < 255)
        it->second.confidence++;
      modified = true;
    }
  }
}

void ExperienceCache::decay() {
  std::lock_guard<std::mutex> lock(mutex);
  for (auto it = entries.begin(); it != entries.end();) {
    if (it->second.confidence > 1) {
      it->second.confidence /= 2;
      ++it;
    } else {
      it = entries.erase(it);
    }
  }
  modified = true;
}

} // namespace Search
} // namespace IroonRook
