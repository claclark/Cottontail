#ifndef COTTONTAIL_MEADOWLARK_REMOTE_WARREN_H_
#define COTTONTAIL_MEADOWLARK_REMOTE_WARREN_H_

#include "src/warren.h"

namespace cottontail::meadowlark {

namespace remote_protocol {
class Connection;
}

class RemoteWarren final : public Warren {
public:
  static std::shared_ptr<RemoteWarren> make(int port,
                                            std::string *error = nullptr);

private:
  RemoteWarren(int port,
               std::shared_ptr<remote_protocol::Connection> connection,
               std::shared_ptr<Featurizer> featurizer,
               std::shared_ptr<Tokenizer> tokenizer);

  std::shared_ptr<Warren> clone_(std::string *error) final;
  std::unique_ptr<Hopper> hopper_from_gcl_(const std::string &query,
                                           std::string *error) final;
  void start_() final;
  void end_() final;
  bool transaction_(std::string *error) final;
  bool ready_(std::string *error) final;
  void commit_() final;
  void abort_() final;
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final;
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final;

  int port_;
  std::shared_ptr<remote_protocol::Connection> connection_;
};

} // namespace cottontail::meadowlark

#endif // COTTONTAIL_MEADOWLARK_REMOTE_WARREN_H_
