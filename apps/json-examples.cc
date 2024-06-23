// Example JSON queries based on the curated collection at:
// https://github.com/ozlerhakan/mongodb-json-files
//
// Index built with "jsonl Files/*"
//
#include <ctime>
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
    std::cout << "@example " << number << ": " << t + 1 << " ms\n";
}

// Example 1
// Some statistics on restaurant ratings
// SELECT MIN(rating), AVG(rating), MAX(rating) FROM restaurants
void example1(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< :rating: Files/restaurant.json)";
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
// Outcomes from city inspections
// SELECT result, COUNT(result) FROM city_inspections GROUP BY result
void example6(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
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
  timestamp(6, t1 - t0 + 1);
  if (verbose)
    for (auto &result : results)
      std::cout << result.first << ": " << result.second << "\n";
  std::flush(std::cout);
}

// Example 7
// How many rows in the database?
// SELECT COUNT(*) FROM *
void example7(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  cottontail::addr count =
      warren->idx()->count(warren->featurizer()->featurize(":"));
  cottontail::addr t1 = cottontail::now();
  timestamp(7, t1 - t0);
  if (verbose)
    std::cout << count << "\n";
  std::flush(std::cout);
}

// Add consistent annotation for dates.
// Not very robust thanks to struct tm.

void annotate_date(std::shared_ptr<cottontail::Warren> warren,
                   cottontail::addr p, cottontail::addr q,
                   const std::string &year, const std::string &month,
                   const std::string day) {
  struct tm tm;
  try {
    tm.tm_year = std::stoi(year) - 1900;
    tm.tm_mon = std::stoi(month) - 1;
    tm.tm_mday = std::stoi(day);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -1;
  } catch (const std::invalid_argument &e) {
    return;
  }
  time_t t = mktime(&tm);
  if (t < 0)
    return;
  cottontail::fval v = (cottontail::fval)t;
  warren->annotator()->annotate(warren->featurizer()->featurize("created:"), p,
                                q, v);
  warren->annotator()->annotate(warren->featurizer()->featurize("year=" + year),
                                p, q);
  warren->annotator()->annotate(
      warren->featurizer()->featurize("month=" + month), p, q);
  warren->annotator()->annotate(warren->featurizer()->featurize("day=" + day),
                                p, q);
}

std::map<std::string, std::string> months = {
    {"Jan", "01"}, {"Feb", "02"}, {"Mar", "03"}, {"Apr", "04"},
    {"May", "05"}, {"Jun", "06"}, {"Jul", "07"}, {"Aug", "08"},
    {"Sep", "09"}, {"Oct", "10"}, {"Nov", "11"}, {"Dec", "12"}};

// "publishedDate" : { "$date" : "2009-04-01T00:00:00.000-0700" }
void annotate_books(std::shared_ptr<cottontail::Warren> warren) {
  std::string query = "(<< :publishedDate:$date: Files/books.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string date = warren->txt()->translate(p, q);
    annotate_date(warren, p, q, date.substr(1, 4), date.substr(6, 2),
                  date.substr(9, 2));
  }
}

// "date":"Feb 20 2015"
void annotate_city_inspections(std::shared_ptr<cottontail::Warren> warren) {
  std::string query = "(<< :date: Files/city_inspections.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string date = warren->txt()->translate(p, q);
    std::string day = date.substr(5, 2);
    if (day.substr(0, 1) == " ")
      day = "0" + day.substr(1, 1);
    annotate_date(warren, p, q, date.substr(8, 4), months[date.substr(1, 3)],
                  day);
  }
}

