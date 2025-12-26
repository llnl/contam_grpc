#ifndef CONTAM_GRPC_UTILS_H
#define CONTAM_GRPC_UTILS_H
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace Contam_gRPC {

std::string getUniqStr(int rank);

bool checkFilename(std::string const& filename);

/*! \brief Add a string to a filename before extension */
std::string addStrToFilenameBeforeExt(std::string const& filename,
                                      std::string const& str);

/*! \brief Check if a file can be read */
bool canReadFile(std::string const& filename);

/** Compare the first 'prefix.size()' characters of 'str' with 'prefix' */
bool hasPrefix(std::string const& str, std::string const& prefix);

/*! \brief Return the names of files with a specific
    extension under a directory */
std::vector<std::string> filesWithExtension(std::string const& ext,
                                            std::string const& dir = "./",
                                            std::string const& prefix = "");

/*! \brief Load a file into a vector of characters */
std::vector<char> loadFile(std::string const& fname);

/*! Write a binary file */
size_t writeFile(std::string const& fname,
                 std::string const& content,
                 std::ios_base::openmode mode = std::ios_base::out);

/*! Print the content of a data */
void print(std::vector<char> const& data);

int mkdir_as_needed(char const* path);

inline double get_time()
{
  using namespace std::chrono;
  return duration_cast<duration<double>>(steady_clock::now().time_since_epoch())
    .count();
}

}  // namespace Contam_gRPC

#endif  // CONTAM_GRPC_UTILS_H
