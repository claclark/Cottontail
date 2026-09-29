#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "src/cottontail.h"

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  std::string simple = "simple";
  std::string burrow = "/data/hdd3/claclark/df.burrow";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  warren->start();
  std::string doc_query = "(... <DOC> </DOC>)";
  std::unique_ptr<cottontail::Hopper> doc_hopper =
      warren->hopper_from_gcl(doc_query);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, '\t'))
      tokens.push_back(token);
    std::string docno = tokens[2];
    std::string docno_query = "(>> (... <DOCNO> </DOCNO>) \"" + docno + "\")";
    std::unique_ptr<cottontail::Hopper> docno_hopper =
        warren->hopper_from_gcl(docno_query);
    cottontail::addr p_docno, q_docno;
    docno_hopper->tau(0, &p_docno, &q_docno);
    cottontail::addr p_doc, q_doc;
    doc_hopper->ohr(p_docno, &p_doc, &q_doc);
    auto clean = [](std::string s) {
      bool inside = false;
      for (size_t i = 0; i < s.length(); i++)
        if (inside) {
          if (s[i] == '>') {
            s[i] = ' ';
            inside = false;
          } else if (s[i] == '\n') {
            inside = false;
          } else {
            s[i] = ' ';
          }
        } else if (s[i] == '<') {
          s[i] = '-';
          inside = true;
        }
      for (size_t i = 0; i < s.length(); i++)
        if (s[i] == '\n' || s[i] == '\t' || s[i] == '\r')
          s[i] = ' ';
      std::string t = "";
      bool spacing = false;
      for (size_t i = 0; i < s.length(); i++)
        if (spacing) {
          if (s[i] != ' ') {
            t += s[i];
            spacing = false;
          }
        } else {
          spacing = (s[i] == ' ');
          t += s[i];
        }
      return t;
    };
    if (p_doc >= 0) {
      cottontail::addr p = q_docno + 1, q = q_doc;
      if (q - p + 1 > 2048)
        q = p + 2048;
      std::string text = clean(warren->txt()->translate(p, q));
      for (auto &token : tokens)
        std::cout << token << '\t';
      std::cout << text << "\n";
    }
  }
  warren->end();
  return 0;
}
