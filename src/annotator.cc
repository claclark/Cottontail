#include "src/annotator.h"

#include <fstream>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/null_annotator.h"
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
  } else {
    safe_set(error) = "No Annotator named: " + name;
  }
  return annotator;
}

bool Annotator::check(const std::string &name, const std::string &recipe,
                      std::string *error) {
  if (name == "" || name == "null") {
    return NullAnnotator::check(recipe, error);
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
    if (!annotate(current.feature, current.p, current.v, current.v))
      return false;
  annf.close();
  return true;
}

} // namespace cottontail
