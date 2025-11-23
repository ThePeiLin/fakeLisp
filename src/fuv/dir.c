#include "fuv.h"

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_dir_print, "dir");

static int fuv_dir_finalize(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(dir, FuvDir, ud);
    FuvValueDir *dir = FUV_DIR(ud);
    if (dir->req == NULL) {
        cleanUpDir(dir->dir, FUV_DIR_CLEANUP_ALL);
        dir->dir = NULL;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const FuvDirUdMetaTable = {
    // .size = sizeof(FuvDir),
    .size = sizeof(FuvValueDir),
    .prin1 = fuv_dir_print,
    .princ = fuv_dir_print,
    .finalize = fuv_dir_finalize,
};

int isFuvDir(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &FuvDirUdMetaTable;
}

FklVMvalue *
createFuvDir(FklVM *vm, FklVMvalue *dll, uv_fs_t *req, size_t nentries) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvDirUdMetaTable, dll);
    // FKL_DECL_VM_UD_DATA(dir_ud, FuvDir, v);
    FuvValueDir *dir_ud = FUV_DIR(v);
    dir_ud->dir = req->ptr;
    req->ptr = NULL;
    uv_dir_t *dir = dir_ud->dir;
    dir->nentries = nentries;
    if (nentries) {
        dir->dirents = (uv_dirent_t *)fklZcalloc(nentries, sizeof(uv_dirent_t));
        FKL_ASSERT(dir->dirents);
    } else
        dir->dirents = NULL;
    return v;
}

// static inline FuvDir *get_fuv_dir(const FklVMvalue *v) {
//     FKL_ASSERT(isFuvDir(v));
//     FKL_DECL_VM_UD_DATA(dir, FuvDir, v);
//     return dir;
// }

int isFuvDirUsing(FklVMvalue *dir) { return FUV_DIR(dir)->req != NULL; }

FklVMvalue *refFuvDir(FklVMvalue *dir_obj, FklVMvalue *req_obj) {
    FUV_DIR(dir_obj)->req = req_obj;
    return dir_obj;
}
