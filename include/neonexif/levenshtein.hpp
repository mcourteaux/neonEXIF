#pragma once

#include <cstdint>
#include <cctype>
#include <string_view>
#include <algorithm>

namespace nexif {

struct LevenshteinCosts {
  int deletion{1};
  int insertion{1};
  int capitalizaton{1};
  int substitution{1};
};

// Returns the Levenshtein distance between word1 and word2.
static inline int levenshtein_distance(std::string_view word1, std::string_view word2, LevenshteinCosts costs)
{
  int size1 = (int)word1.size();
  int size2 = (int)word2.size();

  if (size1 == 0) {
    return size2 * costs.insertion;
  }
  if (size2 == 0) {
    return size1 * costs.deletion;
  }

  uint16_t *dist_matrix = (uint16_t *)
#if _MSC_VER
    _alloca
#else
    __builtin_alloca
#endif
    ( (size1 + 1) * (size2 + 1) * sizeof(uint16_t));

#define ACCESS_DM(i1, i2) dist_matrix[((i1) * (size2 + 1)) + (i2)]

  for (int i = 0; i <= size1; i++) {
    ACCESS_DM(i, 0) = i * costs.deletion;
  }
  for (int j = 0; j <= size2; j++) {
    ACCESS_DM(0, j) = j * costs.insertion;
  }

  for (int i = 1; i <= size1; i++) {
    for (int j = 1; j <= size2; j++) {
      int cost = 0;
      if (word2[j - 1] != word1[i - 1]) {
        if (::tolower(word2[j - 1]) == ::tolower(word1[i - 1])) {
          cost = costs.capitalizaton;
        } else {
          cost = costs.substitution;
        }
      }

      // a = the upper adjacent value plus 1: verif[i - 1][j] + deletion_cost
      // b = the left adjacent value plus 1: verif[i][j - 1] + insertion_cost
      // c = the upper left adjacent value plus the modification cost: verif[i - 1][j - 1] + cost
      ACCESS_DM(i, j) = std::min({
        ACCESS_DM(i - 1, j) + costs.deletion,
        ACCESS_DM(i, j - 1) + costs.insertion,
        ACCESS_DM(i - 1, j - 1) + cost,
      });
    }
  }

  int result = ACCESS_DM(size1, size2);
  return result;
#undef ACCESS_DM
}

}  // namespace nexif
