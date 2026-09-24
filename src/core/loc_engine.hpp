#pragma once
// loc_engine.hpp — working-tree line counting (delta & LOC engine).
#include <string>

namespace dometrics::loc {

struct TreeStats {
  long files = 0;
  long lines = 0; // total lines across counted source files
};

bool is_skipped_dir(const std::string& name);
bool is_counted_file(const std::string& filename);

// Recursively count lines in <root>, skipping .git, build dirs, binaries, etc.
TreeStats count_tree(const std::string& root);

// Count '\n'-terminated lines in a single file. -1 on error/unreadable.
long count_file_lines(const std::string& filepath);

} // namespace dometrics::loc
