#ifndef CONTAM_GRPC_UTILS_H
#define CONTAM_GRPC_UTILS_H
#include <string>
#include <vector>
#include <iostream>
#include <chrono>

namespace Contam_gRPC {

std::string getUniqStr(int rank);

bool checkFilename(const std::string& filename);

/*! \brief Add a string to a filename before extension */
std::string addStrToFilenameBeforeExt(const std::string& filename, const std::string& str);

std::vector<char> loadFile(const std::string& fname);

/*! Write a binary file */
size_t writeFile(
  const std::string& fname,
  const std::string& content,
  std::ios_base::openmode mode = std::ios_base::out);

/*! Print the content of a data */
void print(const std::vector<char>& data);

int mkdir_as_needed(const char* path);

inline double get_time() {
  using namespace std::chrono;
  return duration_cast<duration<double>>(
           steady_clock::now().time_since_epoch()).count();
}

} // end of namespace

#endif // CONTAM_GRPC_UTILS_H
