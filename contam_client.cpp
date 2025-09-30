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
#include <tuple>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

//#ifdef BAZEL_BUILD
//#include "examples/protos/contam.grpc.pb.h"
//#else
#include "contam.grpc.pb.h"
//#endif

#include "contam_client.h"
#include "contam_params.h"
#include <absl/types/optional.h>

ABSL_FLAG(std::string, ctm_target, "localhost:50051", "Server address");
ABSL_FLAG(std::string, ctm_prj_file, "", "Contam project filename");
ABSL_FLAG(std::string, ctm_of_prefix, "", "Prefix to contam output filenames");
ABSL_FLAG(absl::optional<bool>, ctm_out, absl::nullopt, "Write stdout from Contam server into a file .out");
ABSL_FLAG(absl::optional<bool>, ctm_err, absl::nullopt, "Write stderr from Contam server into a file .err");
ABSL_FLAG(absl::optional<bool>, ctm_ach, absl::nullopt, "Retrieve .ach output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_cex, absl::nullopt, "Retrieve .cex output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_csm, absl::nullopt, "Retrieve .csm output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_log, absl::nullopt, "Retrieve .log output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_rst, absl::nullopt, "Retrieve .rst output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_sim, absl::nullopt, "Retrieve .sim output file from Contam server");
ABSL_FLAG(absl::optional<bool>, ctm_xlog, absl::nullopt, "Retrieve .xlog output file from Contam server");
ABSL_FLAG(bool, ctm_verbose, false, "Verbosely parse the Contam param prototext file");
ABSL_FLAG(std::string, ctm_proto, "", "Filename of the prototext for Contam params");

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
  ContamParams contam_params;
  ClientParams client_params;
  std::string of_prefix;

  ContamState();
  void Clear();
  void InitState(int r);
  std::string to_string() const;
};

ContamState::ContamState()
: sim_step(0u), rank(-1)
{}

void ContamState::Clear() {
  sim_id.clear();
  sim_step = 0u;
  rank = -1;
  contam_params.clear();
  client_params.clear();
}

void comment_on_overriden_params(bool& overriden)
{
  if (overriden) { // already made the remark
    return;
  }
  overriden = true;
  std::cerr << "Overriding prototext params via command-line arguments\n";
}

