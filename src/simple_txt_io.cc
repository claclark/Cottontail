#include "src/simple_txt_io.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace cottontail {

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
  if (thing->contents_.fail())
    thing->contents_.open(nameof_contents, std::ios::binary | std::ios::in);
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
  if (mapf.fail())
    mapf.open(thing->nameof_chunk_map_, std::ios::binary | std::ios::in);
  if (mapf.fail()) {
    // We give up
    safe_set(error) = "SimpleTxtIO can't access: " + thing->nameof_chunk_map_;
    return nullptr;
  }
  for (;;) {
    std::streamoff where;
    mapf.read(reinterpret_cast<char *>(&where), sizeof(where));
    if (mapf.fail())
      break;
    else
      thing->chunk_map_.push_back(where);
  }
  if (thing->chunk_map_.size() == 0) {
    thing->size_ = 0;
  } else {
    thing->fetch_last_chunk();
    thing->size_ =
        thing->chunk_size_ * (thing->chunk_map_.size() - 1) + thing->chunk_end_;
  }
  return thing;
}

void SimpleTxtIO::fetch_chunk(size_t chunk_index) {
  assert(chunk_map_.size() > 0 && chunk_index < chunk_map_.size());
  if (chunk_valid_ && chunk_index_ == chunk_index)
    return;
  sync_last_chunk();
  chunk_valid_ = true;
  chunk_index_ = chunk_index;
  std::streamoff where;
  if (chunk_index_ == 0)
    where = 0;
  else
    where = chunk_map_[chunk_index_ - 1];
  std::streamsize amount = chunk_map_[chunk_index_] - where;
  contents_.seekg(where, contents_.beg);
  assert(!contents_.fail());
  contents_.read(buffer_.get(), amount);
  assert(!contents_.fail());
  chunk_end_ =
      compressor_->tang(buffer_.get(), amount, chunk_.get(), chunk_size_ + 1);
  assert(chunk_end_ <= chunk_size_);
}

void SimpleTxtIO::fetch_last_chunk() { fetch_chunk(chunk_map_.size() - 1); }

void SimpleTxtIO::append_last_chunk(char *text, addr length) {
  assert(chunk_map_.size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_.size() - 1 &&
         chunk_end_ + length <= chunk_size_);
  memcpy(chunk_.get() + chunk_end_, text, length);
  chunk_end_ += length;
  last_chunk_synced_ = false;
}

void SimpleTxtIO::sync_last_chunk() {
  if (last_chunk_synced_)
    return;
  assert(chunk_map_.size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_.size() - 1 && chunk_end_ > 0);
  std::streamoff where;
  if (chunk_map_.size() == 1)
    where = 0;
  else
    where = chunk_map_[chunk_map_.size() - 2];
  std::streamsize amount =
      compressor_->crush(chunk_.get(), chunk_end_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  chunk_map_[chunk_map_.size() - 1] = where;
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
  if (chunk_map_.size() == 0)
    where = 0;
  else
    where = chunk_map_[chunk_map_.size() - 1];
  std::streamsize amount =
      compressor_->crush(text, chunk_size_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  chunk_map_.push_back(where);
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::out | std::ios::app);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
}

void SimpleTxtIO::start_new_chunk(char *text, addr length) {
  assert(length > 0 && length <= chunk_size_);
  chunk_valid_ = true;
  chunk_index_ = chunk_map_.size();
  memcpy(chunk_.get(), text, length);
  chunk_end_ = length;
  std::streamoff where;
  if (chunk_map_.size() == 0)
    where = 0;
  else
    where = chunk_map_[chunk_map_.size() - 1];
  std::streamsize amount =
      compressor_->crush(text, chunk_end_, buffer_.get(), buffer_size_);
  contents_.seekp(where, contents_.beg);
  assert(!contents_.fail());
  contents_.write(buffer_.get(), amount);
  assert(!contents_.fail());
  where += amount;
  chunk_map_.push_back(where);
  std::fstream mapf(nameof_chunk_map_,
                    std::ios::binary | std::ios::out | std::ios::app);
  assert(!mapf.fail());
  mapf.write(reinterpret_cast<char *>(&where), sizeof(where));
  assert(!mapf.fail());
  last_chunk_synced_ = true;
}

void SimpleTxtIO::append(char *text, addr length) {
  if (length <= 0)
    return;
  flushed_ = false;
  if (chunk_map_.size() == 0) {
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
    size_ = chunk_size_ * (chunk_map_.size() - 1) + chunk_end_;
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
  assert(chunk_map_.size() > 0 && chunk_valid_ &&
         chunk_index_ == chunk_map_.size() - 1);
  size_ = chunk_size_ * (chunk_map_.size() - 1) + chunk_end_;
}

std::unique_ptr<char[]> SimpleTxtIO::read(addr start, addr desired,
                                          addr *actual) {
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
      where = chunk_map_[index - 1];
    contents_.seekg(where, contents_.beg);
    std::streamsize amount = chunk_map_[index] - where;
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
  for (size_t index = 0; index < chunk_map_.size(); index++) {
    fetch_chunk(index);
    output.write(chunk_.get(), chunk_end_);
  }
  output.flush();
}
} // namespace cottontail
