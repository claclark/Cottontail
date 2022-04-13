#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/cottontail.h"

bool fetch_text(std::shared_ptr<cottontail::Warren> warren,
                std::string identifier, std::string *text, std::string *error) {
  std::string container = warren->default_container();
  // Assuming it's okay not to match against the identifier query.
  std::string query = "(>> " + container + " \"" + identifier + "\")";
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(query, error);
  if (hopper == nullptr)
    return false;
  cottontail::addr p, q;
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  if (p == cottontail::maxfinity) {
    cottontail::safe_set(error) =
        "can't find item with identifier: " + identifier;
    return false;
  }
  *text = warren->txt()->translate(p + 2, q);
  for (size_t i = 0; i < text->size(); i++)
    if ((*text)[i] == '\n' || (*text)[i] == '\t')
      (*text)[i] = ' ';
  return true;
}

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] questions preferences\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc != 3) {
    usage(program_name);
    return 1;
  }
  std::string questions_filename = argv[1];
  std::string preferences_filename = argv[2];
  std::ifstream quef(questions_filename);
  if (quef.fail()) {
    std::cerr << program_name
              << ": can't open questions: " + questions_filename;
    return 1;
  }
  std::map<std::string, std::string> questions;
  std::string line;
  while (std::getline(quef, line)) {
    if (line.find(" ") == std::string::npos) {
      std::cerr << program_name << ": bad question: " << line << "\n";
      return 1;
    }
    std::string topic = line.substr(0, line.find(" "));
    std::string question = line.substr(line.find(" ") + 1);
    for (size_t i = 0; i < question.size(); i++)
      if (question[i] == '\r')
        question[i] = ' ';
    questions[topic] = question;
  }
  quef.close();
  std::ifstream pref(preferences_filename);
  if (pref.fail()) {
    std::cerr << program_name
              << ": can't open preferences: " + preferences_filename;
    return 1;
  }
  std::string error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  if (burrow == "")
    warren = cottontail::Warren::make(simple, &error);
  else
    warren = cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  size_t bad_count = 0;
  warren->start();
  while (std::getline(pref, line)) {
    if (bad_count > 40) {
      std::cerr << program_name << ": too many bad preferences\n";
      return 1;
    }
    std::string s = line;
    if (s.find(" ") == std::string::npos) {
      std::cerr << program_name << ": bad preference: " << line << "\n";
      bad_count++;
      continue;
    }
    std::string topic = s.substr(0, s.find(" "));
    if (questions.find(topic) == questions.end()) {
      std::cerr << program_name << ": unknown topic in preference: " << line
                << "\n";
      bad_count++;
      continue;
    }
    s = s.substr(s.find(" ") + 1);
    if (s.find(" ") == std::string::npos) {
      std::cerr << program_name << ": bad preference: " << line << "\n";
      bad_count++;
      continue;
    }
    std::string first = s.substr(0, s.find(" "));
    s = s.substr(s.find(" ") + 1);
    if (s.find(" ") == std::string::npos) {
      std::cerr << program_name << ": bad preference: " << line << "\n";
      bad_count++;
      continue;
    }
    std::string second = s.substr(0, s.find(" "));
    std::string best = s.substr(s.find(" ") + 1);
    std::string worst;
    if (first == best) {
      worst = second;
    } else if (second == best) {
      worst = first;
    } else {
      std::cerr << program_name << "bad preference: " << line << "\n";
      bad_count++;
      continue;
    }
    std::string best_text;
    if (!fetch_text(warren, best, &best_text, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      bad_count++;
      continue;
    }
    std::string worst_text;
    if (!fetch_text(warren, worst, &worst_text, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      bad_count++;
      continue;
    }
    std::cout << questions[topic] << "\t" << best_text << "\t" << worst_text
              << "\n";
  }
  warren->end();
  return 0;
}
