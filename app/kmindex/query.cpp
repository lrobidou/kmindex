
#include "query.hpp"

#include <iostream>
#include <kmindex/query/query.hpp>
#include <kmindex/index/index.hpp>
#include <kmindex/query/format.hpp>

#include <kmindex/threadpool.hpp>
#include <kseq++/seqio.hpp>

#include <fmt/format.h>
#include <spdlog/spdlog.h>


namespace kmq {

  kmq_options_t kmq_query_cli(parser_t parser, kmq_query_options_t options)
  {
    auto cmd = parser->add_command("query", "Query index.");

    cmd->add_param("-i/--index", "Global index path.")
       ->meta("STR")
       ->checker(bc::check::is_dir)
       ->setter(options->global_index_path);

    auto name_setter = [options](const std::string& v) {
      options->index_names = bc::utils::split(v, ',');
    };

    cmd->add_param("-n/--names", "Sub-indexes to query, comma separated.")
       ->meta("STR")
       ->setter_c(name_setter);

    cmd->add_param("-z/--zvalue", "Index s-mers and query (s+z)-mers (findere algorithm).")
       ->meta("INT")
       ->def("0")
       ->checker(bc::check::f::range(0, 8))
       ->setter(options->z);

    cmd->add_param("-r/--threshold", "Shared k-mers threshold (currently unused).")
       ->def("0")
       ->checker(bc::check::f::range(0.0, 1.0))
       ->setter(options->sk_threshold);

    cmd->add_param("-o/--output", "Output directory.")
       ->meta("STR")
       ->def("output")
       ->setter(options->output);

    cmd->add_param("-q/--fastx", "Input fasta/q file (supports gz/bzip2) containing the sequence(s) to query.")
       ->meta("STR")
       ->checker(bc::check::f::ext(
         "fa|fq|fasta|fastq|fna|fa.gz|fq.gz|fasta.gz|fastq.gz|fna.gz|fa.bz2|fq.bz2|fasta.bz2|fastq.bz2|fna.bz2"))
       ->setter(options->input);

    cmd->add_param("-s/--single-query", "Query identifier. All sequences are considered as a unique query.")
      ->def("")
      ->meta("STR")
      ->setter(options->single);


    auto format_setter = [options](const std::string& v) {
      options->format = str_to_format(v);
    };

    cmd->add_param("-f/--format", "Output format [json|matrix]")
       ->meta("STR")
       ->def("json")
       ->checker(bc::check::f::in("json|matrix"))
       ->setter_c(format_setter);

    add_common_options(cmd, options, true);

    return options;
  }

  void main_query(kmq_options_t opt)
  {
    kmq_query_options_t o = std::static_pointer_cast<struct kmq_query_options>(opt);

    Timer gtime;

    index global(o->global_index_path);

    spdlog::info("Global index: {}", o->global_index_path);

    std::shared_ptr<query_formatter_base> f {nullptr};
    for (auto& index_name : o->index_names)
    {
      Timer timer;
      auto infos = global.get(index_name);
      f = get_formatter(o->format, infos.bw());

      spdlog::info("Query '{}' ({} samples)", infos.name(), infos.nb_samples());
      query_result_agg agg;

      klibpp::KSeq record;
      klibpp::SeqStreamIn iss(o->input.c_str());
      kindex ki(infos);

      ThreadPool pool(o->nb_threads);

      while (iss >> record)
      {
        pool.add_task([record=record, &ki, &agg, &o, &infos](int i){
          unused(i);
          query q(record.name, record.seq, infos.smer_size(), o->z, infos.nb_samples(), 0.0, infos.bw());
          agg.add(ki.resolve(q));
        });
      }

      pool.join_all();

      if (o->single.size() > 0)
      {
        write_result(f->merge_format(infos.name(), infos.samples(), agg, o->single),
                     infos.name(),
                     o->output,
                     o->format);
      }
      else
      {
        write_result(f->format(infos.name(), infos.samples(), agg),
                     infos.name(),
                     o->output,
                     o->format);
      }

      spdlog::info("Results dumped at {}/{}.{} ({})",
                 o->output,
                 infos.name(),
                 o->format == format::json ? "json" : "tsv",
                 timer.formatted());
    }

    spdlog::info("Done ({}).", gtime.formatted());
  }
}
