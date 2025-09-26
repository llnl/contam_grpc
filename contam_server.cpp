#include "utils.h"
#include "contam_driver.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <memory>
#include <thread> // this_thread
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"

#ifdef BAZEL_BUILD
#include "examples/protos/contam.grpc.pb.h"
#else
#include "contam.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using contam::ContamServer;
using contam::ContamReply;
using contam::ContamRequest;



ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");
ABSL_FLAG(std::string, ctm_of_prefix, "/tmp/ctm", "Prefix of the names of Contam output files");

namespace Contam_gRPC {

// Logic and data behind the server's behavior.
class ContamServerServiceImpl final : public ContamServer::Service {
  Status RunContam(ServerContext* context, const ContamRequest* request,
                  ContamReply* reply) override {

    const std::string of_prefix = absl::GetFlag(FLAGS_ctm_of_prefix);
    mkdir_as_needed(of_prefix.c_str());

    const auto uniq_str = of_prefix
      + ((!of_prefix.empty() && of_prefix.back() != '/')?  "/ctm." : "ctm.")
      + request->sim_id() + '.' + std::to_string(request->sim_step());
    std::cout << "prefix of output files: " << uniq_str << std::endl;

    std::string prj_fname = uniq_str + ".prj";
    std::string stdout_fname = uniq_str + ".out";
    std::string stderr_fname = uniq_str + ".err";

    writeFile(prj_fname, request->prj());
    std::cout << "Contam project received: " << prj_fname << std::endl;

    runContam(request->sim_id(), prj_fname, stdout_fname, stderr_fname);
    unlink(prj_fname.c_str());

    {
      const auto content = loadFile(stdout_fname);
      reply->set_stdout(content.data(), content.size());
    }
    unlink(stdout_fname.c_str());

    {
      const auto content = loadFile(stderr_fname);
      reply->set_stderr(content.data(), content.size());
    }
    unlink(stderr_fname.c_str());

    const std::string ach_fname = uniq_str + ".ach";
    if (request->ach()) {
      const auto content = loadFile(ach_fname);
      reply->set_ach(content.data(), content.size());
    }
    unlink(ach_fname.c_str());

    const std::string cex_fname = uniq_str + ".cex";
    if (request->cex()) {
      const auto content = loadFile(cex_fname);
      reply->set_cex(content.data(), content.size());
    }
    unlink(cex_fname.c_str());

    const std::string csm_fname = uniq_str + ".csm";
    if (request->csm()) {
      const auto content = loadFile(csm_fname);
      reply->set_csm(content.data(), content.size());
    }
    unlink(csm_fname.c_str());

    const std::string log_fname = uniq_str + ".log";
    if (request->log()) {
      const auto content = loadFile(log_fname);
      reply->set_log(content.data(), content.size());
    }
    unlink(log_fname.c_str());

    const std::string rst_fname = uniq_str + ".rst";
    if (request->rst()) {
      const auto content = loadFile(rst_fname);
      reply->set_rst(content.data(), content.size());
    }
    unlink(rst_fname.c_str());

    const std::string sim_fname = uniq_str + ".sim";
    if (request->sim()) {
      const auto content = loadFile(sim_fname);
      reply->set_sim(content.data(), content.size());
    }
    unlink(sim_fname.c_str());

    const std::string xlog_fname = uniq_str + ".xlog";
    if (request->xlog()) {
      const auto content = loadFile(xlog_fname);
      reply->set_xlog(content.data(), content.size());
    }
    unlink(xlog_fname.c_str());

    return Status::OK;
  }
};

void RunServer(uint16_t port) {
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

} // end of namespace


int main(int argc, char** argv)
{
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  Contam_gRPC::RunServer(absl::GetFlag(FLAGS_port));
}
