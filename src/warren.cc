#include "src/warren.h"

#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "meadowlark/remote_warren.h"
#include "meadowlark/server.h"
#include "src/bigwig.h"
#include "src/core.h"
#include "src/dna.h"
#include "src/hazel.h"
#include "src/recipe.h"
#include "src/simple_warren.h"
#include "src/wlock.h"

namespace cottontail {
std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  if (name != "simple") {
    safe_error(error) =
        "The named Warren activation factory supports only Simple burrows";
    return nullptr;
  }
  return SimpleWarren::make(burrow, error);
}

namespace {
bool is_regular_file(const std::string &filename) {
  if (filename == "")
    return false;
  struct stat status;
  if (stat(filename.c_str(), &status) != 0)
    return false;
  return S_ISREG(status.st_mode);
}

std::shared_ptr<Warren> single_file_burrow(const std::string &burrow,
                                           std::string *error) {
  std::fstream in(burrow, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Can't open burrow: " + burrow;
    return nullptr;
  }
  const std::string magic = cottontail_file_magic;
  std::string actual(magic.size(), '\0');
  in.read(&actual[0], actual.size());
  if (actual != magic) {
    safe_error(error) = "Bad burrow magic number: " + burrow;
    return nullptr;
  }
  std::string dna;
  std::string line;
  bool found_blank = false;
  while (std::getline(in, line)) {
    if (line == "") {
      found_blank = true;
      break;
    }
    dna += line + "\n";
  }
  if (!found_blank) {
    safe_error(error) = "Burrow has no DNA terminator: " + burrow;
    return nullptr;
  }
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  auto warren = parameters.find("warren");
  if (warren == parameters.end()) {
    safe_error(error) = "Burrow has no warren type: " + burrow;
    return nullptr;
  }
  std::string name = warren->second;
  if (name == "hazel")
    return Hazel::make(burrow, dna, error);
  safe_error(error) = "Invalid warren type: " + name;
  return nullptr;
}

bool launch_server(const std::string &directory, const std::string &command,
                   std::string *error) {
  std::string logfile = directory + "/meadowlark.log";
  const char *log_name = logfile.c_str();
  const char *launch_command = command.c_str();
  pid_t child = fork();
  if (child < 0) {
    safe_error(error) = "Cannot launch Meadowlark server: " +
                        std::string(std::strerror(errno));
    return false;
  }
  if (child == 0) {
    if (setsid() < 0)
      _exit(1);
    int output = open(log_name, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW |
                                    O_NONBLOCK,
                      0666);
    struct stat status;
    if (output < 0 || fstat(output, &status) != 0 || !S_ISREG(status.st_mode) ||
        dup2(output, STDOUT_FILENO) < 0 || dup2(output, STDERR_FILENO) < 0)
      _exit(1);
    if (output > STDERR_FILENO)
      close(output);
    int input = open("/dev/null", O_RDONLY);
    if (input < 0 || dup2(input, STDIN_FILENO) < 0)
      _exit(1);
    if (input != STDIN_FILENO)
      close(input);
    // Reap the launcher now and let the detached server outlive this caller.
    pid_t server = fork();
    if (server != 0)
      _exit(server < 0 ? 1 : 0);
    execl("/bin/sh", "sh", "-c", launch_command, static_cast<char *>(nullptr));
    constexpr char message[] = "Cannot execute Meadowlark server command\n";
    write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(127);
  }
  int status;
  pid_t waited;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0)
    return true;
  safe_error(error) = "Cannot start Meadowlark server for " + directory +
                      " (log: " + logfile + ")";
  return false;
}
} // namespace

std::shared_ptr<Warren> Warren::make(const std::string &burrow,
                                     std::string *error) {
  if (is_regular_file(burrow))
    return single_file_burrow(burrow, error);
  const std::string &directory = burrow.empty() ? DEFAULT_BURROW : burrow;
  std::string lock_error;
  bool busy;
  auto wlock = WLock::obtain(directory, &lock_error, &busy);
  if (wlock == nullptr && !busy) {
    safe_set(error) = lock_error;
    return nullptr;
  }
  std::string dna, dna_error;
  std::map<std::string, std::string> parameters;
  if (read_dna(directory + "/" + DNA_NAME, &dna, &dna_error) &&
      cook(dna, &parameters, &dna_error) && parameters.contains("server")) {
    std::map<std::string, std::string> server;
    if (!cook(parameters.at("server"), &server, error))
      return nullptr;
    if (okay(server["active"])) {
      if (wlock != nullptr) {
        const std::string &command = server["command"];
        if (command.empty()) {
          safe_error(error) = "Meadowlark server command is missing for " +
                              directory;
          return nullptr;
        }
        if (!meadowlark::check_server_owner(directory + "/" + DNA_NAME, error))
          return nullptr;
        wlock.reset();
        if (!launch_server(directory, command, error))
          return nullptr;
      }
      return meadowlark::RemoteWarren::make(directory, error);
    }
  }
  if (wlock != nullptr)
    return local(wlock, error);
  safe_set(error) = lock_error;
  return nullptr;
}

std::shared_ptr<Warren> Warren::local(std::shared_ptr<WLock> wlock,
                                      std::string *error) {
  if (wlock == nullptr) {
    safe_error(error) = "Warren local activation needs a burrow lock";
    return nullptr;
  }
  if (!wlock->begin_activation(error))
    return nullptr;

  std::string activation_error;
  std::shared_ptr<Warren> warren;
  try {
    const std::string name = wlock->name();
    if (name == "simple")
      warren = SimpleWarren::make(wlock->working(), &activation_error);
    else if (name == "bigwig")
      warren = Bigwig::make(wlock, &activation_error);
    else if (name.empty())
      safe_error(&activation_error) =
          "Burrow has no valid DNA: " + wlock->working()->make_name(DNA_NAME);
    else
      safe_error(&activation_error) = "Invalid warren type: " + name;
  } catch (const std::exception &exception) {
    safe_error(&activation_error) =
        "Warren activation failed: " + std::string(exception.what());
  }

  if (warren != nullptr) {
    wlock->finish_activation("");
    return warren;
  }
  if (activation_error.empty())
    safe_error(&activation_error) = "Warren activation failed";
  wlock->finish_activation(activation_error);
  safe_set(error) = activation_error;
  return nullptr;
}

std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &recipe,
                                     const std::string &burrow,
                                     std::string *error, bool recreate) {
  if (name != "bigwig") {
    safe_error(error) = "Cannot create Warren of type: " + name;
    return nullptr;
  }
  if (!Bigwig::check_recipe(recipe, error))
    return nullptr;
  std::shared_ptr<WLock> wlock = recreate ? WLock::recreate(name, burrow, error)
                                          : WLock::create(name, burrow, error);
  if (wlock == nullptr)
    return nullptr;
  return Bigwig::make(wlock, recipe, error);
}

void Warren::commit_all(std::vector<std::shared_ptr<Warren>> warrens) {
  std::vector<std::shared_ptr<Bigwig>> bigwigs;
  for (auto &warren : warrens) {
    if (warren == nullptr || warren->name() != "bigwig")
      break;
    auto bigwig = std::dynamic_pointer_cast<Bigwig>(warren);
    if (bigwig == nullptr)
      break;
    bigwigs.push_back(bigwig);
  }
  if (bigwigs.size() == warrens.size() && Bigwig::commit_all(bigwigs))
    return;
  for (auto &warren : warrens)
    warren->commit();
}

std::shared_ptr<Warren> Warren::clone_(std::string *error) {
  safe_error(error) = "Warren type does not support cloning: " + name();
  return nullptr;
}
} // namespace cottontail
