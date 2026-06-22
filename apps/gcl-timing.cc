#include <cstddef>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "gcl/optimizer.h"
#include "src/cottontail.h"

namespace {

struct Query {
  std::string label;
  std::string gcl;
};

struct Timing {
  bool okay = false;
  cottontail::addr milliseconds = 0;
  cottontail::addr matches = 0;
  std::string error;
};

Query query(const std::string &label, std::initializer_list<std::string> terms,
            std::size_t window = 0) {
  std::string gcl = "(<< (^";
  for (const auto &term : terms)
    gcl += " " + term;
  if (window == 0)
    window = terms.size();
  gcl += ") (# " + std::to_string(window) + "))";
  return {label, gcl};
}

const std::vector<Query> queries = {
    query("winnie-pooh", {"winnie", "the", "pooh"}),
    query("game-thrones", {"game", "of", "thrones"}),
    query("midsummer-dream", {"a", "midsummer", "night", "s", "dream"}),
    query("harry-truman", {"harry", "s", "truman"}),
    query("murder-orient", {"murder", "on", "the", "orient", "express"}),
    query("lord-rings", {"lord", "of", "the", "rings"}),
    query("war-worlds", {"war", "of", "the", "worlds"}),
    query("old-man-sea", {"old", "man", "and", "the", "sea"}),
    query("kill-mockingbird", {"to", "kill", "a", "mockingbird"}),
    query("cold-blood", {"in", "cold", "blood"}),
    query("catcher-rye", {"catcher", "in", "the", "rye"}),
    query("two-cities", {"tale", "of", "two", "cities"}),
    query("history-time", {"brief", "history", "of", "time"}),
    query("cholera", {"love", "in", "the", "time", "of", "cholera"}),
    query("solitude", {"hundred", "years", "of", "solitude"}),
    query("sound-fury", {"sound", "and", "fury"}),
    query("grapes-wrath", {"grapes", "of", "wrath"}),
    query("eighty-days", {"around", "the", "world", "in", "eighty", "days"}),
    query("center-earth", {"journey", "to", "the", "center", "of", "earth"}),
    query("call-wild", {"call", "of", "the", "wild"}),
    query("wild-things", {"where", "the", "wild", "things", "are"}),
    query("looking-glass", {"through", "the", "looking", "glass"}),
    query("wind-willows", {"wind", "in", "the", "willows"}),
    query("remains-day", {"remains", "of", "the", "day"}),
    query("seven-gables", {"house", "of", "the", "seven", "gables"}),
    query("high-castle", {"man", "in", "the", "high", "castle"}),
    query("electric-sheep", {"androids", "dream", "of", "electric", "sheep"}),
    query("hitchhiker", {"hitchhiker", "s", "guide", "to", "galaxy"}),
    query("lion-wardrobe", {"lion", "witch", "and", "wardrobe"}),
    query("western-front", {"all", "quiet", "on", "the", "western", "front"}),
    query("bell-tolls", {"for", "whom", "the", "bell", "tolls"}),
    query("sun-rises", {"the", "sun", "also", "rises"}),
};

/*
const std::vector<Query> queries = {
    {"winnie-pooh", "(<< (^ winnie the pooh) (# 3))"},
    {"game-thrones", "(<< (^ game of thrones) (# 3))"},
    {"midsummer-dream", "(<< (^ a midsummer dream) (# 5))"},
    {"harry-truman", "(<< (^ harry s truman) (# 3))"},
    {"murder-orient", "(<< (^ the orient express) (# 5))"},
    {"lord-rings", "(<< (^ lord the rings) (# 4))"},
    {"war-worlds", "(<< (^ war the worlds) (# 4))"},
    {"old-man-sea", "(<< (^ man the sea) (# 5))"},
    {"kill-mockingbird", "(<< (^ to kill mockingbird) (# 4))"},
    {"cold-blood", "(<< (^ in cold blood) (# 3))"},
    {"catcher-rye", "(<< (^ catcher the rye) (# 4))"},
    {"two-cities", "(<< (^ tale of cities) (# 4))"},
    {"history-time", "(<< (^ brief history of) (# 4))"},
    {"cholera", "(<< (^ love the cholera) (# 6))"},
    {"solitude", "(<< (^ hundred of solitude) (# 4))"},
    {"sound-fury", "(<< (^ sound and fury) (# 3))"},
    {"grapes-wrath", "(<< (^ grapes of wrath) (# 3))"},
    {"eighty-days", "(<< (^ the eighty days) (# 6))"},
    {"center-earth", "(<< (^ center the earth) (# 6))"},
    {"call-wild", "(<< (^ call the wild) (# 4))"},
    {"wild-things", "(<< (^ the wild things) (# 5))"},
    {"looking-glass", "(<< (^ the looking glass) (# 4))"},
    {"wind-willows", "(<< (^ wind the willows) (# 4))"},
    {"remains-day", "(<< (^ remains the day) (# 4))"},
    {"seven-gables", "(<< (^ seven the gables) (# 5))"},
    {"high-castle", "(<< (^ high the castle) (# 5))"},
    {"electric-sheep", "(<< (^ androids of sheep) (# 5))"},
    {"hitchhiker", "(<< (^ hitchhiker s galaxy) (# 5))"},
    {"lion-wardrobe", "(<< (^ witch and wardrobe) (# 4))"},
    {"western-front", "(<< (^ the western front) (# 6))"},
    {"bell-tolls", "(<< (^ bell the tolls) (# 5))"},
    {"sun-rises", "(<< (^ the sun rises) (# 4))"},
};
*/

/*
const std::vector<Query> queries = {
    query("latrine-suspenders", {"latrine", "the", "suspenders"}, 32),
    query("harpsichord-turnip", {"harpsichord", "the", "turnip"}, 32),
    query("barometer-cauliflower", {"barometer", "the", "cauliflower"}, 32),
    query("carburetor-velvet", {"carburetor", "the", "velvet"}, 32),
    query("sundial-polyester", {"sundial", "the", "polyester"}, 32),
    query("semaphore-lentils", {"semaphore", "the", "lentils"}, 32),
};
*/

constexpr int query_label_width = 24;

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow]\n";
}

