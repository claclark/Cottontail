#include "src/simple_txt_io.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace cottontail {

namespace {
std::unique_ptr<char[]> read_limits(const std::string &nameof_contents,
                                    addr *chunk_map_limit,
                                    addr *last_chunk_end) {
  std::string nameof_limits = nameof_contents + ".limits";
  std::fstream limits;
  limits.open(nameof_limits, std::ios::binary | std::ios::in);
  if (limits.fail())
    return nullptr;
  limits.read(reinterpret_cast<char *>(chunk_map_limit), sizeof(addr));
  assert(!limits.fail());
  limits.read(reinterpret_cast<char *>(last_chunk_end), sizeof(addr));
  assert(!limits.fail());
  std::unique_ptr<char[]> last_chunk =
      std::unique_ptr<char[]>(new char[*last_chunk_end + 1]);
  limits.read(last_chunk.get(), *last_chunk_end);
  assert(!limits.fail());
  limits.close();
  return last_chunk;
}

void unlock(const std::string &nameof_contents) {
  std::string nameof_temp = nameof_contents + ".temp";
  std::remove(nameof_temp.c_str());
  std::string nameof_limits = nameof_contents + ".limits";
  std::remove(nameof_limits.c_str());
  std::string nameof_lock = nameof_contents + ".lock";
  std::remove(nameof_lock.c_str());
}

void restore(const std::string &nameof_contents,
             const std::string &nameof_chunk_map, addr chunk_size,
             std::shared_ptr<Compressor> compressor) {
  addr chunk_map_limit;
  addr last_chunk_end;
  std::unique_ptr<char[]> last_chunk =
      read_limits(nameof_contents, &chunk_map_limit, &last_chunk_end);
  if (last_chunk == nullptr)
    return;
  if (chunk_map_limit == 0) {
    assert(truncate(nameof_contents.c_str(), 0) == 0);
    assert(truncate(nameof_chunk_map.c_str(), 0) == 0);
    return;
  }
  assert(truncate(nameof_chunk_map.c_str(),
                  chunk_map_limit * sizeof(std::streamoff)) == 0);
  std::fstream mapf(nameof_chunk_map,
                    std::ios::binary | std::ios::in | std::ios::out);
  assert(!mapf.fail());
  std::streamoff where;
  if (chunk_map_limit == 1) {
    where = 0;
  } else {
    mapf.seekp(-2 * sizeof(where), mapf.end);
    assert(!mapf.fail());
    mapf.read(reinterpret_cast<char *>(&where), sizeof(where));
  }
  addr buffer_size = chunk_size + compressor->extra(chunk_size) + 1;
  std::unique_ptr<char[]> buffer =
      std::unique_ptr<char[]>(new char[buffer_size]);
  std::streamsize amount = compressor->crush(last_chunk.get(), last_chunk_end,
                                             buffer.get(), buffer_size);
  assert(truncate(nameof_contents.c_str(), where + amount) == 0);
  std::fstream contents(nameof_contents,
                        std::ios::binary | std::ios::in | std::ios::out);
  assert(!contents.fail());
  contents.seekp(-amount, contents.end);
  assert(!contents.fail());
  contents.write(buffer.get(), amount);
  assert(!contents.fail());
  where = contents.tellp();
  assert(!contents.fail() && where >= 0);
  contents.close();
  mapf.seekp(-sizeof(where), mapf.end);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
  mapf.close();
}

} // namespace

void SimpleTxtIO::recover(const std::string &nameof_contents,
                          const std::string &nameof_chunk_map, addr chunk_size,
                          std::shared_ptr<Compressor> compressor, bool commit) {
  if (!commit)
    restore(nameof_contents, nameof_chunk_map, chunk_size, compressor);
  unlock(nameof_contents);
}

