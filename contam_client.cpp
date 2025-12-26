/*! @file contam_client.cpp
    \brief Implementation of RPC client for Contam simulation service
*/

#include "utils.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

#include <grpcpp/grpcpp.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "contam_client.h"
#include "contam_params.h"
#include "contam_rpc.grpc.pb.h"
#include <absl/types/optional.h>

ABSL_FLAG(std::string, ctm_target, "localhost:50051", "Server address");
ABSL_FLAG(std::string, ctm_prj_file, "", "Contam project filename");
ABSL_FLAG(std::string, ctm_of_prefix, "", "Prefix to contam output filenames");
ABSL_FLAG(bool, ctm_dryrun, false, "client-side dryrun");
ABSL_FLAG(absl::optional<bool>,
          ctm_out,
          absl::nullopt,
          "Write stdout from Contam server into a file .out");
ABSL_FLAG(absl::optional<bool>,
          ctm_err,
          absl::nullopt,
          "Write stderr from Contam server into a file .err");
ABSL_FLAG(absl::optional<bool>,
          ctm_ach,
          absl::nullopt,
          "Retrieve .ach output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_age,
          absl::nullopt,
          "Retrieve .age output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_bal,
          absl::nullopt,
          "Retrieve .bal output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_cbw,
          absl::nullopt,
          "Retrieve .cbw output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_cex,
          absl::nullopt,
          "Retrieve .cex output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_csm,
          absl::nullopt,
          "Retrieve .csm output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_ebw,
          absl::nullopt,
          "Retrieve .ebw output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_log,
          absl::nullopt,
          "Retrieve .log output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_rst,
          absl::nullopt,
          "Retrieve .rst output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_rxr,
          absl::nullopt,
          "Retrieve .rxr output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_rzf,
          absl::nullopt,
          "Retrieve .rzf output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_rzm,
          absl::nullopt,
          "Retrieve .rzm output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_rzi,
          absl::nullopt,
          "Retrieve .rzi output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_sim,
          absl::nullopt,
          "Retrieve .sim output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_sqlite3,
          absl::nullopt,
          "Retrieve .sqlite3 output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_srf,
          absl::nullopt,
          "Retrieve .srf output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_val,
          absl::nullopt,
          "Retrieve .val output file from Contam server");
ABSL_FLAG(absl::optional<bool>,
          ctm_xlog,
          absl::nullopt,
          "Retrieve .xlog output file from Contam server");
ABSL_FLAG(bool, ctm_verbose, false, "Verbosely parse the Contam params");
ABSL_FLAG(std::string,
          ctm_proto,
          "",
          "Filename of the prototext for Contam params");

