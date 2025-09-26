/*! @file contam_client.cpp
    \brief Implementation of RPC client for Contam simulation service 
*/

#include <grpcpp/grpcpp.h>

#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <filesystem>
#include <thread>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#ifdef BAZEL_BUILD
#include "examples/protos/contam.grpc.pb.h"
#else
#include "contam.grpc.pb.h"
#endif

#include "contam_client.h"

ABSL_FLAG(std::string, ctm_target, "localhost:50051", "Server address");
ABSL_FLAG(std::string, ctm_prj_file, "", "Contam project filename");
ABSL_FLAG(std::string, ctm_of_prefix, "", "Prefix to contam output filenames");
ABSL_FLAG(bool, ctm_out, false, "Write stdout from Contam server into a file .out");
ABSL_FLAG(bool, ctm_err, false, "Write stderr from Contam server into a file .err");
ABSL_FLAG(bool, ctm_ach, false, "Retrieve .ach output file from Contam server");
ABSL_FLAG(bool, ctm_cex, false, "Retrieve .cex output file from Contam server");
ABSL_FLAG(bool, ctm_csm, false, "Retrieve .csm output file from Contam server");
ABSL_FLAG(bool, ctm_log, false, "Retrieve .log output file from Contam server");
ABSL_FLAG(bool, ctm_rst, false, "Retrieve .rst output file from Contam server");
ABSL_FLAG(bool, ctm_sim, false, "Retrieve .sim output file from Contam server");
ABSL_FLAG(bool, ctm_xlog, false, "Retrieve .xlog output file from Contam server");

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using contam::ContamServer;
using contam::ContamReply;
using contam::ContamRequest;


namespace Contam_gRPC {


struct ContamState {
  std::string sim_id;
  unsigned int sim_step;
  int rank;
  std::string of_prefix;
  bool of_out;
  bool of_err;
  bool of_ach;
  bool of_cex;
  bool of_csm;
  bool of_log;
  bool of_rst;
  bool of_sim;
  bool of_xlog;

  ContamState();
  void Clear();
  void InitState(int r);
  std::string to_string() const;
};

ContamState::ContamState()
: sim_step(0u), rank(-1), of_out(false), of_err(false), of_ach(false), of_cex(false),
  of_csm(false), of_log(false), of_rst(false), of_sim(false), of_xlog(false) {}

void ContamState::Clear() {
  sim_id.clear();
  sim_step = 0u;
  rank = -1;
  of_prefix.clear();
}

void ContamState::InitState(int r) {
  sim_id = getUniqStr(rank = r);
  sim_step = 0u;
  of_prefix = absl::GetFlag(FLAGS_ctm_of_prefix);

  of_out = absl::GetFlag(FLAGS_ctm_out);
  of_err = absl::GetFlag(FLAGS_ctm_err);
  of_ach = absl::GetFlag(FLAGS_ctm_ach);
  of_cex = absl::GetFlag(FLAGS_ctm_cex);
  of_csm = absl::GetFlag(FLAGS_ctm_csm);
  of_log = absl::GetFlag(FLAGS_ctm_log);
  of_rst = absl::GetFlag(FLAGS_ctm_rst);
  of_sim = absl::GetFlag(FLAGS_ctm_sim);
  of_xlog = absl::GetFlag(FLAGS_ctm_xlog);
}

std::string ContamState::to_string() const
{
  std::string str;
  str = " - sim_id: " + sim_id + "\n"
      + " - sim_step: " + std::to_string(sim_step) + "\n"
      + " - rank: " + std::to_string(rank) + "\n"
      + " - of_prefix: " + of_prefix + "\n"
      + " - of_out: " + std::string(of_out ? "true\n" : "false\n")
      + " - of_err: " + std::string(of_err ? "true\n" : "false\n")
      + " - of_ach: " + std::string(of_ach ? "true\n" : "false\n")
      + " - of_cex: " + std::string(of_cex ? "true\n" : "false\n")
      + " - of_csm: " + std::string(of_csm ? "true\n" : "false\n")
      + " - of_log: " + std::string(of_log ? "true\n" : "false\n")
      + " - of_rst: " + std::string(of_rst ? "true\n" : "false\n")
      + " - of_sim: " + std::string(of_sim ? "true\n" : "false\n")
      + " - of_xlog: " + std::string(of_xlog? "true\n" : "false\n");
  return str;
}


static thread_local ContamState ctmState;
static void ContaStateInit () __attribute__ ((constructor));
static void ContaStateFini () __attribute__ ((destructor));

void ContaStateInit () {
  ctmState.Clear();
}

void ContaStateFini () {
  ctmState.Clear();
}

void pickContamArgs(int argc, char** argv)
{
  absl::ParseCommandLine(argc, argv);
}

void initContamClientState(int rank)
{
  ctmState.InitState(rank);
}


class ContamServerClient {
 public:
  ContamServerClient(std::shared_ptr<Channel> channel)
      : stub_(ContamServer::NewStub(channel)) {}

