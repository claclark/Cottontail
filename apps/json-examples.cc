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

void timestamp(size_t number, cottontail::addr t) {
  if (t == 0)
    std::cout << "@example " << number << ": <1 ms\n";
  else
    std::cout << "@example " << number << ": " << t << " ms\n";
}

// Example 0
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
  timestamp(0, t1 - t0 + 1);
  if (verbose)
    for (auto &result : results)
      std::cout << result.first << ": " << result.second << "\n";
  std::flush(std::cout);
}

// Example 1
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
  timestamp(1, t1 - t0);
  if (verbose)
    std::cout << min_rating << ", " << total_ratings / restaurants << ", "
              << max_rating << "\n";
  std::flush(std::cout);
}

// Example 2
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
  timestamp(2, t1 - t0);
  if (verbose)
    std::cout << count << "\n";
  std::flush(std::cout);
}

// Example 3
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
  timestamp(3, t1 - t0);
  if (verbose)
    for (auto &result : results)
      std::cout << result << "\n";
  std::flush(std::cout);
}

// Example 4
// Titles and authors of books
// SELECT title, EXPLODE(authors) AS author FROM books
void example4(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::unique_ptr<cottontail::Hopper> books =
      warren->hopper_from_gcl("(<< : Files/books.json)");
  std::unique_ptr<cottontail::Hopper> title =
      warren->hopper_from_gcl(":title:");
  std::vector<std::unique_ptr<cottontail::Hopper>> authors;
  std::vector<std::pair<std::string, std::string>> results;
  cottontail::addr p, q;
  for (books->tau(cottontail::minfinity + 1, &p, &q); q < cottontail::maxfinity;
       books->tau(p + 1, &p, &q)) {
    cottontail::addr p_title, q_title;
    title->tau(p, &p_title, &q_title);
    if (q_title > q)
      continue;
    std::string the_title = warren->txt()->translate(p_title, q_title);
    if (the_title == "\"\"")
      continue;
    for (size_t i = 0;; i++) {
      if (authors.size() <= i) { // EXPLODE
        std::string feature = ":authors:[" + std::to_string(i) + "]:";
        authors.emplace_back(warren->hopper_from_gcl(feature));
      }
      cottontail::addr p_author, q_author;
      authors[i]->tau(p, &p_author, &q_author);
      if (q_author > q)
        break;
      std::string the_author = warren->txt()->translate(p_author, q_author);
      if (the_author == "\"\" ")
        continue;
      results.emplace_back(the_title, the_author);
    }
  }
  cottontail::addr t1 = cottontail::now();
  timestamp(4, t1 - t0);
  if (verbose)
    for (auto &&result : results)
      std::cout << result.first << ", " << result.second << "\n";
  std::flush(std::cout);
}

// Example 5
// How many stock trades?
// SELECT COUNT(*) FROM trades
void example5(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< : Files/trades.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  size_t count = 0;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    count++;
  cottontail::addr t1 = cottontail::now();
  timestamp(5, t1 - t0);
  if (verbose)
    std::cout << count << "\n";
  std::flush(std::cout);
}

// Example 6
// How many rows in the database?
// SELECT COUNT(*) FROM *
void example6(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  cottontail::addr count =
      warren->idx()->count(warren->featurizer()->featurize(":"));
  cottontail::addr t1 = cottontail::now();
  timestamp(6, t1 - t0);
  if (verbose)
    std::cout << count << "\n";
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
  example0(warren, false);
  example1(warren, false);
  example2(warren, false);
  example3(warren, false);
  example4(warren, false);
  example5(warren, false);
  example6(warren, false);
  warren->end();
  return 0;
}
