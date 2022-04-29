#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include "src/array_hopper.h"
#include "src/bits.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/mt.h"
#include "src/ranking.h"
#include "src/simple_builder.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/simple_txt.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] \n";
}

int main(int argc, char **argv) {
  cottontail::Mt mt;
  char *line;
  std::string error;
  while ((line = readline(">> ")) != nullptr) {
    if (strlen(line) > 0) {
      add_history(line);
    } else {
      continue;
    }

    if (mt.infix_expression(std::string(line), &error))
      std::cout << mt.s_expression() << "\n";
    else
      std::cout << "error: " << error << "\n";
  }
  return 1;
}
