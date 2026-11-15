/*
 * Copyright 2026 Lawrence Livermore National Security, LLC.
 * See the top-level LICENSE file for details.

 * SPDX-License-Identifier: BSD-3
 */

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
  wp_wo_envelope = 0;
  ambiences.clear();
}

std::string ContamParams::to_string(unsigned indent) const
{
  using std::string;
  const string str_indent = string(indent, ' ');
  const string str_indent_inner = string(indent + 2, ' ');
  string str = str_indent + "{\n" + str_indent + "  prj_file: " + prj_filename
               + "\n" + str_indent
               + "  wp_wo_envelope: " + std::to_string(wp_wo_envelope) + "\n"
               + str_indent + "  ambiences:\n" + str_indent_inner + "[\n";
  for (auto const& amb_series : ambiences) {
    str += str_indent_inner + "  {\n" + str_indent_inner + "    amb_series:\n";
    str += Contam_gRPC::to_string(amb_series, 8) + "\n" + str_indent_inner
           + "  },\n";
  }
  str += str_indent_inner + "]\n" + str_indent + "}";

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
  of_age = false;
  of_bal = false;
  of_cbw = false;
  of_cex = false;
  of_csm = false;
  of_ebw = false;
  of_log = false;
  of_rst = false;
  of_rxr = false;
  of_rzf = false;
  of_rzm = false;
  of_rzi = false;
  of_sim = false;
  of_sqlite3 = false;
  of_srf = false;
  of_val = false;
  of_xlog = false;
  of_prefix.clear();
}

#define ADD_CP_PRINT_STR(_PARAM_)                                              \
  +"of_"##_PARAM_ ": " + string(of_ #_PARAM_ ? "true\n" : "false\n")           \
    + str_indent_inner

std::string ClientParams::to_string(unsigned indent) const
{
  using std::string;
  const string str_indent = string(indent, ' ');
  const string str_indent_inner = string(indent + 2, ' ');

  string str =
    str_indent + string("{\n") + str_indent_inner
    + "of_out: " + string(of_out ? "true\n" : "false\n") + str_indent_inner
    + "of_err: " + string(of_err ? "true\n" : "false\n") + str_indent_inner
    + "of_ach: " + string(of_ach ? "true\n" : "false\n") + str_indent_inner
    + "of_age: " + string(of_age ? "true\n" : "false\n") + str_indent_inner
    + "of_bal: " + string(of_bal ? "true\n" : "false\n") + str_indent_inner
    + "of_cbw: " + string(of_cbw ? "true\n" : "false\n") + str_indent_inner
    + "of_cex: " + string(of_cex ? "true\n" : "false\n") + str_indent_inner
    + "of_csm: " + string(of_csm ? "true\n" : "false\n") + str_indent_inner
    + "of_ebw: " + string(of_ebw ? "true\n" : "false\n") + str_indent_inner
    + "of_log: " + string(of_log ? "true\n" : "false\n") + str_indent_inner
    + "of_rst: " + string(of_rst ? "true\n" : "false\n") + str_indent_inner
    + "of_rxr: " + string(of_rxr ? "true\n" : "false\n") + str_indent_inner
    + "of_rzf: " + string(of_rzf ? "true\n" : "false\n") + str_indent_inner
    + "of_rzm: " + string(of_rzm ? "true\n" : "false\n") + str_indent_inner
    + "of_rzi: " + string(of_rzi ? "true\n" : "false\n") + str_indent_inner
    + "of_sim: " + string(of_sim ? "true\n" : "false\n") + str_indent_inner
    + "of_sqlite3: " + string(of_sqlite3 ? "true\n" : "false\n")
    + str_indent_inner + "of_srf: " + string(of_srf ? "true\n" : "false\n")
    + str_indent_inner + "of_val: " + string(of_val ? "true\n" : "false\n")
    + str_indent_inner + "of_xlog: " + string(of_xlog ? "true\n" : "false\n")
    + str_indent_inner + "of_prefix: " + of_prefix + "\n" + str_indent + "}";
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

void set_ambience(
  std::vector<std::vector<AmbienceSample>>& ambiences,
  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const&
    proto_ambiences,
  int ambiences_size)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.reserve(n);

  for (auto const& proto_amb : proto_ambiences) {
    auto& amb_series = ambiences.emplace_back();
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
  }
}

