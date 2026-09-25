#ifndef FKL_CB_HELPER_H
#define FKL_CB_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SB_LINE(...) fklStrBuilderLine(build, __VA_ARGS__)
#define SB_FMT(...) fklStrBuilderFmt(build, __VA_ARGS__)

#define SB_LINE_START(...) fklStrBuilderLineStart(build, __VA_ARGS__)
#define SB_LINE_END(...) fklStrBuilderLineEnd(build, __VA_ARGS__)

#define SB_INDENT(flag)                                                        \
    for (uint8_t flag = (fklStrBuilderIndent(build), 0); flag < 1;             \
            fklStrBuilderUnindent(build), ++flag)

#ifdef __cplusplus
}
#endif

#endif
