#include <string>
#include <iostream>
#include <fstream>
#include <tuple>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>
#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "contam_params.h"
#include "contam_params.pb.h"

namespace Contam_gRPC {

void ContamParams::clear()
{
  prj_filename.clear();
  amb_series.clear();
}

std::string ContamParams::to_string() const
{
  std::string str = "{\n  prj_file: " + prj_filename + "\n  amb_series:\n    [\n";
  for (const auto& s: amb_series) {
    str += "      " + s.to_string() + ",\n";
  }
  str += "    ]\n}";

  return str;
}

ClientParams::ClientParams()
{
  clear();
}

void ClientParams::clear()
{
  of_out = false;
  of_err = false;
  of_ach = false;
  of_cex = false;
  of_csm = false;
  of_log = false;
  of_rst = false;
  of_sim = false;
  of_xlog = false;
  of_prefix.clear();
}

std::string ClientParams::to_string() const
{
  using std::string;

  std::string str
    = string("{\n")
    + "  of_out: " + string(of_out ? "true\n" : "false\n")
    + "  of_err: " + string(of_err ? "true\n" : "false\n")
    + "  of_ach: " + string(of_ach ? "true\n" : "false\n")
    + "  of_cex: " + string(of_cex ? "true\n" : "false\n")
    + "  of_csm: " + string(of_csm ? "true\n" : "false\n")
    + "  of_log: " + string(of_log ? "true\n" : "false\n")
    + "  of_rst: " + string(of_rst ? "true\n" : "false\n")
    + "  of_sim: " + string(of_sim ? "true\n" : "false\n")
    + "  of_xlog: " + string(of_xlog? "true\n" : "false\n")
    + "  of_prefix: " + of_prefix + "\n}";
  return str;
}

/// Error handler
class ContamProtoLogSink : public absl::LogSink {
 public:
  void Send(const absl::LogEntry& entry) override {
    std::cerr << "Contam proto: " << entry.text_message_with_prefix_and_newline();
  }
};

/// General prototext reader
template<typename T>
bool read_prototext(const std::string& file_name, const bool is_binary,
                    T& proto_params)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  std::ifstream input(file_name, std::ios::in | std::ios::binary);

  // absl::InitializeLog();
  ContamProtoLogSink contam_sink;
  absl::AddLogSink(&contam_sink);

  if (!input) {
    std::cerr << file_name << ": File not found!" << std::endl;
    return false;
  }
  if (is_binary) {
    if (!proto_params.ParseFromIstream(&input)) {
      std::cerr << "Failed to parse simulation params in binary-formatted input file: "
                << file_name << std::endl;
      return false;
    }
  } else {
    google::protobuf::io::IstreamInputStream istrm(&input);
    if (!google::protobuf::TextFormat::Parse(&istrm, &proto_params)) {
      std::cerr << "Failed to parse simulation params in text-formatted input file: "
                << file_name << std::endl;
      return false;
    }
  }

  absl::RemoveLogSink(&contam_sink);
  return true;
}

static void set_contam_options(const contam::Sim_Params::Contam_Params& cfg,
                               ContamParams& cp, bool verbose = false)
{
  using Contam_Params = contam::Sim_Params::Contam_Params;

  cp.prj_filename = cfg.prj_filename();
  size_t n = cfg.amb_series_size();
  cp.amb_series.reserve(n);

  for(const auto& s: cfg.amb_series()) {
    cp.amb_series.emplace_back(s.time(), s.temperature(), s.pressure(), s.wind_speed(), s.wind_direction(), s.mass_fraction());
  }

  if (verbose) {
    std::cout << cp.to_string() << std::endl;
  }
}

static void set_client_options(const contam::Sim_Params::Client_Params& cfg,
                               ClientParams& cp, bool verbose = false)
{
  using Client_Params = contam::Sim_Params::Client_Params;

  cp.of_out = cfg.of_out();
  cp.of_err = cfg.of_err();
  cp.of_ach = cfg.of_ach();
  cp.of_cex = cfg.of_cex();
  cp.of_csm = cfg.of_csm();
  cp.of_log = cfg.of_log();
  cp.of_rst = cfg.of_rst();
  cp.of_sim = cfg.of_sim();
  cp.of_xlog = cfg.of_xlog();
  cp.of_prefix = cfg.of_prefix();

  if (verbose) {
    std::cout << cp.to_string() << std::endl;
  }
}

std::tuple<ContamParams, ClientParams>
read_proto_params(const std::string& filename, bool verbose)
{
  contam::Sim_Params sim_params;
  read_prototext(filename, false, sim_params);

  if (verbose) {
    std::string str;
    google::protobuf::TextFormat::PrintToString(sim_params, &str);
    std::cout << "---- Prototext '" << filename << "' read ----" << std::endl
              << str << std::endl;
  }

  ContamParams contam_p;
  ClientParams client_p;
  set_contam_options(sim_params.contam_params(), contam_p, verbose);
  set_client_options(sim_params.client_params(), client_p, verbose);

  return {contam_p, client_p};
}

} // end of namespace
