#include "src/dna.h"

#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

#include "src/core.h"
#include "src/recipe.h"

namespace cottontail {
namespace {
const std::string HEADER =
    "# DO NOT EDIT DNA - DO NOT EDIT DNA - DO NOT EDIT DNA ";
}

bool read_dna(std::shared_ptr<Working> working, std::string *dna,
              std::string *error) {
  std::string dna_filename = working->make_name(DNA_NAME);
  std::fstream in(dna_filename, std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Can't read configuration from: " + dna_filename;
    return false;
  }
  std::string line;
  std::getline(in, line);
  if (line != HEADER) {
    safe_error(error) = "Invalid configuration: " + dna_filename;
    return false;
  }
  *dna = "";
  while (std::getline(in, line))
    *dna += (line + " ");
  return true;
}

bool write_dna(std::shared_ptr<Working> working, const std::string &dna,
               std::string *error) {
  std::string dna_filename = working->make_name(DNA_NAME);
  std::string temp_filename = working->make_temp("dna");
  std::fstream out(temp_filename, std::ios::out);
  if (out.fail()) {
    safe_error(error) = "Can't write configuration to: " + temp_filename;
    return false;
  }
  out << HEADER << "\n";
  out << dna;
  out.close();
  std::rename(temp_filename.c_str(), dna_filename.c_str());
  return true;
}

bool get_parameter_from_dna(std::shared_ptr<Working> working,
                            const std::string &key, std::string *value,
                            std::string *error) {
  std::string dna;
  if (!read_dna(working, &dna, error))
    return false;
  std::map<std::string, std::string> dna_parameters;
  if (!cook(dna, &dna_parameters, error))
    return false;
  std::map<std::string, std::string> parameters;
  std::map<std::string, std::string>::iterator it =
      dna_parameters.find("parameters");
  if (it == dna_parameters.end()) {
    *value = "";
    return true;
  }
  if (!cook(it->second, &parameters, error))
    return false;
  it = parameters.find(key);
  if (it == parameters.end())
    *value = "";
  else
    *value = it->second;
  return true;
}

bool set_parameter_in_dna(std::shared_ptr<Working> working,
                          const std::string &key, const std::string &value,
                          std::string *error) {
  std::string dna;
  if (!read_dna(working, &dna, error))
    return false;
  std::map<std::string, std::string> dna_parameters;
  if (!cook(dna, &dna_parameters, error))
    return false;
  std::map<std::string, std::string> parameters;
  std::map<std::string, std::string>::iterator it =
      dna_parameters.find("parameters");
  if (it != dna_parameters.end() && !cook(it->second, &parameters, error))
    return false;
  parameters[key] = value;
  dna_parameters["parameters"] = freeze(parameters);
  dna = freeze(dna_parameters);
  if (!write_dna(working, dna, error))
    return false;
  return true;
}

bool name_and_recipe(const std::string &dna, const std::string &key,
                     std::string *error, std::string *name,
                     std::string *recipe) {
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters)) {
    safe_error(error) = "Bad parameters";
    return false;
  }
  std::map<std::string, std::string>::iterator v = parameters.find(key);
  if (v == parameters.end()) {
    safe_error(error) = "No " + key + " found";
    return false;
  }
  std::map<std::string, std::string> k_parameters;
  if (!cook(v->second, &k_parameters)) {
    safe_error(error) = "Bad " + key + " parameters";
    return false;
  }
  std::map<std::string, std::string>::iterator k_name =
      k_parameters.find("name");
  if (k_name == k_parameters.end()) {
    safe_error(error) = "No " + key + " name found";
    return false;
  }
  safe_set(name) = k_name->second;
  std::map<std::string, std::string>::iterator k_recipe =
      k_parameters.find("recipe");
  if (k_recipe == k_parameters.end()) {
    safe_error(error) = "No " + key + " recipe found";
    return false;
  }
  safe_set(recipe) = k_recipe->second;
  return true;
}

} // namespace cottontail
