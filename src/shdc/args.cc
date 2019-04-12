/*
    parse command line arguments
*/
#include <vector>
#include <stdio.h>
#include "fmt/format.h"
#include "types.h"
#include "getopt/getopt.h"
#include "pystring.h"

namespace shdc {

static const getopt_option_t option_list[] = {
    { "help", 'h',  GETOPT_OPTION_TYPE_NO_ARG, 0, 'h', "print this help text", 0},
    { "input", 'i', GETOPT_OPTION_TYPE_REQUIRED, 0, 'i', "input source file", "GLSL file" },
    { "output", 'o', GETOPT_OPTION_TYPE_REQUIRED, 0, 'o', "output source file", "C header" },
    { "slang", 'l', GETOPT_OPTION_TYPE_REQUIRED, 0, 'l', "output shader language(s)", "glsl330:glsl100:glsl300es:hlsl5:metal_macos:metal_ios" },
    { "bytecode", 'b', GETOPT_OPTION_TYPE_NO_ARG, 0, 'b', "output bytecode (HLSL and Metal)"},
    { "dump", 'd', GETOPT_OPTION_TYPE_NO_ARG, 0, 'd', "dump debugging information to stderr"},
    GETOPT_OPTIONS_END
};

static void print_help_string(getopt_context_t& ctx) {
    fmt::print(stderr, 
        "Shader compiler / code generator for sokol_gfx.h based on GLslang + SPIRV-Cross\n"
        "https://github.com/floooh/sokol-tools\n\n"
        "Usage: sokol-shdc -i input [-o output] [options]\n\n"
        "Where [input] is exactly one .glsl file, and [output] is a C header\n"
        "with embedded shader source code and/or byte code, and code-generated\n"
        "uniform-block and shader-descripton C structs ready for use with sokol_gfx.h\n\n"
        "The input source file may contains custom '@-tags' to group the\n"
        "source code for several shaders and reusable code blocks into one file:\n\n"
        "  - @block name: a general reusable code block\n"
        "  - @vs name: a named vertex shader code block\n"
        "  - @fs name: a named fragment shader code block\n"
        "  - @end: ends a @vs, @fs or @block code block\n"
        "  - @include block_name: include a code block in a @vs or @fs block\n"
        "  - @program name vs_name fs_name: a named, linked shader program\n\n"
        "An input file must contain at least one @vs block, one @fs block\n"
        "and one @program declaration.\n\n"
        "Options:\n\n");
    char buf[2048];
    fmt::print(stderr, getopt_create_help_string(&ctx, buf, sizeof(buf)));
}

/* parse string of format 'hlsl|glsl100|...' args.slang bitmask */
static bool parse_slang(args_t& args, const char* str) {
    args.slang = 0;
    std::vector<std::string> splits;
    pystring::split(str, splits, ":");
    for (const auto& item : splits) {
        bool item_valid = false;
        for (int i = 0; i < slang_t::NUM; i++) {
            if (item == slang_t::to_str((slang_t::type_t)i)) {
                args.slang |= slang_t::bit((slang_t::type_t)i);
                item_valid = true;
                break;
            }
        }
        if (!item_valid) {
            fmt::print(stderr, "error: unknown shader language '{}'\n", item);
            args.valid = false;
            args.exit_code = 10;
            return false;
        }
    }
    return true;
}

static void validate(args_t& args) {
    bool err = false;
    if (args.input.empty()) {
        fmt::print(stderr, "error: no input file (--input [path])\n");
        err = true;
    }
    if (args.output.empty()) {
        fmt::print(stderr, "error: no output file (--output [path])\n");
        err = true;
    }
    if (args.slang == 0) {
        fmt::print(stderr, "error: no shader languages (--slang ...)\n");
        err = true;
    }
    if (err) {
        args.valid = false;
        args.exit_code = 10;
    }
    else {
        args.valid = true;
        args.exit_code = 0;
    }
}

args_t args_t::parse(int argc, const char** argv) {
    args_t args;
    getopt_context_t ctx;
    if (getopt_create_context(&ctx, argc, argv, option_list) < 0) {
        fmt::print(stderr, "error in getopt_create_context()\n");
    }
    else {
        int opt = 0;
        while ((opt = getopt_next(&ctx)) != -1) {
            switch (opt) {
                case '+':
                    fmt::print(stderr, "got argument without flag: {}\n", ctx.current_opt_arg);
                    break;
                case '?':
                    fmt::print(stderr, "unknown flag {}\n", ctx.current_opt_arg);
                    break;
                case '!':
                    fmt::print(stderr, "invalid use of flag {}\n", ctx.current_opt_arg);
                    break;
                case 'i':
                    args.input = ctx.current_opt_arg;
                    break;
                case 'o':
                    args.output = ctx.current_opt_arg;
                    break;
                case 'b':
                    args.byte_code = true;
                    break;
                case 'd':
                    args.debug_dump = true;
                    break;
                case 'l':
                    if (!parse_slang(args, ctx.current_opt_arg)) {
                        /* error details have been filled by parse_slang() */
                        return args;
                    }
                    break;
                case 'h':
                    print_help_string(ctx);
                    args.valid = false;
                    args.exit_code = 0;
                    return args;
                default:
                    break;
            }
        }
    }
    validate(args);
    return args;
}

void args_t::dump() {
    fmt::print(stderr, "args_t:\n");
    fmt::print(stderr, "  valid: {}\n", valid);
    fmt::print(stderr, "  exit_code: {}\n", exit_code);
    fmt::print(stderr, "  input:  '{}'\n", input);
    fmt::print(stderr, "  output: '{}'\n", output);
    fmt::print(stderr, "  slang:  '{}'\n", slang_t::bits_to_str(slang));
    fmt::print(stderr, "  byte_code: {}\n", byte_code);
    fmt::print(stderr, "  debug_dump: {}\n", debug_dump);
}

} // namespace shdc