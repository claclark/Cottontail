// Example JSON queries based on the curated collection at:
// https://github.com/ozlerhakan/mongodb-json-files
//
// Index built with "jsonl Files/*"
//
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] \n";
}

// Outcomes from city inspections
// SELECT result, COUNT(result) FROM city_inspections GROUP BY result
void example0(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< :result: Files/city_inspections.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  std::map<std::string, size_t> results;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string result = warren->txt()->translate(p, q);
    auto f = results.find(result);
    if (f == results.end())
      results[result] = 1;
    else
      results[result]++;
  }
  cottontail::addr t1 = cottontail::now();
  std::cout << "0 " << t1 - t0 << "\n";
  if (verbose)
    for (auto &result : results)
      std::cout << result.first << ": " << result.second << "\n";
  std::flush(std::cout);
}

// Some statistics on restaurant ratings
// SELECT MIN(rating), AVG(rating), MAX(rating) FROM restaurants
void example1(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< rating: Files/restaurant.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::fval min_rating, max_rating, total_ratings, restaurants = 1;
  cottontail::addr p, q;
  cottontail::fval v;
  hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
  if (p == cottontail::maxfinity) {
    min_rating = max_rating = total_ratings = 0.0;
  } else {
    min_rating = max_rating = total_ratings = v;
    for (; p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v)) {
      min_rating = std::min(min_rating, v);
      max_rating = std::max(max_rating, v);
      total_ratings += v;
      restaurants++;
    }
  }
  cottontail::addr t1 = cottontail::now();
  std::cout << "1 " << t1 - t0 << "\n";
  if (verbose)
    std::cout << min_rating << ", " << total_ratings / restaurants << ", "
              << max_rating << "\n";
  std::flush(std::cout);
}

// How many zip codes does New York have?
// SELECT COUNT(*) FROM zips WHERE CITY = "NEW YORK"
void example2(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< (>> :city: \"NEW YORK\") Files/zips.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  size_t count = 0;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    if (q - p == 3) // exact match
      count++;
  cottontail::addr t1 = cottontail::now();
  std::cout << "2 " << t1 - t0 << "\n";
  if (verbose)
    std::cout << count << "\n";
  std::flush(std::cout);
}

// Names of nanotech companies
// SELECT name FROM companies WHERE category_code CONTAINS "nanotech"
void example3(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< :name: (>> : (<< nanotech (<< :category_code: "
                      "Files/companies.json)))))";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  std::vector<std::string> results;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    results.emplace_back(warren->txt()->translate(p, q));
  cottontail::addr t1 = cottontail::now();
  std::cout << "3 " << t1 - t0 << "\n";
  if (verbose)
    for (auto &result : results)
      std::cout << result << "\n";
  std::flush(std::cout);
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = "/data/hdd2/claclark/JSON/json.burrow";
  if (argc == 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  } else if (argc != 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  example0(warren, true);
  example1(warren, true);
  example2(warren, true);
  example3(warren, true);
  warren->end();
  return 0;
}
