#include "utils.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <sstream>
#ifdef __linux__
#include <unistd.h>

#include <sys/syscall.h>
#endif

namespace Contam_gRPC {

std::string getUniqStr(int rank)
{
  const std::string rank_str = std::to_string(rank);

#ifdef __linux__
  const std::string tid_str = std::to_string(syscall(SYS_gettid));
#else
  std::ostringstream ss;
  ss << std::this_thread::get_id();
  const std::string tid_str = ss.str();
#endif

  return (rank_str + "-" + tid_str);
}

bool checkFilename(std::string const& filename)
{
  std::filesystem::path p(filename);
  if (p.filename().empty() || p.filename() == "." || p.filename() == "..") {
    return false;
  }
  return true;
}

bool canReadFile(const std::string& filename)
{
  std::ifstream file(filename);
  return file.good();
}

bool hasPrefix(const std::string& str, const std::string& prefix)
{
  return (str.size() >= prefix.size()) &&
         (str.compare(0, prefix.size(), prefix) == 0);
}

std::vector<std::string> filesWithExtension(
    const std::string& ext,
    const std::string& dir,
    const std::string& prefix)
{
  std::vector<std::string> result;

  const std::filesystem::path& fdir = dir;

  if (!std::filesystem::exists(fdir) || !std::filesystem::is_directory(fdir)) {
    return result;
  }

  const std::string _ext = '.' + ext;

  for (const auto& entry : std::filesystem::directory_iterator(fdir))
  {
    if (entry.is_regular_file() &&
        entry.path().extension() == _ext)
    {
      const auto& fname = entry.path().filename().string();
      if (hasPrefix(fname, prefix) && canReadFile(fdir.string() + fname)) {
        result.push_back(fname);
      }
    }
  }
  return result;
}

std::string addStrToFilenameBeforeExt(std::string const& filename,
                                      std::string const& str)
{
  std::filesystem::path p(filename);

  // Get the filename without the extension
  std::string fnameWOExt = p.stem().string();

  // Get the extension (including the dot)
  std::string ext = p.extension().string();

  // Construct the new filename
  std::string newFilename = fnameWOExt + str + ext;

  // Create a new path with the modified filename
  std::filesystem::path newPath = p.parent_path() / newFilename;

  return newPath.string();
}

std::vector<char> loadFile(std::string const& fname)
{
  if (!checkFilename(fname)) {
    std::cerr << fname + " is not valid a filename!\n";
    return std::vector<char> {};
  }

  std::ifstream ifs(fname, std::ios_base::in | std::ios_base::binary);
  if (!ifs) {
    std::cerr << "Cannot open " + fname + " for reading!\n";
    return std::vector<char> {};
  }
  auto fsize = std::filesystem::file_size(fname);
  std::vector<char> content(fsize, 0u);
  ifs.read(&content[0], fsize);
  ifs.close();
  return content;
}

size_t writeFile(std::string const& fname,
                 std::string const& content,
                 std::ios_base::openmode mode)
{
  if (!checkFilename(fname)) {
    std::cerr << "Invalid filename: " + fname << std::endl;
    return static_cast<size_t>(0ul);
  }

  std::ofstream ofs(fname, mode | std::ios::binary);
  if (!ofs) {
    std::cerr << "Failed to open " << fname << " for writing." << std::endl;
    return static_cast<size_t>(0ul);
  }
  ofs.write(content.data(), content.size());
  ofs.close();
  return content.size();
}

void print(std::vector<char> const& data)
{
  std::copy(
    data.cbegin(), data.cend(), std::ostream_iterator<char>(std::cout, " "));
  std::cout << std::endl;
}

}  // namespace Contam_gRPC

#include <fcntl.h>   // open
#include <libgen.h>  // basename dirname

#include <cerrno>

#include <sys/stat.h>   // open
#include <sys/types.h>  // open
//#include <climits>
//#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Contam_gRPC {

/**
 * Recursively create a directory
 * https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix
 */
int mkpath(char const* dir, const mode_t m)
{
  struct stat sb;

  if (!dir) {
    errno = EINVAL;
    return 1;
  }

  if (!stat(dir, &sb))
    return 0;

  mkpath(dirname(strdupa(dir)), m);

  return mkdir(dir, m);
}

int mkdir_as_needed(char const* path)
{
  if (path == NULL || strlen(path) == 0ul) {
    fprintf(stderr, "Cannot create a directory with no name\n");
    return -3;
  }

  const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
  struct stat sb;

  const mode_t RWX_UGO = (S_IRWXU | S_IRWXG | S_IRWXO);

  if (stat(path, &sb) == 0) {        // already exist
    if (S_ISDIR(sb.st_mode) == 0) {  // not a directory
      fprintf(stderr, "\"%s\" already exists not as a directory\n", path);
      return -2;
    } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
      fprintf(stderr,
              "Directory \"%s\" already exists with "
              "different permission bits %o from "
              "the requested %o\n",
              path,
              (sb.st_mode & RWX_UGO),
              (m & RWX_UGO));
      return 5;  // already exists but with different mode
    }
    return 1;  // already exists
  }
  fprintf(stderr, "Creating directory \"%s\"\n", path);

  const mode_t old_mask = umask(0);
  if (mkpath(path, m) != 0) {
    if (stat(path, &sb) == 0) {        // already exist
      if (S_ISDIR(sb.st_mode) == 0) {  // not a directory
        fprintf(stderr, "\"%s\" already exists not as a directory\n", path);
        return -4;
      } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
        fprintf(stderr,
                "Directory \"%s\" already exists with "
                "different permission bits %o from "
                "the requested %o\n",
                path,
                (sb.st_mode & RWX_UGO),
                (m & RWX_UGO));
        return 5;  // already exists but with different mode
      }
      return 1;  // already exists
    }
    fprintf(
      stderr, "Cannot create directory \"%s\": %s\n", path, strerror(errno));
    perror("mkdir_as_needed() ");
    return -1;
  }
  umask(old_mask);

  return 0;  // The new directory has been succesfully created
}

}  // namespace Contam_gRPC