std::shared_ptr<SimpleTxtIO>
SimpleTxtIO::make(const std::string &nameof_contents,
                  const std::string &nameof_chunk_map, addr chunk_size,
                  std::shared_ptr<Compressor> compressor, std::string *error) {
  std::shared_ptr<SimpleTxtIO> thing =
      std::shared_ptr<SimpleTxtIO>(new SimpleTxtIO(
          nameof_contents, nameof_chunk_map, chunk_size, compressor));
  assert(!compressor->destructive());
  thing->chunk_ = std::unique_ptr<char[]>(new char[thing->chunk_size_ + 1]);
  thing->buffer_size_ = chunk_size + compressor->extra(thing->chunk_size_) + 1;
  thing->buffer_ = std::unique_ptr<char[]>(new char[thing->buffer_size_]);
  // Create content file if it doesn't exist, but don't it alter if it does
  // This may fail if the burrow isn't writable, but we don't care.
  thing->contents_.open(thing->nameof_contents_,
                        std::ios::binary | std::ios::app);
  thing->contents_.close();
  // Try opening contents for I/O.
  // It may not exist if the burrow isn't writable
  thing->contents_.open(nameof_contents,
                        std::ios::binary | std::ios::out | std::ios::in);
  // Try opening contents for input only.
  if (thing->contents_.fail()) {
    thing->contents_.open(nameof_contents, std::ios::binary | std::ios::in);
    thing->read_only_ = true;
  }
  if (thing->contents_.fail()) {
    // We give up
    safe_set(error) = "SimpleTxtIO can't access: " + nameof_contents;
    return nullptr;
  }
  std::fstream mapf;
  // Create the chunk map file if it doesn't exist, but don't alter it if does
  // This may fail if the burrow isn't writable, but we don't care.
  mapf.open(thing->nameof_chunk_map_, std::ios::binary | std::ios::app);
  mapf.close();
  // Try opening the chunk map for I/O.
  // It may not exist if the burrow isn't writable.
  mapf.open(thing->nameof_chunk_map_,
            std::ios::binary | std::ios::in | std::ios::out);
  // Try opening the chunk map for input only.
  if (mapf.fail()) {
    mapf.open(thing->nameof_chunk_map_, std::ios::binary | std::ios::in);
    thing->read_only_ = true;
  }
  if (mapf.fail()) {
    // We give up
    safe_set(error) = "SimpleTxtIO can't access: " + thing->nameof_chunk_map_;
    return nullptr;
  }
  thing->last_chunk_ = read_limits(nameof_contents, &(thing->chunk_map_limit_),
                                   &(thing->last_chunk_end_));
  thing->limited_ = (thing->last_chunk_ != nullptr);
  if (thing->limited_)
    thing->read_only_ = true;
  thing->chunk_map_ = std::shared_ptr<std::vector<std::streamoff>>(
      new std::vector<std::streamoff>);
  assert(thing->chunk_map_ != nullptr);
  for (addr i = 0; !thing->limited_ || i < thing->chunk_map_limit_; i++) {
    std::streamoff where;
    mapf.read(reinterpret_cast<char *>(&where), sizeof(where));
    if (mapf.fail())
      break;
    else
      thing->chunk_map_->push_back(where);
  }
  if (thing->chunk_map_->size() == 0) {
    thing->size_ = 0;
  } else {
    thing->fetch_last_chunk();
    thing->size_ = thing->chunk_size_ * (thing->chunk_map_->size() - 1) +
                   thing->chunk_end_;
  }
  return thing;
}

std::shared_ptr<SimpleTxtIO> SimpleTxtIO::clone(std::string *error) {
  if (transacting()) {
    safe_set(error) = "Can't clone SimpleTxtIo during transaction";
    return nullptr;
  }
  std::shared_ptr<SimpleTxtIO> io =
      std::shared_ptr<SimpleTxtIO>(new SimpleTxtIO(
          nameof_contents_, nameof_chunk_map_, chunk_size_, compressor_));
  assert(io != nullptr);
  io->chunk_ = std::unique_ptr<char[]>(new char[io->chunk_size_ + 1]);
  assert(io->chunk_ != nullptr);
  io->buffer_size_ = chunk_size_ + io->compressor_->extra(io->chunk_size_) + 1;
  io->buffer_ = std::unique_ptr<char[]>(new char[io->buffer_size_]);
  assert(io->buffer_ != nullptr);
  io->contents_.open(nameof_contents_, std::ios::binary | std::ios::in);
  if (io->contents_.fail()) {
    safe_set(error) =
        "SimpleTxtIO can't open file for cloning: " + io->nameof_contents_;
    return nullptr;
  }
  io->read_only_ = true;
  io->last_chunk_ = read_limits(nameof_contents_, &(io->chunk_map_limit_),
                                &(io->last_chunk_end_));
  io->limited_ = (io->last_chunk_ != nullptr);
  io->chunk_map_ = chunk_map_;
  io->size_ = size_;
  return io;
}

