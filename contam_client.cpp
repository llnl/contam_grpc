#include <grpcpp/grpcpp.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <filesystem>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#ifdef BAZEL_BUILD
#include "examples/protos/contam.grpc.pb.h"
#else
#include "contam.grpc.pb.h"
#endif

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");
ABSL_FLAG(std::string, ctm_prj_filename, "", "Contam project filename");

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using contam::ContamServer;
using contam::ContamReply;
using contam::ContamRequest;


// Parses a ctm_prj_fileiname from the command line flag value `text`.
// Returns true and sets `filename` on success; returns false and sets `*error`
// on failure.
bool AbslParseFlag(absl::string_view text, std::string* filename, std::string* error) {
  // Convert from text to int using the int-flag parser.
  if (!absl::ParseFlag(text, filename, error)) {
    return false;
  }
  std::filesystem::path p(*filename);
  if (p.filename().empty() || p.filename() == "." || p.filename() == "..") {
    *error = "Invalid Contam project filename";
    return false;
  }
  return true;
}

std::string contam_load_project_file()
{
  std::string prj_fname = absl::GetFlag(FLAGS_ctm_prj_filename);
  auto fsize = std::filesystem::file_size(prj_fname);
  std::string prj_file(fsize, '\0');
  std::ifstream ifs(prj_fname);
  ifs.read(&prj_file[0], fsize);
  ifs.close();
  return prj_file;
}

class ContamServerClient {
 public:
  ContamServerClient(std::shared_ptr<Channel> channel)
      : stub_(ContamServer::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string RunContam(const std::string& prj) {
    // Data we are sending to the server.
    ContamRequest request;
    request.set_prj(prj);

    // Container for the data we expect from the server.
    ContamReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->RunContam(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.stdout();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<ContamServer::Stub> stub_;
};

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  std::string target_str = absl::GetFlag(FLAGS_target);

  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  ContamServerClient contam_connector(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string prj_file = contam_load_project_file();
  std::string reply = contam_connector.RunContam(prj_file);
  std::cout << "ContamServer received: " << reply << std::endl;

  return 0;
}
