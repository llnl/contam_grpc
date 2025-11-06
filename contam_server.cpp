#include "utils.h"

#include <memory>
#include <thread>  // this_thread

#include "contam_driver.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <google/protobuf/repeated_ptr_field.h>

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


bool set_ambience(std::vector<AmbienceData>& ambiences,
                  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const& proto_ambiences,
                  int ambiences_size,
                  bool const verbose = false)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.clear();
  ambiences.reserve(n);

  for (const auto& proto_amb : proto_ambiences) {
    auto& amb_data = ambiences.emplace_back();
    auto& amb_series = amb_data.data();
    amb_series.reserve(proto_amb.amb_series_size());

    for (auto const& s : proto_amb.amb_series()) {
      auto const& mf = s.mass_fraction();
      amb_series.emplace_back(
        s.time(),
        s.temperature(),
        s.pressure(),
        s.wind_speed(),
        s.wind_direction(),
        std::vector<CTM_R8> {s.mass_fraction().begin(), s.mass_fraction().end()});
    }
    if (!amb_data.init()) {
      return false;
    }
  }
  if (verbose) {
    printf("amb_series:\n[\n");
    for (const auto& ad : ambiences) {
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
                   ContamReply* reply) override
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

    std::vector<AmbienceData> ambs;
    if (!set_ambience(
          ambs, request->ambiences(), request->ambiences_size(), verbose)) {
      std::cerr << "Failed to set ambience!" << std::endl;
    }

    runContam(request->sim_id(),
              prj_fname,
              stdout_fname,
              stderr_fname,
              std::move(ambs),
              request->wp_wo_envelop(),
              verbose);
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

    const std::string ach_fname = uniq_str + ".ach";
    if (request->ach()) {
      auto const content = loadFile(ach_fname);
      reply->set_ach(content.data(), content.size());
    }
    unlink(ach_fname.c_str());

    const std::string cex_fname = uniq_str + ".cex";
    if (request->cex()) {
      auto const content = loadFile(cex_fname);
      reply->set_cex(content.data(), content.size());
    }
    unlink(cex_fname.c_str());

    const std::string csm_fname = uniq_str + ".csm";
    if (request->csm()) {
      auto const content = loadFile(csm_fname);
      reply->set_csm(content.data(), content.size());
    }
    unlink(csm_fname.c_str());

    const std::string log_fname = uniq_str + ".log";
    if (request->log()) {
      auto const content = loadFile(log_fname);
      reply->set_log(content.data(), content.size());
    }
    unlink(log_fname.c_str());

    const std::string rst_fname = uniq_str + ".rst";
    if (request->rst()) {
      auto const content = loadFile(rst_fname);
      reply->set_rst(content.data(), content.size());
    }
    unlink(rst_fname.c_str());

    const std::string sim_fname = uniq_str + ".sim";
    if (request->sim()) {
      auto const content = loadFile(sim_fname);
      reply->set_sim(content.data(), content.size());
    }
    unlink(sim_fname.c_str());

    const std::string xlog_fname = uniq_str + ".xlog";
    if (request->xlog()) {
      auto const content = loadFile(xlog_fname);
      reply->set_xlog(content.data(), content.size());
    }
    unlink(xlog_fname.c_str());

    return Status::OK;
  }
};

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