std::shared_ptr<cottontail::Warren> activate(const std::string &burrow,
                                             std::string *error) {
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, error);
  if (warren != nullptr)
    warren->start();
  return warren;
}

Timing time_query(cottontail::Warren *warren, const Query &query) {
  Timing timing;
  cottontail::addr t0 = cottontail::now();
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(query.gcl, &timing.error);
  if (hopper == nullptr) {
    timing.milliseconds = cottontail::now() - t0;
    return timing;
  }
  cottontail::addr p, q;
  cottontail::fval v;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v))
    timing.matches++;
  timing.milliseconds = cottontail::now() - t0;
  timing.okay = true;
  return timing;
}

void print_timing(const Query &query, const Timing &timing) {
  std::cout << "  " << query.label << ": " << timing.milliseconds << " ms, "
            << timing.matches << " match(es)\n";
  std::cout << "    " << query.gcl << "\n";
}

bool run_phase(const std::string &program_name, const std::string &burrow,
               const std::string &label, std::vector<Timing> *timings) {
  std::string error;
  std::shared_ptr<cottontail::Warren> warren = activate(burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return false;
  }

  std::cout << label << "\n";
  timings->clear();
  timings->reserve(queries.size());
  for (const auto &query : queries) {
    Timing timing = time_query(warren.get(), query);
    if (!timing.okay) {
      std::cerr << program_name << ": " << query.gcl << ": " << timing.error
                << "\n";
      warren->end();
      return false;
    }
    print_timing(query, timing);
    timings->push_back(timing);
  }
  warren->end();
  return true;
}

std::string matches(const Timing &optimized, const Timing &unoptimized) {
  if (optimized.matches == unoptimized.matches)
    return std::to_string(optimized.matches);
  return std::to_string(optimized.matches) + "/" +
         std::to_string(unoptimized.matches);
}

void print_comparison(const std::vector<Timing> &optimized,
                      const std::vector<Timing> &unoptimized) {
  std::cout << "\ncomparison\n";
  std::cout << std::left << std::setw(query_label_width) << "query"
            << std::right
            << std::setw(14) << "optimized" << std::setw(16)
            << "unoptimized" << std::setw(12) << "delta"
            << std::setw(14) << "matches" << "\n";
  for (size_t i = 0; i < queries.size(); i++) {
    long long delta = static_cast<long long>(unoptimized[i].milliseconds) -
                      static_cast<long long>(optimized[i].milliseconds);
    std::cout << std::left << std::setw(query_label_width) << queries[i].label
              << std::right
              << std::setw(10) << optimized[i].milliseconds << " ms"
              << std::setw(12) << unoptimized[i].milliseconds << " ms"
              << std::setw(8) << delta << " ms" << std::setw(14)
              << matches(optimized[i], unoptimized[i]) << "\n";
  }
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = cottontail::DEFAULT_BURROW;
  for (int arg = 1; arg < argc; arg++) {
    std::string option = argv[arg];
    if (option == "--help") {
      usage(program_name);
      return 0;
    } else if (option == "-b" || option == "--burrow") {
      if (arg + 1 >= argc) {
        usage(program_name);
        return 1;
      }
      burrow = argv[++arg];
    } else {
      usage(program_name);
      return 1;
    }
  }

  std::cout << "burrow: " << burrow << "\n\n";

  cottontail::gcl::Optimizer::enable();
  std::vector<Timing> optimized;
  if (!run_phase(program_name, burrow, "optimization enabled", &optimized))
    return 1;

  cottontail::gcl::Optimizer::disable();
  std::vector<Timing> unoptimized;
  if (!run_phase(program_name, burrow, "\noptimization disabled", &unoptimized))
    return 1;

  print_comparison(optimized, unoptimized);
  cottontail::gcl::Optimizer::enable();
  return 0;
}
