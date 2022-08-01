#ifndef COTTONTAIL_SRC_SIMPLE_TXT_IO_H_
#define COTTONTAIL_SRC_SIMPLE_TXT_IO_H_

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/committable.h"
#include "src/compressor.h"
#include "src/core.h"

namespace cottontail {

class SimpleTxtIO final : public Committable {
public:
  static std::shared_ptr<SimpleTxtIO>
  make(const std::string &nameof_contents, const std::string &nameof_chunk_map,
       addr chunk_size, std::shared_ptr<Compressor> compressor,
       std::string *error = nullptr);
  static void recover(const std::string &nameof_contents,
                      const std::string &nameof_chunk_map, addr chunk_size,
                      std::shared_ptr<Compressor> compressor, bool commit);
  void append(char *text, addr length);
  void flush() {
    if (read_only_)
      return;
    if (!flushed_) {
      sync_last_chunk();
      contents_.flush();
      flushed_ = true;
    }
  }
  std::unique_ptr<char[]> read(addr start, addr desired, addr *actual);
  addr size() { return size_; }
  void dump(std::ostream &output = std::cout);

  virtual ~SimpleTxtIO() { flush(); }
  SimpleTxtIO(const SimpleTxtIO &) = delete;
  SimpleTxtIO &operator=(const SimpleTxtIO &) = delete;
  SimpleTxtIO(SimpleTxtIO &&) = delete;
  SimpleTxtIO &operator=(SimpleTxtIO &&) = delete;

private:
  SimpleTxtIO(const std::string &nameof_contents,
              const std::string &nameof_chunk_map, std::streamsize chunk_size,
              std::shared_ptr<Compressor> compressor)
      : nameof_contents_(nameof_contents), nameof_chunk_map_(nameof_chunk_map),
        chunk_size_(chunk_size), compressor_(compressor){};
  bool transaction_(std::string *error = nullptr) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  void fetch_chunk(size_t chunk_index_);
  void fetch_last_chunk();
  void append_last_chunk(char *text, addr length);
  void sync_last_chunk();
  void append_full_chunk(char *text);
  void start_new_chunk(char *text, addr length);
  addr size_;
  std::fstream contents_;
  std::string nameof_contents_;
  std::string nameof_chunk_map_;
  std::streamsize chunk_size_;
  std::shared_ptr<Compressor> compressor_;
  std::vector<std::streamoff> chunk_map_;
  bool chunk_valid_ = false;
  bool last_chunk_synced_ = true;
  bool flushed_ = true;
  size_t chunk_index_;
  std::unique_ptr<char[]> chunk_;
  std::streamsize chunk_end_;
  addr buffer_size_;
  std::unique_ptr<char[]> buffer_;
  bool read_only_ = false;
  bool limited_;
  addr chunk_map_limit_;
  addr last_chunk_end_;
  std::unique_ptr<char[]> last_chunk_;
};
} // namespace cottontail

#endif // COTTONTAIL_SRC_SIMPLE_TXT_IO_H_