  /*! \brief Sends the client's request and presents the response received from the server.
   */
  ContamResponse RunContam(const std::vector<char>& prj);

 private:
  std::unique_ptr<ContamServer::Stub> stub_;
};

ContamResponse ContamServerClient::RunContam(const std::vector<char>& prj) {
  // Message to send to the server.
  ContamRequest request;
  request.set_sim_id(ctmState.sim_id);
  request.set_prj(prj.data(), prj.size());
  request.set_ach(ctmState.of_ach);
  request.set_cex(ctmState.of_cex);
  request.set_csm(ctmState.of_csm);
  request.set_log(ctmState.of_log);
  request.set_rst(ctmState.of_rst);
  request.set_sim(ctmState.of_sim);
  request.set_xlog(ctmState.of_xlog);
  //std::cout << ctmState.to_string() << std::endl;

  // Reponse received from the server
  ContamReply reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  ClientContext context;

  // The actual RPC
  Status status = stub_->RunContam(&context, request, &reply);

  mkdir_as_needed(ctmState.of_prefix.c_str());
  const auto uniq_str = ctmState.of_prefix
    + ((!ctmState.of_prefix.empty() && ctmState.of_prefix.back() != '/')?  "/ctm." : "ctm.")
    + ctmState.sim_id + '.' + std::to_string(ctmState.sim_step);
  std::cout << "prefix of output files: " << uniq_str << std::endl;


  if (ctmState.of_out) {
    writeFile(uniq_str + ".out", reply.stdout(), std::ios_base::app);
  } else {
    std::cout << reply.stdout() << std::endl;
  }

  if (ctmState.of_err) {
    writeFile(uniq_str + ".err", reply.stderr(), std::ios_base::app);
  } else {
    std::cerr << reply.stderr() << std::endl;
  }


  if (reply.ach().size() != 0ul) {
    writeFile(uniq_str + ".ach", reply.ach(), std::ios_base::app);
  }

  if (reply.cex().size() != 0ul) {
    writeFile(uniq_str + ".cex", reply.cex(), std::ios_base::app);
  }

  if (reply.csm().size() != 0ul) {
    writeFile(uniq_str + ".csm", reply.csm(), std::ios_base::app);
  }

  if (reply.log().size() != 0ul) {
    writeFile(uniq_str + ".log", reply.log(), std::ios_base::app);
  }

  if (reply.rst().size() != 0ul) {
    writeFile(uniq_str + ".rst", reply.rst(), std::ios_base::app);
  }

  if (reply.sim().size() != 0ul) {
    writeFile(uniq_str + ".sim", reply.sim(), std::ios_base::app);
  }

  if (reply.xlog().size() != 0ul) {
    writeFile(uniq_str + ".xlog", reply.xlog(), std::ios_base::app);
  }

  ContamResponse response;
  if (status.ok()) {
    response.status = true;
  } else {
    response.status = false;
    response.err_str = std::to_string(status.error_code()) + ": " + status.error_message();
  }
  return response;
}

ContamResponse contamClient() {
  std::string target_str = absl::GetFlag(FLAGS_ctm_target);

  // Instantiate the client. RPCs are created out of a channel, that models
  // a connection to an endpoint.
  // The channel here isn't authenticated (thus InsecureChannelCredentials())
  ContamServerClient contam_connector(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  //std::string prj_file = loadContamProject();
  const auto prj_file = loadFile(absl::GetFlag(FLAGS_ctm_prj_file));
  const auto reply = contam_connector.RunContam(prj_file);

  std::cout << "Contam RPC reply status: " << (reply.status? "success" : "fail") << std::endl;

  return reply;
}

} // end of namespace
