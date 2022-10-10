#include "feature_set.h"
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>

int main(int argc, char** argv)
{
  CLI cli("Compile apertium-selector-weights");
  cli.add_bool_arg('h', "help", "print this help and exit");
  cli.add_file_arg("input", true);
  cli.add_file_arg("output", true);
  cli.parse_args(argc, argv);

  FeatureSet fs;

  InputFile input;
  if (!cli.get_files()[0].empty()) {
    input.open_or_exit(cli.get_files()[0].c_str());
  }
  FILE* output = openOutBinFile(cli.get_files()[1]);

  fs.read(input);
  fs.compile(output);

  fclose(output);
  return 0;
}
