#include "src/scribe.h"

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/builder.h"
#include "src/committable.h"
#include "src/core.h"
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
  bool transaction_(std::string *error = nullptr) final { return true; };
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
  bool transaction_(std::string *error = nullptr) final {
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
  bool transaction_(std::string *error) final {
    if (builder_ == nullptr) {
      safe_set(error) =
          "BuilderAnnotator does not support more than one transaction";
      return false;
    }
    return true;
  };
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
    return builder_->add_text(text, p, q, error);
  };
  bool transaction_(std::string *error) final {
    if (builder_ == nullptr) {
      safe_set(error) =
          "BuilderAppender does not support more than one transaction";
      return false;
    }
    return true;
  };
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
  bool transaction_(std::string *error = nullptr) final {
    if (builder_ == nullptr) {
      safe_set(error) =
          "BuilderScribe does not support more than one transaction";
      return false;
    }
    return true;
  };
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
                  std::shared_ptr<Scribe> scribe, bool verbose,
                  std::string *error) {
  if (scribe == nullptr) {
    safe_set(error) = "Function scribe_files passed null scribe";
    return false;
  }
  if (!scribe->transaction())
    return false;
  addr file_feature = scribe->featurizer()->featurize("file:");
  addr filename_feature = scribe->featurizer()->featurize("filename:");
  addr content_feature = scribe->featurizer()->featurize("content:");
  for (auto &filename : filenames) {
    if (verbose)
      std::cout << "Scribe inhaling: " << filename << "\n";
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
    }
  }
  if (verbose)
    std::cout << "Scribe commiting\n";
  if (!scribe->ready()) {
    safe_set(error) = "Function scribe_files unable to commit";
    return false;
  }
  scribe->commit();
  if (verbose)
    std::cout << "Scribe done\n";
  return true;
}

} // namespace cottontail
