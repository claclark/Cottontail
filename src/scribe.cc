#include "src/scribe.h"

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/builder.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/json.h"
#include "src/null_annotator.h"
#include "src/null_appender.h"
#include "src/null_featurizer.h"
#include "src/warren.h"

namespace cottontail {

namespace {

class NullScribe : public Scribe {
public:
  NullScribe() {
    std::string recipe = "";
    null_featurizer_ = NullFeaturizer::make(recipe);
    null_annotator_ = NullAnnotator::make(recipe);
    null_appender_ = NullAppender::make(recipe);
  };

  virtual ~NullScribe(){};
  NullScribe(const NullScribe &) = delete;
  NullScribe &operator=(const NullScribe &) = delete;
  NullScribe(NullScribe &&) = delete;
  NullScribe &operator=(NullScribe &&) = delete;

private:
  std::shared_ptr<Featurizer> featurizer_() final { return null_featurizer_; }
  std::shared_ptr<Annotator> annotator_() final { return null_annotator_; }
  std::shared_ptr<Appender> appender_() final { return null_appender_; };
  bool set_(const std::string &key, const std::string &value,
            std::string *error) final {
    return true;
  };
  bool finalize_(std::string *error) final { return true; };
  bool transaction_(std::string *error) final { return true; };
  bool ready_() final { return true; };
  void commit_() final { return; };
  void abort_() final { return; };

  std::shared_ptr<Featurizer> null_featurizer_;
  std::shared_ptr<Annotator> null_annotator_;
  std::shared_ptr<Appender> null_appender_;
};

class WarrenScribe : public Scribe {
public:
  WarrenScribe(std::shared_ptr<Warren> warren) : warren_(warren){};
  WarrenScribe() = delete;
  virtual ~WarrenScribe(){};
  WarrenScribe(const WarrenScribe &) = delete;
  WarrenScribe &operator=(const WarrenScribe &) = delete;
  WarrenScribe(WarrenScribe &&) = delete;
  WarrenScribe &operator=(WarrenScribe &&) = delete;

private:
  std::shared_ptr<Featurizer> featurizer_() final {
    return warren_->featurizer();
  }
  std::shared_ptr<Annotator> annotator_() final { return warren_->annotator(); }
  std::shared_ptr<Appender> appender_() final { return warren_->appender(); };
  bool set_(const std::string &key, const std::string &value,
            std::string *error) final {
    return warren_->set_parameter(key, value, error);
  };
  bool finalize_(std::string *error) final { return true; }
  bool transaction_(std::string *error) final {
    return warren_->transaction(error);
  };
  bool ready_() final { return warren_->ready(); };
  void commit_() final { warren_->commit(); };
  void abort_() final { warren_->abort(); };

  std::shared_ptr<Warren> warren_;
};

class BuilderAnnotator : public Annotator {
public:
  static std::shared_ptr<Annotator> make(std::shared_ptr<Builder> builder,
                                         std::string *error = nullptr) {
    if (builder == nullptr) {
      safe_set(error) = "BuilderAnnotator got null builder";
      return nullptr;
    }
    std::shared_ptr<BuilderAnnotator> annotator =
        std::shared_ptr<BuilderAnnotator>(new BuilderAnnotator());
    annotator->builder_ = builder;
    return annotator;
  };

  virtual ~BuilderAnnotator(){};
  BuilderAnnotator(const BuilderAnnotator &) = delete;
  BuilderAnnotator &operator=(const BuilderAnnotator &) = delete;
  BuilderAnnotator(BuilderAnnotator &&) = delete;
  BuilderAnnotator &operator=(BuilderAnnotator &&) = delete;

private:
  BuilderAnnotator(){};
  std::string recipe_() final { return ""; };
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error = nullptr) final {
    if (builder_ == nullptr) {
      safe_set(error) = "BuilderAnnotator has null builder";
      return false;
    }
    builder_->add_annotation(feature, p, q, v, error);
    return true;
  };
  bool transaction_(std::string *error) final { return true; };
  bool ready_() final { return true; };
  void commit_() final { builder_ = nullptr; };
  void abort_() final { builder_ = nullptr; };

