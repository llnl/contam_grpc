#include "utils.h"

#include <memory>
#include <thread>  // this_thread

#include "contam_driver.h"
#include <google/protobuf/repeated_ptr_field.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"
#include "contam_ambience.pb.h"
#include "contam_rpc.grpc.pb.h"

using contam::rpc::ContamReply;
using contam::rpc::ContamRequest;
using contam::rpc::ContamServer;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");
ABSL_FLAG(bool, ctm_verbose, false, "verbosely report");
ABSL_FLAG(std::string,
          ctm_of_prefix,
          "/tmp/ctm",
          "Prefix of the names of Contam output files");

namespace Contam_gRPC {

/** Compare the first 'prefix.size()' characters of 'str' with 'prefix' */
bool has_prefix(const std::string& str, const std::string& prefix) {
  if (str.size() >= prefix.size()) {
    return (str.compare(0, prefix.size(), prefix) == 0);
  }
  return false;
}

#if defined(BUILD_DEBUG)
#define NOTICE_ON_PACKED_FILE(verbose, request, ext)                           \
  if (verbose) {                                                               \
    const auto sim_id = (request)->sim_id().c_str();                           \
    printf("------ %s packed for simulation (%s) !\n", ext, sim_id);           \
  }
#else
#define NOTICE_ON_PACKED_FILE(verbose, request, ext)
#endif

#define SET_FILE_REPLY(reply, verbose, request, prefix, fname, ext)            \
  do {                                                                         \
      auto const content = loadFile(prefix + fname);                           \
      contam::rpc::File* file = (reply)->add_files();                          \
      file->set_filename(fname);                                               \
      file->set_data(content.data(), content.size());                          \
      if (file->data().size() != content.size()) {                             \
        printf("incorrect length of packed field (%s): %lu -> %lu\n",          \
               ext, content.size(), file->data().size());                      \
      } else if (content.size() == 0ul) {                                      \
        printf("(%s) is empty\n", ext);                                        \
      } else {                                                                 \
        NOTICE_ON_PACKED_FILE((verbose), (request), ext)                       \
      }                                                                        \
  } while (0)

#define LOAD_AND_DEL_STDFILE(request, reply, verbose, prefix, uniq_str, _ext_) \
  do {                                                                         \
    const std::string fname = (uniq_str) + "." #_ext_;                         \
    SET_FILE_REPLY((reply), (verbose), (request), (prefix), fname, #_ext_);    \
    unlink(fname.c_str());                                                     \
  } while (0)

#define LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, _ext_)    \
  do {                                                                         \
    const auto flist = filesWithExtension(#_ext_, (prefix), (uniq_str));       \
    if ((request)->_ext_()) {                                                  \
      for (const auto& fn: flist) {                                            \
        SET_FILE_REPLY((reply), (verbose), (request), (prefix), fn, #_ext_);   \
      }                                                                        \
    }                                                                          \
    for (const auto& fn: flist) {                                              \
      unlink(fn.c_str());                                                      \
    }                                                                          \
  } while (0)


bool set_ambience(
  std::vector<AmbienceData>& ambiences,
  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const&
    proto_ambiences,
  int ambiences_size,
  bool const verbose = false)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.clear();
  ambiences.reserve(n);

  for (auto const& proto_amb : proto_ambiences) {
    auto& amb_data = ambiences.emplace_back();
    auto& amb_series = amb_data.data();
    amb_series.reserve(proto_amb.amb_series_size());

    for (auto const& s : proto_amb.amb_series()) {
      auto const& mf = s.mass_fraction();
      amb_series.emplace_back(s.time(),
                              s.temperature(),
                              s.pressure(),
                              s.wind_speed(),
                              s.wind_direction(),
                              std::vector<CTM_R8> {s.mass_fraction().begin(),
                                                   s.mass_fraction().end()});
    }
    if (!amb_data.init()) {
      return false;
    }
  }
  if (verbose) {
    printf("------ Ambience set\n");
    printf("amb_series:\n[\n");
    for (auto const& ad : ambiences) {
      printf("%s,\n", ad.to_string(2).c_str());
    }
    printf("]\n");
  }
  return true;
}

bool set_envelope(
  std::vector<AmbienceData>& ambiences,
  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const&
    proto_ambiences,
  int ambiences_size,
  bool const verbose = false)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.clear();
  ambiences.reserve(n);

  for (auto const& proto_amb : proto_ambiences) {
    auto& amb_data = ambiences.emplace_back();
    auto& amb_series = amb_data.data();
    amb_series.reserve(proto_amb.amb_series_size());

    for (auto const& s : proto_amb.amb_series()) {
      auto const& mf = s.mass_fraction();
      amb_series.emplace_back(s.time(),
                              s.pressure(),
                              std::vector<CTM_R8> {s.mass_fraction().begin(),
                                                   s.mass_fraction().end()});
    }
    if (!amb_data.init()) {
      return false;
    }
  }
  if (verbose) {
    printf("------ Envelop set\n");
    printf("amb_series:\n[\n");
    for (auto const& ad : ambiences) {
      printf("%s,\n", ad.to_string(2).c_str());
    }
    printf("]\n");
  }
  return true;
}

// Logic and data behind the server's behavior.
class ContamServerServiceImpl final : public ContamServer::Service {
  Status RunContam(ServerContext* context,
                   ContamRequest const* request,
                   ContamReply* reply) override;
};

Status ContamServerServiceImpl::RunContam(ServerContext* context,
                                          ContamRequest const* request,
                                          ContamReply* reply)
{
  std::string prefix = absl::GetFlag(FLAGS_ctm_of_prefix);
  prefix += ((!prefix.empty() && prefix.back() != '/') ? "/" : "");
  prefix += std::to_string(request->sim_step()) + "/";
  mkdir_as_needed(prefix.c_str());

  auto const sim_id = request->sim_id();
  auto const uniq_str = "ctm." + sim_id;
  std::cout << "Prefix of output files: " << prefix + uniq_str << std::endl;

  std::string prj_fname = prefix + uniq_str + ".prj";

  writeFile(prj_fname, request->prj());
  std::cout << "Contam project received: " << prj_fname << std::endl;

  bool const verbose = absl::GetFlag(FLAGS_ctm_verbose);
  unsigned const wp_wo_envelope = request->wp_wo_envelope();

  std::vector<AmbienceData> ambs;
  bool ok = false;

  if (wp_wo_envelope == 1) {
    ok = set_ambience(
      ambs, request->ambiences(), request->ambiences_size(), verbose);
  } else if (wp_wo_envelope == 0) {
    ok = set_envelope(
      ambs, request->ambiences(), request->ambiences_size(), verbose);
  }

  if (!ok) {
    std::cerr << "Failed to set ambience!" << std::endl;
  }

  std::string stdout_fname = prefix + uniq_str + ".out";
  std::string stderr_fname = prefix + uniq_str + ".err";

  ok = runContam(sim_id,
                 prj_fname,
                 stdout_fname,
                 stderr_fname,
                 std::move(ambs),
                 wp_wo_envelope,
                 verbose);

  if (verbose) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("------ Simulation (%s) %s!\n",
           sim_id.c_str(),
           (ok ? "complete" : "failed"));
  }

  unlink(prj_fname.c_str());

  LOAD_AND_DEL_STDFILE(request, reply, verbose, prefix, uniq_str, out);
  LOAD_AND_DEL_STDFILE(request, reply, verbose, prefix, uniq_str, err);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, ach);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, cex);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, csm);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, log);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, rst);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, sim);
  LOAD_AND_DEL_FILE(request, reply, verbose, prefix, uniq_str, xlog);

  if (verbose) {
    printf("------ Packed response of simulation (%s) "
           "into a %lu byte-long message!\n",
           request->sim_id().c_str(), reply->ByteSizeLong());
  }

  std::fflush(stdout);
  std::fflush(stderr);

  return Status::OK;
}

void RunServer(uint16_t port)
{
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
  ContamServerServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

}  // namespace Contam_gRPC

int main(int argc, char** argv)
{
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  Contam_gRPC::RunServer(absl::GetFlag(FLAGS_port));
}