// "created_at" : { "$date" : 1180075887000 }
// "created_at" : "Fri May 25 19:30:28 UTC 2007"
void annotate_companies(std::shared_ptr<cottontail::Warren> warren) {
  std::string query = "(<< :created_at:$date: Files/companies.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::addr p, q;
  cottontail::fval v;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v)) {
    time_t t = ((time_t)v) / 1000;
    struct tm *lt = gmtime(&t);
    char buffer[64];
    strftime(buffer, 64, "%Y-%m-%d", lt);
    std::string date = buffer;
    annotate_date(warren, p, q, date.substr(0, 4), date.substr(5, 2),
                  date.substr(8, 2));
  }
  query = "(<< (!> :created_at: :created_at:$date:) Files/companies.json)";
  hopper = warren->hopper_from_gcl(query);
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string date = warren->txt()->translate(p, q);
    annotate_date(warren, p, q, date.substr(25, 4), months[date.substr(5, 3)],
                  date.substr(9, 2));
  }
}

// "ts":{"$date":"2012-11-20T20:02:24.386Z"}
void annotate_profiles(std::shared_ptr<cottontail::Warren> warren) {
  std::string query = "(<< :ts:$date: Files/profiles.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string date = warren->txt()->translate(p, q);
    annotate_date(warren, p, q, date.substr(1, 4), date.substr(6, 2),
                  date.substr(9, 2));
  }
}

// "time":{"$date":"2012-03-02T22:00:00.000Z"}
void annotate_trades(std::shared_ptr<cottontail::Warren> warren) {
  std::string query = "(<< :time:$date: Files/trades.json)";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string date = warren->txt()->translate(p, q);
    annotate_date(warren, p, q, date.substr(1, 4), date.substr(6, 2),
                  date.substr(9, 2));
  }
}

bool annotate_dates(std::shared_ptr<cottontail::Warren> warren,
                    std::string *error) {
  std::string has_dates;
  if (!warren->get_parameter("has_dates", &has_dates, error))
    return false;
  if (cottontail::okay(has_dates))
    return true;
  if (!warren->transaction(error))
    return false;
  annotate_books(warren);
  annotate_city_inspections(warren);
  annotate_companies(warren);
  annotate_profiles(warren);
  annotate_trades(warren);
  warren->ready();
  warren->commit();
  if (!warren->set_parameter("has_dates", "yes", error))
    return false;
  return true;
}

// Example 8
// Titles of books publised in 2008
// SELECT title FROM books
//     WHERE created >= '2008-01-01' AND created <= '2008-12-31'
void example8(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(<< :title: (>> Files/books.json year=2008))";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  std::vector<std::string> results;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    results.emplace_back(warren->txt()->translate(p, q));
  cottontail::addr t1 = cottontail::now();
  timestamp(8, t1 - t0);
  if (verbose)
    for (auto &result : results)
      std::cout << result << "\n";
  std::flush(std::cout);
}

// Example 9
// Count objects with a creation date of '2008-21-01'
// SELECT COUNT(*) FROM * WHERE created = '2008-12-01'
void example9(std::shared_ptr<cottontail::Warren> warren, bool verbose) {
  cottontail::addr t0 = cottontail::now();
  std::string query = "(>> : (^ year=2008 month=12 day=01))";
  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
  size_t count = 0;
  cottontail::addr p, q;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    count++;
  cottontail::addr t1 = cottontail::now();
  timestamp(9, t1 - t0);
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
  bool verbose = false;
  std::string burrow = "/data/hdd2/claclark/JSON/json.burrow";
  while (argc > 1) {
    if (argc > 2 &&
        (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
      burrow = argv[2];
      argc -= 2;
      argv += 2;
    } else if (argv[1] == std::string("-v") ||
               argv[1] == std::string("--verbose")) {
      verbose = true;
      --argc;
      argv++;
    } else {
      usage(program_name);
      return 1;
    }
  }
  if (argc != 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  };
  ;
  warren->start();
  example1(warren, verbose);
  example2(warren, verbose);
  example3(warren, verbose);
  example4(warren, verbose);
  example5(warren, verbose);
  example6(warren, verbose);
  example7(warren, verbose);
  if (!annotate_dates(warren, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->end();
  warren->start();
  example8(warren, verbose);
  example9(warren, verbose);
  warren->end();
  return 0;
}