void ContamState::InitState(int r) {
  sim_id = getUniqStr(rank = r);
  sim_step = 0u;

  using std::cerr;
  using std::string;
  using std::endl;

  const auto ctm_proto = absl::GetFlag(FLAGS_ctm_proto);
  const bool ctm_verbose = absl::GetFlag(FLAGS_ctm_verbose);
  bool overriden = false;
  if (!ctm_proto.empty()) {
    std::tie(contam_params, client_params) = read_proto_params(ctm_proto, ctm_verbose);
    if (ctm_verbose) {
      cerr << "---- end of the '" + ctm_proto + "' ----" << endl;
    }
  }

  const auto prj_filename_tmp =  absl::GetFlag(FLAGS_ctm_prj_file);
  if (!prj_filename_tmp.empty()) {
    contam_params.prj_filename = prj_filename_tmp;
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  Contam project file: " + contam_params.prj_filename << endl;
    }
  }

  absl::optional<bool> ctm_out = absl::GetFlag(FLAGS_ctm_out);
  if (ctm_out.has_value()) {
    client_params.of_out = ctm_out.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_out: " + string(client_params.of_out ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_err = absl::GetFlag(FLAGS_ctm_err);
  if (ctm_err.has_value()) {
    client_params.of_err = ctm_err.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_err: " + string(client_params.of_err ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_ach = absl::GetFlag(FLAGS_ctm_ach);
  if (ctm_ach.has_value()) {
    client_params.of_ach = ctm_ach.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_ach: " + string(client_params.of_ach ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_cex = absl::GetFlag(FLAGS_ctm_cex);
  if (ctm_cex.has_value()) {
    client_params.of_cex = ctm_cex.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_cex: " + string(client_params.of_cex ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_csm = absl::GetFlag(FLAGS_ctm_csm);
  if (ctm_csm.has_value()) {
    client_params.of_csm = ctm_csm.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_csm: " + string(client_params.of_csm ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_log = absl::GetFlag(FLAGS_ctm_log);
  if (ctm_log.has_value()) {
    client_params.of_log = ctm_log.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_log: " + string(client_params.of_log ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_rst = absl::GetFlag(FLAGS_ctm_rst);
  if (ctm_rst.has_value()) {
    client_params.of_rst = ctm_rst.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_rst: " + string(client_params.of_rst ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_sim = absl::GetFlag(FLAGS_ctm_sim);
  if (ctm_sim.has_value()) {
    client_params.of_sim = ctm_sim.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_sim: " + string(client_params.of_sim ? "true\n" : "false\n");
    }
  }

  absl::optional<bool> ctm_xlog = absl::GetFlag(FLAGS_ctm_xlog);
  if (ctm_xlog.has_value()) {
    client_params.of_xlog = ctm_xlog.value();
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_xlog: " + string(client_params.of_xlog? "true\n" : "false\n");
    }
  }

  const auto of_prefix_tmp = absl::GetFlag(FLAGS_ctm_of_prefix);
  if (!of_prefix_tmp.empty()) {
    client_params.of_prefix = of_prefix_tmp;
    if (ctm_verbose) {
      comment_on_overriden_params(overriden);
      cerr << "  of_prefix: " + client_params.of_prefix << endl;
    }
  }
}

std::string ContamState::to_string() const
{
  std::string str
    = std::string("{\n")
    + "  sim_id: " + sim_id + "\n"
    + "  sim_step: " + std::to_string(sim_step) + "\n"
    + "  rank: " + std::to_string(rank) + "\n"
    + "  contam_params: \n" + contam_params.to_string() + "\n"
    + "  client_params: \n" + client_params.to_string() + "\n}";
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

ContamResponse ContamServerClient::RunContam(const std::vector<char>& prj)
{
  std::cout << "================ Contam setup ================\n";
  std::cout << ctmState.to_string() << std::endl;
  std::cout << "==============================================\n";
  // Message to send to the server.
  ContamRequest request;
  request.set_sim_id(ctmState.sim_id);
  request.set_prj(prj.data(), prj.size());
  request.set_ach(ctmState.client_params.of_ach);
  request.set_cex(ctmState.client_params.of_cex);
  request.set_csm(ctmState.client_params.of_csm);
  request.set_log(ctmState.client_params.of_log);
  request.set_rst(ctmState.client_params.of_rst);
  request.set_sim(ctmState.client_params.of_sim);
  request.set_xlog(ctmState.client_params.of_xlog);

  // Reponse received from the server
  ContamReply reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  ClientContext context;

  // The actual RPC
  Status status = stub_->RunContam(&context, request, &reply);

  const auto& of_prefix = ctmState.client_params.of_prefix;
  mkdir_as_needed(of_prefix.c_str());
  const auto uniq_str = of_prefix
    + ((!of_prefix.empty() && of_prefix.back() != '/')?  "/ctm." : "ctm.")
    + ctmState.sim_id + '.' + std::to_string(ctmState.sim_step);
  std::cout << "Prefix of output files: " << uniq_str << std::endl;


  if (ctmState.client_params.of_out) {
    writeFile(uniq_str + ".out", reply.stdout(), std::ios_base::app);
  } else {
    std::cout << reply.stdout() << std::endl;
  }

  if (ctmState.client_params.of_err) {
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

  const auto prj_file = loadFile(ctmState.contam_params.prj_filename);
  std::cout << "Loaded Contam project file '" + ctmState.contam_params.prj_filename
             + "' of " + std::to_string(prj_file.size()) + " bytes\n";

  const auto reply = contam_connector.RunContam(prj_file);

  std::cout << "Contam RPC reply status: " << (reply.status? "success" : "fail") << std::endl;

  return reply;
}

} // end of namespace
