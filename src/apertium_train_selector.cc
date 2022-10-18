#include "train.h"
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>

int main(int argc, char** argv)
{
  CLI cli("Train apertium-selector weights");
  cli.add_bool_arg('h', "help", "print this help and exit");
  cli.add_file_arg("raw_corpus", false);
  cli.add_file_arg("gold_corpus", false);
  cli.add_file_arg("input_weights", true);
  cli.add_file_arg("output_weights", true);
  cli.parse_args(argc, argv);

  SelectorTrainer st;

  InputFile raw, gold, input;
  if (!cli.get_files()[0].empty()) {
    raw.open_or_exit(cli.get_files()[0].c_str());
  }
  if (!cli.get_files()[1].empty()) {
    gold.open_or_exit(cli.get_files()[1].c_str());
  }
  if (!cli.get_files()[2].empty()) {
    input.open_or_exit(cli.get_files()[2].c_str());
  }
  UFILE* output = openOutTextFile(cli.get_files()[3]);

  st.read(input);
  st.train(raw, gold, 5);
  st.write(output);

  u_fclose(output);
  return 0;
}