  std::shared_ptr<Builder> builder_;
};

class BuilderAppender : public Appender {
public:
  static std::shared_ptr<Appender> make(std::shared_ptr<Builder> builder,
                                        std::string *error = nullptr) {
    if (builder == nullptr) {
      safe_set(error) = "BuilderAppender got null builder";
      return nullptr;
    }
    std::shared_ptr<BuilderAppender> appender =
        std::shared_ptr<BuilderAppender>(new BuilderAppender());
    appender->builder_ = builder;
    return appender;
  };

  virtual ~BuilderAppender(){};
  BuilderAppender(const BuilderAppender &) = delete;
  BuilderAppender &operator=(const BuilderAppender &) = delete;
  BuilderAppender(BuilderAppender &&) = delete;
  BuilderAppender &operator=(BuilderAppender &&) = delete;

private:
  BuilderAppender(){};
  std::string recipe_() final { return ""; };
  bool append_(const std::string &text, addr *p, addr *q,
               std::string *error) final {
    if (builder_ == nullptr) {
      safe_set(error) = "BuilderAppender has null builder";
      return false;
    }
    return builder_->add_text(text, p, q, error);
  };
  bool transaction_(std::string *error) final { return true; };
  bool ready_() final { return true; };
  void commit_() final { builder_ = nullptr; };
  void abort_() final { builder_ = nullptr; };

  std::shared_ptr<Builder> builder_;
};

class BuilderScribe : public Scribe {
public:
  BuilderScribe(std::shared_ptr<Builder> builder) : builder_(builder) {
    builder_annotator_ = BuilderAnnotator::make(builder);
    builder_appender_ = BuilderAppender::make(builder);
  };

  BuilderScribe() = delete;
  virtual ~BuilderScribe(){};
  BuilderScribe(const BuilderScribe &) = delete;
  BuilderScribe &operator=(const BuilderScribe &) = delete;
  BuilderScribe(BuilderScribe &&) = delete;
  BuilderScribe &operator=(BuilderScribe &&) = delete;

private:
  std::shared_ptr<Featurizer> featurizer_() final {
    return builder_->featurizer();
  }
  std::shared_ptr<Annotator> annotator_() final { return builder_annotator_; }
  std::shared_ptr<Appender> appender_() final { return builder_appender_; };
  bool set_(const std::string &key, const std::string &value,
            std::string *error) final {
    safe_set(error) = "BuilderScribe does not support parameters";
    return false;
  };
  bool finalize_(std::string *error) { return builder_->finalize(error); }
  bool transaction_(std::string *error) final { return true; };
  bool ready_() final { return true; };
  void commit_() final { return; };
  void abort_() final { return; };

  std::shared_ptr<Builder> builder_;
  std::shared_ptr<Annotator> builder_annotator_;
  std::shared_ptr<Appender> builder_appender_;
};

} // namespace

std::shared_ptr<Scribe> Scribe::null(std::string *error) {
  return std::shared_ptr<Scribe>(new NullScribe());
}

std::shared_ptr<Scribe> Scribe::make(std::shared_ptr<Warren> warren,
                                     std::string *error) {
  if (warren == nullptr) {
    safe_set(error) = "Scribe got null warren";
    return nullptr;
  }
  return std::shared_ptr<Scribe>(new WarrenScribe(warren));
}

std::shared_ptr<Scribe> Scribe::make(std::shared_ptr<Builder> builder,
                                     std::string *error) {
  if (builder == nullptr) {
    safe_set(error) = "Scribe got null builder";
    return nullptr;
  }
  return std::shared_ptr<Scribe>(new BuilderScribe(builder));
}

bool scribe_files(const std::vector<std::string> &filenames,
                  std::shared_ptr<Scribe> scribe, std::string *error,
                  bool verbose) {
  if (scribe == nullptr) {
    safe_set(error) = "Function scribe_files passed null scribe";
    return false;
  }
  addr file_feature = scribe->featurizer()->featurize("file:");
  addr filename_feature = scribe->featurizer()->featurize("filename:");
  addr content_feature = scribe->featurizer()->featurize("content:");
  for (auto &filename : filenames) {
    if (!scribe->transaction(error))
      return false;
    if (verbose)
      std::cerr << "scribe_files inhaling: " << filename << "\n";
    std::shared_ptr<std::string> content = inhale(filename, error);
    if (content) {
      addr name_p, name_q, content_p, content_q;
      if (!scribe->appender()->append(filename, &name_p, &name_q, error))
        return false;
      if (!scribe->appender()->append(*content, &content_p, &content_q, error))
        return false;
      if (!scribe->annotator()->annotate(file_feature, name_p, content_q,
                                         error))
        return false;
      if (!scribe->annotator()->annotate(filename_feature, name_p, name_q,
                                         error))
        return false;
      if (!scribe->annotator()->annotate(content_feature, content_p, content_q,
                                         error))
        return false;
      if (verbose)
        std::cerr << "scribe_files commiting: " << filename << "\n";
      if (!scribe->ready()) {
        safe_set(error) = "Function scribe_files unable to commit";
        return false;
      }
      scribe->commit();
    }
  }
  if (verbose)
    std::cerr << "scribe_files done\n";
  return true;
}

