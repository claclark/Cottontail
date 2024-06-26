#include "src/annotator.h"

#include <fstream>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/null_annotator.h"
#include "src/simple_annotator.h"
#include "src/working.h"

namespace cottontail {

std::shared_ptr<Annotator> Annotator::make(const std::string &name,
                                           const std::string &recipe,
                                           std::string *error,
                                           std::shared_ptr<Working> working) {
  std::shared_ptr<Annotator> annotator = nullptr;
  if (name == "" || name == "null") {
    annotator = NullAnnotator::make(recipe, error);
    if (annotator != nullptr)
      annotator->name_ = "null";
  } else if (name == "simple") {
    annotator = SimpleAnnotator::make(recipe, working, error);
    if (annotator != nullptr)
      annotator->name_ = "simple";
  } else {
    safe_set(error) = "No Annotator named: " + name;
  }
  return annotator;
}

bool Annotator::check(const std::string &name, const std::string &recipe,
                      std::string *error) {
  if (name == "" || name == "null") {
    return NullAnnotator::check(recipe, error);
  } else if (name == "simple") {
    return SimpleAnnotator::check(recipe, error);
  } else {
    safe_set(error) = "No Annotator named: " + name;
    return false;
  }
}

bool Annotator::recover(const std::string &name, const std::string &recipe,
                        bool commit, std::string *error,
                        std::shared_ptr<Working> working) {
  if (name == "" || name == "null") {
    return true;
  } else if (name == "simple") {
    return SimpleAnnotator::recover(recipe, commit, working, error);
  } else {
    safe_set(error) = "No Annotator named: " + name;
    return false;
  }
}

bool Annotator::annotate_(const std::string &annotations_filename,
                          std::string *error) {
  std::fstream annf(annotations_filename, std::ios::binary | std::ios::in);
  if (annf.fail()) {
    safe_set(error) =
        "Annotator can't access annotations in: " + annotations_filename;
    return false;
  }
  Annotation current;
  for (annf.read(reinterpret_cast<char *>(&current), sizeof(Annotation));
       !annf.fail();
       annf.read(reinterpret_cast<char *>(&current), sizeof(Annotation)))
    if (current.feature != null_feature &&
        !annotate(current.feature, current.p, current.q, current.v))
      return false;
  annf.close();
  return true;
}

bool Annotator::erase_(addr p, addr q, std::string *error) {
  safe_set(error) = "Annotator " + name() + "can't erase content";
  return false;
}

} // namespace cottontail