void SimpleTxtIO::fetch_chunk(size_t chunk_index) {
  assert(chunk_map_->size() > 0 && chunk_index < chunk_map_->size());
  if (chunk_valid_ && chunk_index_ == chunk_index)
    return;
  sync_last_chunk();
  chunk_valid_ = true;
  chunk_index_ = chunk_index;
  if (limited_ && chunk_index == chunk_map_->size() - 1) {
    memcpy(chunk_.get(), last_chunk_.get(), last_chunk_end_);
    chunk_end_ = last_chunk_end_;
    return;
  }
  std::streamoff where;
  if (chunk_index_ == 0)
    where = 0;
  else
    where = (*chunk_map_)[chunk_index_ - 1];
  std::streamsize amount = (*chunk_map_)[chunk_index_] - where;
  contents_.seekg(where, contents_.beg);
  assert(!contents_.fail());
  contents_.read(buffer_.get(), amount);
  assert(!contents_.fail());
  chunk_end_ =
      compressor_->tang(buffer_.get(), amount, chunk_.get(), chunk_size_ + 1);
  assert(chunk_end_ <= chunk_size_);
}

void SimpleTxtIO::fetch_last_chunk() { fetch_chunk(chunk_map_->size() - 1); }

void SimpleTxtIO::append_last_chunk(char *text, addr length) {
  assert(!limited_ && chunk_map_->size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_->size() - 1 &&
         chunk_end_ + length <= chunk_size_);
  memcpy(chunk_.get() + chunk_end_, text, length);
  chunk_end_ += length;
  last_chunk_synced_ = false;
}

void SimpleTxtIO::sync_last_chunk() {
  if (last_chunk_synced_)
    return;
  assert(!limited_ && chunk_map_->size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_->size() - 1 && chunk_end_ > 0);
  std::streamoff where;
  if (chunk_map_->size() == 1)
    where = 0;
  else
    where = (*chunk_map_)[chunk_map_->size() - 2];
  std::streamsize amount =
      compressor_->crush(chunk_.get(), chunk_end_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  (*chunk_map_)[chunk_map_->size() - 1] = where;
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::in | std::ios::out);
  assert(!mapf.fail());
  mapf.seekp(-sizeof(where), mapf.end);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
  last_chunk_synced_ = true;
}

void SimpleTxtIO::append_full_chunk(char *text) {
  std::streamoff where;
  if (chunk_map_->size() == 0)
    where = 0;
  else
    where = (*chunk_map_)[chunk_map_->size() - 1];
  std::streamsize amount =
      compressor_->crush(text, chunk_size_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  chunk_map_->push_back(where);
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::out | std::ios::app);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
}

void SimpleTxtIO::start_new_chunk(char *text, addr length) {
  assert(!limited_ && length > 0 && length <= chunk_size_);
  chunk_valid_ = true;
  chunk_index_ = chunk_map_->size();
  memcpy(chunk_.get(), text, length);
  chunk_end_ = length;
  std::streamoff where;
  if (chunk_map_->size() == 0)
    where = 0;
  else
    where = (*chunk_map_)[chunk_map_->size() - 1];
  std::streamsize amount =
      compressor_->crush(text, chunk_end_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  chunk_map_->push_back(where);
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::out | std::ios::app);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
  last_chunk_synced_ = true;
}

void SimpleTxtIO::append(char *text, addr length) {
  if (read_only_)
    return;
  if (length <= 0)
    return;
  flushed_ = false;
  if (chunk_map_->size() == 0) {
    start_new_chunk(text, std::min(chunk_size_, length));
    text += std::min(chunk_size_, length);
    length -= std::min(chunk_size_, length);
  } else {
    fetch_last_chunk();
    std::streamsize room = chunk_size_ - chunk_end_;
    if (room) {
      append_last_chunk(text, std::min(room, length));
      text += std::min(room, length);
      length -= std::min(room, length);
    }
  }
  if (length == 0) {
    size_ = chunk_size_ * (chunk_map_->size() - 1) + chunk_end_;
    return;
  }
  sync_last_chunk();
  while (length > chunk_size_) {
    append_full_chunk(text);
    text += chunk_size_;
    length -= chunk_size_;
  }
  if (length > 0)
    start_new_chunk(text, length);
  assert(chunk_map_->size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_->size() - 1);
  size_ = chunk_size_ * (chunk_map_->size() - 1) + chunk_end_;
}

