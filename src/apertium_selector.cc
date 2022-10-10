#include "selector.h"
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>

int main(int argc, char** argv)
{
  CLI cli("Disambiguate Apertium stream format");
  cli.add_bool_arg('z', "null-flush", "flush stream on reading \\0");
  cli.add_bool_arg('h', "help", "print this help and exit");
  cli.add_file_arg("binfile");
  cli.add_file_arg("input", true);
  cli.add_file_arg("output", true);
  cli.parse_args(argc, argv);

  Selector sel;

  FILE* bin = openInBinFile(cli.get_files()[0]);
  sel.load(bin);
  fclose(bin);

  //sel.dump();

  InputFile input;
  if (!cli.get_files()[1].empty()) {
    input.open_or_exit(cli.get_files()[1].c_str());
  }
  UFILE* output = openOutTextFile(cli.get_files()[2]);

  sel.process(input, output);

  u_fclose(output);
  return 0;
}
