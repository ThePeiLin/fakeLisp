#include "fuv.h"

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_dir_as_print, "dir");

static int fuv_dir_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(dir, FuvDir, ud);
    FuvValueDir *dir = FUV_DIR(ud);
    if (dir->d.req == NULL) {
        cleanUpDir(dir->d.dir, FUV_DIR_CLEANUP_ALL);
        dir->d.dir = NULL;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const FuvDirUdMetaTable = {
    // .size = sizeof(FuvDir),
    .size = sizeof(FuvValueDir),
    .__as_prin1 = fuv_dir_as_print,
    .__as_princ = fuv_dir_as_print,
    .__finalizer = fuv_dir_finalizer,
};

int isFuvDir(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &FuvDirUdMetaTable;
}

FklVMvalue *
createFuvDir(FklVM *vm, FklVMvalue *dll, uv_fs_t *req, size_t nentries) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvDirUdMetaTable, dll);
    // FKL_DECL_VM_UD_DATA(dir_ud, FuvDir, v);
    FuvValueDir *dir_ud = FUV_DIR(v);
    dir_ud->d.dir = req->ptr;
    req->ptr = NULL;
    uv_dir_t *dir = dir_ud->d.dir;
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

int isFuvDirUsing(FklVMvalue *dir) { return FUV_DIR(dir)->d.req != NULL; }

FklVMvalue *refFuvDir(FklVMvalue *dir_obj, FklVMvalue *req_obj) {
    FUV_DIR(dir_obj)->d.req = req_obj;
    return dir_obj;
}
