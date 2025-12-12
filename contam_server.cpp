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

#if defined(BUILD_DEBUG)
  #define NOTICE_ON_PACKED_OUTFILE(_ext_) \
  if (verbose) { \
    printf("------ " #_ext_ " packed for simulation (%s) !\n", \
    request->sim_id().c_str()); \
  }
#else
  #define NOTICE_ON_PACKED_OUTFILE(_ext_)
#endif

#define LOAD_AND_DELETE_OUTFILE(_ext_) do { \
  const std::string fname = uniq_str + "." #_ext_; \
  if (request->_ext_()) { \
    auto const content = loadFile(fname); \
    reply->set_##_ext_(content.data(), content.size()); \
  } \
  unlink(fname.c_str()); \
  NOTICE_ON_PACKED_OUTFILE(_ext_) \
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
  const std::string of_prefix = absl::GetFlag(FLAGS_ctm_of_prefix);
  mkdir_as_needed(of_prefix.c_str());

  auto const uniq_str =
    of_prefix
    + ((!of_prefix.empty() && of_prefix.back() != '/') ? "/ctm." : "ctm.")
    + request->sim_id() + '.' + std::to_string(request->sim_step());
  std::cout << "Prefix of output files: " << uniq_str << std::endl;

  std::string prj_fname = uniq_str + ".prj";
  std::string stdout_fname = uniq_str + ".out";
  std::string stderr_fname = uniq_str + ".err";

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

  ok = runContam(request->sim_id(),
                 prj_fname,
                 stdout_fname,
                 stderr_fname,
                 std::move(ambs),
                 wp_wo_envelope,
                 verbose);

  if (verbose) {
    printf("------ Simulation (%s) %s!\n",
           request->sim_id().c_str(),
           (ok? "complete" : "failed"));
  }

  unlink(prj_fname.c_str());

  {
    auto const content = loadFile(stdout_fname);
    reply->set_stdout(content.data(), content.size());
  }
  unlink(stdout_fname.c_str());

  {
    auto const content = loadFile(stderr_fname);
    reply->set_stderr(content.data(), content.size());
  }
  unlink(stderr_fname.c_str());

  if (verbose) {
    printf("------ stdout/stderr packed for simulation (%s) !\n", request->sim_id().c_str());
  }

  LOAD_AND_DELETE_OUTFILE(ach);
  LOAD_AND_DELETE_OUTFILE(cex);
  LOAD_AND_DELETE_OUTFILE(csm);
  LOAD_AND_DELETE_OUTFILE(log);
  LOAD_AND_DELETE_OUTFILE(rst);
  LOAD_AND_DELETE_OUTFILE(sim);
  LOAD_AND_DELETE_OUTFILE(xlog);

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
