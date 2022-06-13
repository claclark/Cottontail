#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

#include "src/cottontail.h"
#include "src/mt.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow]\n";
}

const char *myreadline(const char *prompt) {
  if (isatty(STDIN_FILENO)) {
    const char *line = readline(prompt);
    if (strlen(line) > 0)
      add_history(line);
    return line;
  } else {
    static std::string line;
    if (std::getline(std::cin, line))
      return line.c_str();
    else
      return nullptr;
  }
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = "";
  if (argc == 3 && argv[1] == std::string("--burrow")) {
    burrow = argv[2];
  } else if (argc > 1) {
    usage(program_name);
    return 0;
  }
  std::string error;
  std::shared_ptr<cottontail::Warren> warren;
  if (burrow == "")
    warren = cottontail::Warren::make("simple", &error);
  else
    warren = cottontail::Warren::make("simple", burrow, &error);
  warren->start();
  std::string docnos;
  std::string id_key = "id";
  if (!warren->get_parameter(id_key, &docnos))
    docnos = "(... <DOCNO> </DOCNO>)";
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(docnos, &error);
  if (hopper == nullptr) {
    std::cout << program_name << ": " << error << " can't find identifiers\n";
    return 1;
  }

  cottontail::Mt mt;
  const char *line;
  while ((line = myreadline(">> ")) != nullptr) {
    if (line != nullptr && line[0] != '\0') {
      if (line[0] == '@') {
        std::string sline = line;
        std::regex ws("\\s+");
        std::vector<std::string> cmd{
            std::sregex_token_iterator(sline.begin(), sline.end(), ws, -1), {}};
        if (cmd.size() > 2 && cmd[0] == "@rank") {
          bool okay = true;
          std::string topic = cmd[1];
          std::vector<std::string> tiers;
          for (size_t i = 2; okay && i < cmd.size(); i++)
            if (mt.infix_expression(cmd[i], &error)) {
              tiers.emplace_back(mt.s_expression());
            } else {
              okay = false;
              std::cerr << program_name << ": " << error << ":" << cmd[i]
                        << "\n";
            }
          if (okay) {
            std::vector<cottontail::RankingResult> ranking =
                cottontail::tiered_ranking(warren, tiers,
                                           warren->default_container(), 1000);
            if (ranking.size() == 0) {
              std::cerr << program_name << ": no results for topic \"" << topic
                        << "\" (creating a fake one)\n";
              std::cout << topic << " Q0 FAKE 1 1 cottontail\n";
            } else {
              for (size_t i = 0; i < ranking.size(); i++) {
                cottontail::addr p, q;
                hopper->tau(ranking[i].container_p(), &p, &q);
                if (q <= ranking[i].container_q()) {
                  std::string docno =
                      cottontail::trec_docno(warren->txt()->translate(p, q));
                  std::cout << topic << " Q0 " << docno << " " << i + 1 << " "
                            << ranking[i].score() << " cottontail\n";
                }
              }
            }
            std::flush(std::cout);
          }
        }
      } else if (!mt.infix_expression(line, &error)) {
        std::cerr << "Error: " << error << ":" << line << "\n";
      }
    }
  }
  warren->end();
  return 0;
}
