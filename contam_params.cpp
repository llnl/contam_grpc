#include "contam_params.h"

#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "contam_params.pb.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>

namespace Contam_gRPC {

void ContamParams::clear()
{
  prj_filename.clear();
  wp_wo_envelop = 0;
  ambiences.clear();
}

std::string ContamParams::to_string() const
{
  std::string str =
    "{\n  prj_file: " + prj_filename +
    "\n  wp_wo_envelop: " +
    std::to_string(wp_wo_envelop) +
    "\n  ambiences:\n    [\n";
  for (auto const& amb_series : ambiences) {
    str += "      { amb_series: [ ";
    for (auto const& s : amb_series) {
      str += s.to_string() + " ] } ,\n";
    }
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

  std::string str = string("{\n")
                    + "  of_out: " + string(of_out ? "true\n" : "false\n")
                    + "  of_err: " + string(of_err ? "true\n" : "false\n")
                    + "  of_ach: " + string(of_ach ? "true\n" : "false\n")
                    + "  of_cex: " + string(of_cex ? "true\n" : "false\n")
                    + "  of_csm: " + string(of_csm ? "true\n" : "false\n")
                    + "  of_log: " + string(of_log ? "true\n" : "false\n")
                    + "  of_rst: " + string(of_rst ? "true\n" : "false\n")
                    + "  of_sim: " + string(of_sim ? "true\n" : "false\n")
                    + "  of_xlog: " + string(of_xlog ? "true\n" : "false\n")
                    + "  of_prefix: " + of_prefix + "\n}";
  return str;
}

/// Error handler
class ContamProtoLogSink : public absl::LogSink {
public:
  void Send(absl::LogEntry const& entry) override
  {
    std::cerr << "Contam proto: "
              << entry.text_message_with_prefix_and_newline();
  }
};

/// General prototext reader
template <typename T>
bool read_prototext(std::string const& file_name,
                    bool const is_binary,
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
      std::cerr
        << "Failed to parse simulation params in binary-formatted input file: "
        << file_name << std::endl;
      return false;
    }
  } else {
    google::protobuf::io::IstreamInputStream istrm(&input);
    if (!google::protobuf::TextFormat::Parse(&istrm, &proto_params)) {
      std::cerr
        << "Failed to parse simulation params in text-formatted input file: "
        << file_name << std::endl;
      return false;
    }
  }

  absl::RemoveLogSink(&contam_sink);
  return true;
}

using Sim_Params = contam::params::Sim_Params;
using Contam_Params = contam::params::Sim_Params::Contam_Params;
using Client_Params = contam::params::Sim_Params::Client_Params;

void set_ambience(std::vector<std::vector<AmbienceSample>>& ambiences,
                  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const& proto_ambiences,
                  int ambiences_size)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.reserve(n);

  for (const auto& proto_amb : proto_ambiences) {
    auto& amb_series = ambiences.emplace_back();
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
  }
}

static void set_contam_options(Contam_Params const& cfg,
                               ContamParams& cp,
                               bool verbose = false)
{
  cp.prj_filename = cfg.prj_filename();
  cp.wp_wo_envelop = cfg.wp_wo_envelop();

  set_ambience(cp.ambiences, cfg.ambiences(), cfg.ambiences_size());

  if (verbose) {
    std::cout << cp.to_string() << std::endl;
  }
}

static void set_client_options(Client_Params const& cfg,
                               ClientParams& cp,
                               bool verbose = false)
{
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
read_proto_params(std::string const& filename, bool verbose)
{
  Sim_Params sim_params;
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

}  // namespace Contam_gRPC
