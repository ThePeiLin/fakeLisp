#ifndef FKL_CB_HELPER_H
#define FKL_CB_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#define CB_LINE(...) fklCodeBuilderLine(build, __VA_ARGS__)
#define CB_FMT(...) fklCodeBuilderFmt(build, __VA_ARGS__)

#define CB_LINE_START(...) fklCodeBuilderLineStart(build, __VA_ARGS__)
#define CB_LINE_END(...) fklCodeBuilderLineEnd(build, __VA_ARGS__)

#define CB_INDENT(flag)                                                        \
    for (uint8_t flag = (fklCodeBuilderIndent(build), 0); flag < 1;            \
            fklCodeBuilderUnindent(build), ++flag)

#ifdef __cplusplus
}
#endif

#endif