using contam::rpc::ContamReply;
using contam::rpc::ContamRequest;
using contam::rpc::ContamServer;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace Contam_gRPC {

#if defined(BUILD_DEBUG)
#define comment_on_overriden_params(verbose, _param_, value)                   \
  do {                                                                         \
    if (verbose) {                                                             \
      using namespace std;                                                     \
      cerr << "Overriding prototext param by cmd arg: of_" #_param_ " = "      \
           << (value) << endl;                                                 \
    }                                                                          \
  } while (0)
#else
#define comment_on_overriden_params(verbose, _param_, value)
#endif

#define SET_CLIENT_PARAM(client_params, verbose, _param_)                      \
  do {                                                                         \
    absl::optional<bool> opt = absl::GetFlag(FLAGS_ctm_##_param_);             \
    if (opt.has_value()) {                                                     \
      (client_params).of_##_param_ = opt.value();                              \
      comment_on_overriden_params(                                             \
        verbose, _param_, string(opt.value() ? "true" : "false"));             \
    }                                                                          \
  } while (0)

struct ContamState {
  std::string sim_id;
  unsigned int sim_step;
  int rank;
  ContamParams contam_params;
  ClientParams client_params;
  std::string of_prefix;
  bool dryrun;
  std::unique_ptr<ContamServer::Stub> stub_;

  ContamState();
  void Clear();
  bool InitState(int r);
  std::string to_string() const;
};

ContamState::ContamState()
  : sim_step(0u), rank(-1), dryrun(false), stub_(nullptr)
{}

void ContamState::Clear()
{
  sim_id.clear();
  sim_step = 0u;
  rank = -1;
  // contam_params.clear();
  client_params.clear();
  dryrun = false;
}

bool ContamState::InitState(int r)
{
  sim_id = getUniqStr(rank = r);
  sim_step = 0u;

  using std::cerr;
  using std::endl;
  using std::string;

  auto const ctm_proto = absl::GetFlag(FLAGS_ctm_proto);
  bool const ctm_verbose = absl::GetFlag(FLAGS_ctm_verbose);
  if (!ctm_proto.empty()) {
    std::tie(contam_params, client_params) =
      read_proto_params(ctm_proto, ctm_verbose);
  }

  /* This comes after ctm_proto handling as prototext configuration
     file may or may not specify prj_filename. The command line option
     ctm_prj_file overrides the prototext-based configuration. */

  auto const prj_filename_tmp = absl::GetFlag(FLAGS_ctm_prj_file);
  if (!prj_filename_tmp.empty()) {
    contam_params.prj_filename = prj_filename_tmp;
    if (ctm_verbose) {
      cerr << "  Contam project file: " + contam_params.prj_filename << endl;
    }
  } else if (contam_params.prj_filename.empty()) {
    cerr << "Contam project file is not specified" << endl;
    return false;
  }

  dryrun = absl::GetFlag(FLAGS_ctm_dryrun);

  SET_CLIENT_PARAM(client_params, ctm_verbose, out);
  SET_CLIENT_PARAM(client_params, ctm_verbose, err);
  SET_CLIENT_PARAM(client_params, ctm_verbose, ach);
  SET_CLIENT_PARAM(client_params, ctm_verbose, age);
  SET_CLIENT_PARAM(client_params, ctm_verbose, bal);
  SET_CLIENT_PARAM(client_params, ctm_verbose, cbw);
  SET_CLIENT_PARAM(client_params, ctm_verbose, cex);
  SET_CLIENT_PARAM(client_params, ctm_verbose, csm);
  SET_CLIENT_PARAM(client_params, ctm_verbose, ebw);
  SET_CLIENT_PARAM(client_params, ctm_verbose, log);
  SET_CLIENT_PARAM(client_params, ctm_verbose, rst);
  SET_CLIENT_PARAM(client_params, ctm_verbose, rxr);
  SET_CLIENT_PARAM(client_params, ctm_verbose, rzf);
  SET_CLIENT_PARAM(client_params, ctm_verbose, rzm);
  SET_CLIENT_PARAM(client_params, ctm_verbose, rzi);
  SET_CLIENT_PARAM(client_params, ctm_verbose, sim);
  SET_CLIENT_PARAM(client_params, ctm_verbose, sqlite3);
  SET_CLIENT_PARAM(client_params, ctm_verbose, srf);
  SET_CLIENT_PARAM(client_params, ctm_verbose, val);
  SET_CLIENT_PARAM(client_params, ctm_verbose, xlog);

  auto const of_prefix_tmp = absl::GetFlag(FLAGS_ctm_of_prefix);
  if (!of_prefix_tmp.empty()) {
    client_params.of_prefix = of_prefix_tmp;
    comment_on_overriden_params(ctm_verbose, prefix, client_params.of_prefix);
  }
  auto& prefix = client_params.of_prefix;
  prefix += ((!prefix.empty() && prefix.back() != '/') ? "/" : "");

  std::string target_str = absl::GetFlag(FLAGS_ctm_target);

  // Instantiate the client. RPCs are created out of a channel, that models
  // a connection to an endpoint.
  // The channel here isn't authenticated (thus InsecureChannelCredentials())
  stub_ = ContamServer::NewStub(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  std::cout << "================ Contam setup ================\n";
  std::cout << to_string() << std::endl;
  std::cout << "==============================================\n";

  return true;
}

std::string ContamState::to_string() const
{
  std::string str =
    std::string("{\n") + "  sim_id: " + sim_id + "\n" + "  sim_step: "
    + std::to_string(sim_step) + "\n" + "  rank: " + std::to_string(rank) + "\n"
    + "  client stub: " + (stub_ ? "set" : "not set") + "\n"
    + "  contam_params: \n" + contam_params.to_string(2u) + "\n"
    + "  client_params: \n" + client_params.to_string(2u) + "\n}";
  return str;
}

static thread_local ContamState ctmState;
static void ContaStateInit() __attribute__((constructor));
static void ContaStateFini() __attribute__((destructor));

void ContaStateInit()
{
  ctmState.Clear();
}

void ContaStateFini()
{
  ctmState.Clear();
  google::protobuf::ShutdownProtobufLibrary();
}

void pickContamArgs(int argc, char** argv)
{
  absl::ParseCommandLine(argc, argv);
}

bool initContamClientState(int rank)
{
  return ctmState.InitState(rank);
}

void set_ambience(ContamRequest& request,
                  std::vector<std::vector<AmbienceSample>> const& ambiences)
{
  request.mutable_ambiences()->Reserve(ambiences.size());

  for (auto const& amb : ambiences) {
    auto ambience_ptr = request.add_ambiences();
    ambience_ptr->mutable_amb_series()->Reserve(amb.size());
    for (auto const& as : amb) {
      auto sample_ptr = ambience_ptr->add_amb_series();
      sample_ptr->set_time(as.time);
      sample_ptr->set_temperature(as.temperature);
      sample_ptr->set_pressure(as.pressure);
      sample_ptr->set_wind_speed(as.wind_speed);
      sample_ptr->set_wind_direction(as.wind_direction);
      sample_ptr->mutable_mass_fraction()->Assign(as.mass_fraction.begin(),
                                                  as.mass_fraction.end());
    }
  }
}

void set_envelope(ContamRequest& request,
                  std::vector<std::vector<AmbienceSample>> const& ambiences)
{
  request.mutable_ambiences()->Reserve(ambiences.size());
  for (auto const& amb : ambiences) {
    auto ambience_ptr = request.add_ambiences();
    ambience_ptr->mutable_amb_series()->Reserve(amb.size());
    for (auto const& as : amb) {
      auto sample_ptr = ambience_ptr->add_amb_series();
      sample_ptr->set_time(as.time);
      sample_ptr->set_pressure(as.pressure);
      sample_ptr->mutable_mass_fraction()->Assign(as.mass_fraction.begin(),
                                                  as.mass_fraction.end());
    }
  }
}

/*! \brief Sends the client's request and presents the response received from
 * the server.
 */
ContamResponse runContamClient()
{
  ContamResponse response;

  auto const prj = loadFile(ctmState.contam_params.prj_filename);
  std::cout << "Loaded Contam project file '"
                 + ctmState.contam_params.prj_filename + "' of "
                 + std::to_string(prj.size()) + " bytes\n";

  // Message to send to the server.
  ContamRequest request;
  request.set_sim_id(ctmState.sim_id);
  request.set_prj(prj.data(), prj.size());
  request.set_ach(ctmState.client_params.of_ach);
  request.set_ach(ctmState.client_params.of_age);
  request.set_ach(ctmState.client_params.of_bal);
  request.set_ach(ctmState.client_params.of_cbw);
  request.set_cex(ctmState.client_params.of_cex);
  request.set_csm(ctmState.client_params.of_csm);
  request.set_ach(ctmState.client_params.of_ebw);
  request.set_log(ctmState.client_params.of_log);
  request.set_rst(ctmState.client_params.of_rst);
  request.set_rst(ctmState.client_params.of_rxr);
  request.set_rst(ctmState.client_params.of_rzf);
  request.set_rst(ctmState.client_params.of_rzm);
  request.set_rst(ctmState.client_params.of_rzi);
  request.set_sim(ctmState.client_params.of_sim);
  request.set_sim(ctmState.client_params.of_sqlite3);
  request.set_sim(ctmState.client_params.of_srf);
  request.set_sim(ctmState.client_params.of_val);
  request.set_xlog(ctmState.client_params.of_xlog);
  request.set_wp_wo_envelope(ctmState.contam_params.wp_wo_envelope);

  if (ctmState.contam_params.wp_wo_envelope == 1) {
    set_ambience(request, ctmState.contam_params.ambiences);
  } else if (ctmState.contam_params.wp_wo_envelope == 0) {
    set_envelope(request, ctmState.contam_params.ambiences);
  }

  if (ctmState.dryrun) {
    response.status = true;
    return response;
  }

  // Reponse received from the server
  ContamReply reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  ClientContext context;

  // The actual RPC
  Status status = ctmState.stub_->RunContam(&context, request, &reply);

  auto prefix =
    ctmState.client_params.of_prefix + std::to_string(ctmState.sim_step) + "/";
  mkdir_as_needed(prefix.c_str());

  for (auto const& file : reply.files()) {
    writeFile(prefix + file.filename(), file.data(), std::ios_base::app);
  }

  if (status.ok()) {
    std::cout << "Contam RPC reply status: success" << std::endl;
    response.status = true;
  } else {
    std::cout << "Contam RPC reply status: fail" << std::endl;
    response.status = false;
    response.err_str =
      std::to_string(status.error_code()) + ": " + status.error_message();
  }
  return response;
}

}  // namespace Contam_gRPC
