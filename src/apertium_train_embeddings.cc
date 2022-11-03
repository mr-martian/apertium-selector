#include "embedding_trainer.h"
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>

int main(int argc, char** argv)
{
  CLI cli("Train apertium-selector word embeddings");
  cli.add_bool_arg('h', "help", "print this help and exit");
  cli.add_file_arg("raw_corpus", true);
  cli.add_file_arg("output_weights", true);
  cli.parse_args(argc, argv);

  EmbeddingTrainer et;

  InputFile input;
  if (!cli.get_files()[0].empty()) {
    input.open_or_exit(cli.get_files()[0].c_str());
  }
  UFILE* output = openOutTextFile(cli.get_files()[1]);

  et.read_corpus(input);
  et.train();
  et.write(output);

  u_fclose(output);
  return 0;
}