std::unique_ptr<char[]> SimpleTxtIO::read(addr start, addr desired,
                                          addr *actual) {
  if (start < 0)
    start = 0;
  flush();
  if (start + desired > size())
    desired = size() - start;
  if (desired <= 0) {
    *actual = 0;
    return nullptr;
  }
  *actual = desired;
  std::unique_ptr<char[]> text = std::unique_ptr<char[]>(new char[desired + 1]);
  char *t = text.get();
  addr offset = start % chunk_size_;
  addr index = start / chunk_size_;
  if (offset > 0) {
    fetch_chunk(index);
    memcpy(t, chunk_.get() + offset, std::min(desired, chunk_size_ - offset));
    t += std::min(desired, chunk_size_ - offset);
    desired -= std::min(desired, chunk_size_ - offset);
    if (desired == 0) {
      *t = '\0';
      return text; // often helpful
    }
    index++;
  }
  while (desired > chunk_size_) {
    std::streamoff where;
    if (index == 0)
      where = 0;
    else
      where = (*chunk_map_)[index - 1];
    contents_.seekg(where, contents_.beg);
    std::streamsize amount = (*chunk_map_)[index] - where;
    contents_.seekg(where, contents_.beg);
    contents_.read(buffer_.get(), amount);
    size_t actual =
        compressor_->tang(buffer_.get(), amount, t, chunk_size_ + 1);
    assert(actual == (size_t)chunk_size_);
    desired -= chunk_size_;
    t += chunk_size_;
    index++;
  }
  fetch_chunk(index);
  memcpy(t, chunk_.get(), desired);
  t += desired;
  *t = '\0'; // often helpful
  return text;
}

void SimpleTxtIO::dump(std::ostream &output) {
  for (size_t index = 0; index < chunk_map_->size(); index++) {
    fetch_chunk(index);
    output.write(chunk_.get(), chunk_end_);
  }
  output.flush();
}

bool SimpleTxtIO::transaction_(std::string *error) {
  if (read_only_) {
    safe_set(error) = "SimpleTxtIO is read only";
    return false;
  }
  std::string nameof_lock = nameof_contents_ + ".lock";
  if (link(nameof_contents_.c_str(), nameof_lock.c_str()) != 0) {
    safe_set(error) = "SimpleTxtIO can't obtain transaction lock";
    return false;
  }
  if (chunk_map_->size() > 0)
    fetch_last_chunk();
  std::string nameof_temp = nameof_contents_ + ".temp";
  std::fstream temp;
  temp.open(nameof_temp, std::ios::binary | std::ios::out);
  if (temp.fail()) {
    std::remove(nameof_lock.c_str());
    safe_set(error) = "SimpleTxtIO can't record transaction info";
    return false;
  }
  addr transaction_chunk_map_limit = chunk_map_->size();
  temp.write(reinterpret_cast<char *>(&transaction_chunk_map_limit),
             sizeof(addr));
  if (temp.fail()) {
    std::remove(nameof_lock.c_str());
    safe_set(error) = "SimpleTxtIO can't record transaction info";
    return false;
  }
  addr last_chunk_end = chunk_end_;
  temp.write(reinterpret_cast<char *>(&last_chunk_end), sizeof(addr));
  if (temp.fail()) {
    std::remove(nameof_lock.c_str());
    safe_set(error) = "SimpleTxtIO can't record transaction info";
    return false;
  }
  temp.write(chunk_.get(), chunk_end_);
  if (temp.fail()) {
    std::remove(nameof_lock.c_str());
    safe_set(error) = "SimpleTxtIO can't record transaction info";
    return false;
  }
  temp.close();
  std::string nameof_limits = nameof_contents_ + ".limits";
  if (std::rename(nameof_temp.c_str(), nameof_limits.c_str()) != 0) {
    std::remove(nameof_temp.c_str());
    std::remove(nameof_limits.c_str());
    std::remove(nameof_lock.c_str());
    safe_set(error) = "SimpleTxtIO can't initiate transaction";
    return false;
  }
  return true;
}

bool SimpleTxtIO::ready_() {
  flush();
  return true;
}

void SimpleTxtIO::commit_() { unlock(nameof_contents_); }

void SimpleTxtIO::abort_() {
  restore(nameof_contents_, nameof_chunk_map_, chunk_size_, compressor_);
  chunk_valid_ = false;
  last_chunk_synced_ = true;
  flushed_ = true;
  contents_.close();
  contents_.open(nameof_contents_,
                 std::ios::binary | std::ios::out | std::ios::in);
  assert(!contents_.fail());
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::in | std::ios::out);
  last_chunk_ = nullptr;
  limited_ = false;
  chunk_map_->clear();
  for (;;) {
    std::streamoff where;
    mapf.read(reinterpret_cast<char *>(&where), sizeof(where));
    if (mapf.fail())
      break;
    else
      chunk_map_->push_back(where);
  }
  if (chunk_map_->size() == 0) {
    size_ = 0;
  } else {
    fetch_last_chunk();
    size_ = chunk_size_ * (chunk_map_->size() - 1) + chunk_end_;
  }
  unlock(nameof_contents_);
}

} // namespace cottontail
