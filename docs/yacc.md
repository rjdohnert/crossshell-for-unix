# yacc

## What it does

`yacc` is a standalone LALR(1) parser generator. It reads a Yacc-style grammar,
constructs parser states and action/goto tables, and writes a generated C or C++
parser. It does not require or invoke an external Bison or Yacc executable.

The grammar may contain a `%{ ... %}` or `%code { ... }` prologue, `%union`,
`%token`, `%type`, `%left`, `%right`, `%nonassoc`, and `%start` declarations,
rules between `%%` markers, semantic actions using `$$` and `$n`, and trailing
epilogue code.

## Usage

```text
yacc [options] <grammar_file.y>
```

With no arguments, `yacc` prints help and exits successfully. A grammar filename
is required for generation.

## Options

- `-d`, `--defines`: generate a token header.
- `--defines=FILE`: generate the token header at `FILE`.
- `-o FILE`, `--output=FILE`: write the generated parser to `FILE`.
- `-p PREFIX`, `--prefix=PREFIX`: replace the generated `yy` symbol prefix.
- `-b PREFIX`, `--file-prefix=PREFIX`: accept a file-prefix setting for Yacc
	compatibility. The current implementation does not apply it to output names.
- `-v`, `--verbose`: write a parser-state report.
- `-y`, `--yacc`: use POSIX-style `y.tab.c` and `y.tab.h` output names and
	generate the header.
- `-t`, `--debug`: initialize generated parser tracing as enabled.
- `-l`, `--no-lines`: suppress generated `#line` directives. The current
	generator does not emit these directives.
- `--json`, `--csv`, `--table`: format status messages for machine-readable or
	tabular output.
- `--pipe COMMAND`: send standard status output through `COMMAND`.
- `-V`, `--version`: print version and license information.
- `-h`, `--help`, `/?`: print help.

## Output files

For an input named `grammar.y`, the default parser output is `grammar.tab.c`.
`-d` adds `grammar.tab.h`, and `-v` adds `grammar.output`. In `-y` mode, the
parser and header are named `y.tab.c` and `y.tab.h`; the verbose report retains
the grammar-based name.

An explicit `-o` changes only the parser source path. Use `--defines=FILE` to
choose the header path. Existing output files are replaced without prompting.

## Conflict handling

Precedence and associativity declarations resolve eligible shift/reduce
conflicts. Otherwise, shift/reduce conflicts prefer shifting, while
reduce/reduce conflicts retain the earlier production. Conflict totals are
reported as warnings and generation still succeeds.

## Examples

Generate a parser and token header:

```text
yacc -d grammar.y
```

Generate POSIX-style output and a state report:

```text
yacc -y -v grammar.y
```

Choose the parser filename and generated symbol prefix:

```text
yacc -d -p calc_ -o calc_parser.c calc.y
```

The generated parser expects the corresponding `<prefix>lex` and
`<prefix>error` functions to be supplied by the caller.

## Exit status

- `0`: help/version was displayed, or generation completed successfully.
- `1`: the input was missing or invalid, an output file could not be generated,
	or an unexpected runtime failure occurred.

Warnings about parser conflicts do not change a successful exit status.

## UNIX origin

The interface follows the traditional Yacc parser-generator workflow and adds
selected GNU Bison-compatible options and grammar declarations.