namespace {
bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error);

bool do_null(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error) {
  if (!scribe->appender()->append("null", p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, NAN, error);
}

bool do_boolean(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
                addr *p, addr *q, std::string *error) {
  if (j) {
    if (!scribe->appender()->append("true", p, q, error))
      return false;
    if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 1.0, error))
      return false;
  } else {
    if (!scribe->appender()->append("false", p, q, error))
      return false;
    if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 0.0, error))
      return false;
  }
  return true;
}

bool do_number(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  fval v = j;
  if (!scribe->appender()->append(std::to_string(v), p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, v, error);
}

bool do_string(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  std::string s = j;
  std::string t = "\"";
  for (const char *p = s.c_str(); *p; p++)
    if (*p == '"')
      t += "\\\"";
    else
      t += *p;
  t += "\"";
  if (!scribe->appender()->append(t, p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 0.0, error);
}

bool do_array(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
              addr *p, addr *q, std::string *error) {
  addr p0, q0;
  addr p_min = maxfinity, q_max = minfinity;
  if (!scribe->appender()->append("[", &p0, &q0, error))
    return false;
  p_min = std::min(p_min, p0);
  q_max = std::max(q_max, q0);
  size_t index = 0;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (!do_json(it.value(), scribe, tag + std::to_string(index) + ":", &p0,
                 &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
    index++;
  }
  if (!scribe->appender()->append("]", &p0, &q0, error))
    return false;
  //p_min = std::min(p_min, p0);
  //q_max = std::max(q_max, q0);
  if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag),
                                     p_min, q_max, (fval) j.size(), error))
    return false;
  *p = p_min;
  *q = q_max;
  return true;
}

bool do_object(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  addr p0, q0;
  addr p_min = maxfinity, q_max = minfinity;
  if (!scribe->appender()->append("{", &p0, &q0, error))
    return false;
  p_min = std::min(p_min, p0);
  q_max = std::max(q_max, q0);
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    std::string key;
    if (it != j.begin())
      key = ",\"" + it.key() + "\":";
    else
      key = "\"" + it.key() + "\":";
    if (!scribe->appender()->append(key, &p0, &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
    if (!do_json(it.value(), scribe, tag + it.key() + ":", &p0, &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
  }
  if (!scribe->appender()->append("}", &p0, &q0, error))
    return false;
  //p_min = std::min(p_min, p0);
  //q_max = std::max(q_max, q0);
  if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag),
                                     p_min, q_max, 0.0, error))
    return false;
  *p = p_min;
  *q = q_max;
  return true;
}

bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error) {
  if (j.is_null())
    return do_null(j, scribe, tag, p, q, error);
  else if (j.is_boolean())
    return do_boolean(j, scribe, tag, p, q, error);
  else if (j.is_number())
    return do_number(j, scribe, tag, p, q, error);
  else if (j.is_string())
    return do_string(j, scribe, tag, p, q, error);
  else if (j.is_array())
    return do_array(j, scribe, tag, p, q, error);
  else if (j.is_object())
    return do_object(j, scribe, tag, p, q, error);
  safe_set(error) = "Unknown JSON data type.";
  return false;
}
} // namespace

bool scribe_json(json &j, std::shared_ptr<Scribe> scribe, std::string *error) {
  if (scribe == nullptr) {
    safe_set(error) = "Function scribe_json passed null scribe";
    return false;
  }
  if (!j.is_object()) {
    safe_set(error) = "Function scribe_json can only scribe JSON objects";
    return false;
  }
  addr p, q;
  return do_json(j, scribe, ":", &p, &q, error);
  return true;
}

} // namespace cottontail