void set_envelope(
  std::vector<std::vector<AmbienceSample>>& ambiences,
  google::protobuf::RepeatedPtrField<contam::ambience::Contam_Ambience> const&
    proto_ambiences,
  int ambiences_size)
{
  size_t n = static_cast<size_t>(ambiences_size);
  ambiences.reserve(n);

  for (auto const& proto_amb : proto_ambiences) {
    auto& amb_series = ambiences.emplace_back();
    amb_series.reserve(proto_amb.amb_series_size());

    for (auto const& s : proto_amb.amb_series()) {
      auto const& mf = s.mass_fraction();
      amb_series.emplace_back(s.time(),
                              s.pressure(),
                              std::vector<CTM_R8> {s.mass_fraction().begin(),
                                                   s.mass_fraction().end()});
    }
  }
}

static void set_contam_options(Contam_Params const& cfg,
                               ContamParams& cp,
                               bool verbose = false)
{
  cp.prj_filename = cfg.prj_filename();
  cp.wp_wo_envelope = cfg.wp_wo_envelope();

  if (cp.wp_wo_envelope == 1) {
    set_ambience(cp.ambiences, cfg.ambiences(), cfg.ambiences_size());
  } else {
    set_envelope(cp.ambiences, cfg.ambiences(), cfg.ambiences_size());
  }

#if defined(BUILD_DEBUG)
  if (verbose) {
    std::cout << "contam_params: " << cp.to_string() << std::endl;
  }
#endif
}

static void set_client_options(Client_Params const& cfg,
                               ClientParams& cp,
                               bool verbose = false)
{
  cp.of_out = cfg.of_out();
  cp.of_err = cfg.of_err();
  cp.of_ach = cfg.of_ach();
  cp.of_age = cfg.of_age();
  cp.of_bal = cfg.of_bal();
  cp.of_cbw = cfg.of_cbw();
  cp.of_cex = cfg.of_cex();
  cp.of_csm = cfg.of_csm();
  cp.of_ebw = cfg.of_ebw();
  cp.of_log = cfg.of_log();
  cp.of_rst = cfg.of_rst();
  cp.of_rxr = cfg.of_rxr();
  cp.of_rzf = cfg.of_rzf();
  cp.of_rzm = cfg.of_rzm();
  cp.of_rzi = cfg.of_rzi();
  cp.of_sim = cfg.of_sim();
  cp.of_sqlite3 = cfg.of_sqlite3();
  cp.of_srf = cfg.of_srf();
  cp.of_val = cfg.of_val();
  cp.of_xlog = cfg.of_xlog();
  cp.of_prefix = cfg.of_prefix();

#if defined(BUILD_DEBUG)
  if (verbose) {
    std::cout << "client_params: " << cp.to_string() << std::endl;
  }
#endif
}

std::tuple<ContamParams, ClientParams>
read_proto_params(std::string const& filename, bool verbose)
{
  Sim_Params sim_params;
  read_prototext(filename, false, sim_params);

#if defined(BUILD_DEBUG)
  if (verbose) {
    using namespace std;
    string str;
    google::protobuf::TextFormat::PrintToString(sim_params, &str);
    cout << "google::protobuf::TextFormat::PrintToString():" << endl
         << str << endl
         << endl;
    cout << endl << "---- Prototext '" << filename << "' read ----" << endl;
  }
#endif

  ContamParams contam_p;
  ClientParams client_p;
  set_contam_options(sim_params.contam_params(), contam_p, verbose);
  set_client_options(sim_params.client_params(), client_p, verbose);

#if defined(BUILD_DEBUG)
  if (verbose) {
    std::cout << "---- end of the '" + filename + "' ----" << std::endl;
  }
#endif

  return {contam_p, client_p};
}

}  // namespace Contam_gRPC
